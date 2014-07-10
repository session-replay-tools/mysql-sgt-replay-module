#include <xcopy.h>
#include <tcpcopy.h>

#define COM_STMT_PREPARE 22
#define COM_STMT_EXECUTE 23
#define COM_QUERY 3
#define SEC_AUTH_PACKET_NUM 3
#define MAX_SP_SIZE 256

typedef struct {
    uint32_t req_begin:1;
    uint32_t seq_diff;
} tc_mysql_session;


typedef struct {
    link_list *list;
    int tot_cont_len;
} mysql_table_item_t;


typedef struct {
    tc_pool_t  *pool;
    hash_table *table;
    tc_iph_t   *fir_auth_pack;
    int         fir_auth_cont_len;
} tc_mysql_ctx_t;

/* TODO allocate it on heap */
static tc_mysql_ctx_t ctx;

static int 
init_mysql_module(void *clt_settings)
{

    ctx.pool = tc_create_pool(TC_DEFAULT_POOL_SIZE, 0, 0);

    if (ctx.pool) {

        ctx.table = hash_create(ctx.pool, 65536);

        if (ctx.table) {
            return TC_OK;
        }
    } 

    return TC_ERR;
}


static void 
exit_mysql_module(void *clt_settings) 
{
    if (ctx.pool != NULL) {
        tc_destroy_pool(ctx.pool);
        ctx.table = NULL;
        ctx.pool = NULL;
    }
}


static bool
check_renew_session(tc_iph_t *ip, tc_tcph_t *tcp)
{
    uint16_t        size_ip, size_tcp, tot_len, cont_len;
    unsigned char  *payload, command, pack_number;

    if (ctx.fir_auth_pack == NULL) {
        tc_log_debug0(LOG_DEBUG, 0, "fir auth packet is null");
        return false;
    }

    size_ip  = ip->ihl << 2;
    size_tcp = tcp->doff << 2;
    tot_len  = ntohs(ip->tot_len);
    cont_len = tot_len - size_tcp - size_ip;

    if (cont_len > 0) {
        payload = (unsigned char *) ((char *) tcp + size_tcp);
        /* skip packet length */
        payload = payload + 3;
        /* retrieve packet number */
        pack_number = payload[0];
        /* if it is the second authenticate_user, skip it */
        if (pack_number != 0) {
            return false;
        }
        /* skip packet number */
        payload = payload + 1;

        command = payload[0];
        tc_log_debug1(LOG_DEBUG, 0, "mysql command:%u", command);
        if (command == COM_QUERY || command == COM_STMT_EXECUTE) {
            return true;
        }
    }

    return false;
}
        

static bool 
check_pack_needed_for_recons(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    uint16_t            size_tcp;
    p_link_node         ln;
    unsigned char      *payload, command, *pkt;
    mysql_table_item_t *item;

    if (s->cur_pack.cont_len > 0) {

        size_tcp = tcp->doff << 2;
        payload = (unsigned char *) ((char *) tcp + size_tcp);
        /* skip packet length */
        payload  = payload + 3;
        /* skip packet number */
        payload  = payload + 1;
        command  = payload[0];

        if (command != COM_STMT_PREPARE) {
            return false;
        }

        item = hash_find(ctx.table, s->hash_key);
        if (!item) {
            item = tc_pcalloc(ctx.pool, sizeof(mysql_table_item_t));
            if (item != NULL) {
                item->list = link_list_create(ctx.pool);
                if (item->list != NULL) {
                    hash_add(ctx.table, ctx.pool, s->hash_key, item);
                } else {
                    tc_log_info(LOG_ERR, 0, "list create err");
                    return false;
                }
            } else {
                tc_log_info(LOG_ERR, 0, "mysql item create err");
                return false;
            }
        }

        if (item->list->size > MAX_SP_SIZE) {
            return false;
        }

        tc_log_debug1(LOG_INFO, 0, "push packet:%u", ntohs(s->src_port));

        pkt = (unsigned char *) cp_fr_ip_pack(ctx.pool, ip);
        ln  = link_node_malloc(ctx.pool, pkt);
        ln->key = ntohl(tcp->seq);
        link_list_append_by_order(item->list, ln);
        item->tot_cont_len += s->cur_pack.cont_len;

        return true;
    }

    return false;
}


static int 
prepare_for_renew_session(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    uint16_t            size_ip;
    uint32_t            tot_clen, base_seq;
    tc_iph_t           *fir_ip, *t_ip;
    tc_tcph_t          *fir_tcp, *t_tcp;
    p_link_node         ln;
    unsigned char      *p;
    mysql_table_item_t *item;

    if (!ctx.fir_auth_pack) {
        tc_log_info(LOG_WARN, 0, "no first auth pack here");
        return TC_ERR;
    }

    s->sm.need_rep_greet = 1;

    fir_ip        = ctx.fir_auth_pack;
    fir_ip->saddr = ip->saddr;
    size_ip       = fir_ip->ihl << 2;
    fir_tcp = (tc_tcph_t *) ((char *) fir_ip + size_ip);
    fir_tcp->source = tcp->source;

    tot_clen = ctx.fir_auth_cont_len;

    item = hash_find(ctx.table, s->hash_key);
    if (item) {
        tot_clen += item->tot_cont_len;
    }

    tc_log_debug2(LOG_INFO, 0, "total len subtracted:%u,p:%u", tot_clen, 
            ntohs(s->src_port));

    tcp->seq     = htonl(ntohl(tcp->seq) - tot_clen);
    fir_tcp->seq = htonl(ntohl(tcp->seq) + 1);
    tc_save_pack(s, s->slide_win_packs, fir_ip, fir_tcp);  

    base_seq = ntohl(fir_tcp->seq) + ctx.fir_auth_cont_len;

    if (item) {
        ln = link_list_first(item->list); 
        while (ln) {
            p = (unsigned char *) ln->data;
            t_ip  = (tc_iph_t *) (p + ETHERNET_HDR_LEN);
            t_tcp = (tc_tcph_t *) ((char *) t_ip + size_ip);
            t_tcp->seq = htonl(base_seq);
            tc_save_pack(s, s->slide_win_packs, t_ip, t_tcp);  
            base_seq += TCP_PAYLOAD_LENGTH(t_ip, t_tcp);
            ln = link_list_get_next(item->list, ln);
        }
    }

    return TC_OK;
}


static int 
proc_when_sess_created(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    tc_mysql_session *data = (tc_mysql_session *) tc_pcalloc(s->pool, 
            sizeof(tc_mysql_session));

    if (data) {
        s->data = data;
    }

    return TC_OK;
}


static int 
proc_when_sess_destroyed(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    return TC_OK;
}


static int 
proc_auth(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    bool              is_need_omit;
    uint16_t          size_tcp;
    unsigned char    *p, *payload, pack_number;
    tc_mysql_session *mysql_sess;

    if (!s->sm.rcv_rep_greet) {
        return PACK_STOP;
    }

    if (!s->sm.fake_syn) {

        is_need_omit = false;
        mysql_sess = s->data;
        
        if (!mysql_sess->req_begin) {
            size_tcp    = tcp->doff << 2;
            payload = (unsigned char *) ((char *) tcp + size_tcp);
            /* skip packet length */
            payload     = payload + 3; 
            pack_number = payload[0];

            if (pack_number == 0) { 
                mysql_sess->req_begin = 1; 
                tc_log_debug0(LOG_NOTICE, 0, "it has no sec auth packet");

            } else if (pack_number == (unsigned char) SEC_AUTH_PACKET_NUM) { 
                /* if it is the second authenticate_user, skip it */
                is_need_omit = true;
                tc_log_debug0(LOG_NOTICE, 0, "omit sec validation for mysql");
                mysql_sess->req_begin = 1; 
                mysql_sess->seq_diff = s->cur_pack.cont_len;

                return PACK_NEXT;
            }    
        }    

        if (!mysql_sess->req_begin) {
            if (!ctx.fir_auth_pack) {
                p = cp_fr_ip_pack(ctx.pool, ip); 
                ctx.fir_auth_pack = (tc_iph_t *) (p + ETHERNET_HDR_LEN);
                ctx.fir_auth_cont_len = s->cur_pack.cont_len;
                tc_log_info(LOG_NOTICE, 0, "fir auth is set");
            }
        }
    }
    
    return PACK_CONTINUE;
}


static int 
adjust_clt_sequence(tc_sess_t *s, tc_iph_t *ip, tc_tcph_t *tcp)
{
    tc_mysql_session *mysql_sess;

    mysql_sess = s->data;

    if (mysql_sess->seq_diff) {
        tcp->seq = htonl(ntohl(tcp->seq) - mysql_sess->seq_diff);
    }

    return TC_OK;
}


tc_module_t tc_mysql_module = {
    &ctx,
    NULL,
    init_mysql_module,
    exit_mysql_module,
    check_renew_session,
    prepare_for_renew_session,
    check_pack_needed_for_recons,
    proc_when_sess_created,
    proc_when_sess_destroyed,
    NULL,
    proc_auth,
    NULL,
    adjust_clt_sequence
};

