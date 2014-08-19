#[mysql-sgt-replay-module](https://github.com/session-replay-tools/mysql-sgt-replay-module) - A MySQL Replay Tool

mysql-sgt-replay-module is a TCPCopy module that can be used to replay MySQL sessions to support real testing of MySQL applications. 

Please refer to [TCPCopy](https://github.com/session-replay-tools/tcpcopy) for more documents

##Quick start

Two quick start options are available for *mysql-sgt-replay-module*:

* [Download the latest intercept release](https://github.com/session-replay-tools/mysql-sgt-replay-module/releases).
* Clone the repo: `git clone git://github.com/session-replay-tools/mysql-sgt-replay-module.git`.

Two quick start options are available for *intercept*:

* [Download the latest intercept release](https://github.com/session-replay-tools/intercept/releases).
* Clone the repo: `git clone git://github.com/session-replay-tools/intercept.git`.

Two quick start options are available for *tcpcopy*:

* [Download the latest tcpcopy release](https://github.com/session-replay-tools/tcpcopy/releases).
* Clone the repo: `git clone git://github.com/session-replay-tools/tcpcopy.git`.


##Getting intercept installed on the assistant server
1. cd intercept
2. ./configure 
3. make
4. make install


##Getting tcpcopy installed on the online server
1. cd tcpcopy
2. ./configure --set-protocol-module=/path/to/mysql-sgt-replay-module
3. make
4. make install


##Running TCPCopy
 
###1) On the target server which runs MySQL applications:
	    a) Set route command appropriately to route response packets to the assistant server

        For example:

	    Assume 10.110.12.18 is the IP address of the assistant server and 10.110.12.15 is the MySQL client IP address. 
        We set the following route command to route all responses to the 10.110.12.15 to the assistant server.

           route add -host 10.110.12.15 gw 10.110.12.18
        
        b) Add skip-grant-tables to the MySQL configure file(my.conf) and restart MySQL

###2) On the assistant server which runs intercept(root privilege is required):

	   ./intercept -F <filter> -i <device,> 
	  
	  Note that the filter format is the same as the pcap filter.
	  For example:

	  ./intercept -i eth0 -F 'tcp and src port 3306' -d

	  intercept will capture response packets of the TCP based application which listens on port 3306 
	  from device eth0 
    
	
###3) On the online source server (root privilege is required):
      
	  ./tcpcopy -x localServerPort-targetServerIP:targetServerPort -s <intercept server,> 
	  
      For example(assume 10.110.12.17 is the IP address of the target server):

	  ./tcpcopy -x 3306-10.110.12.17:3306 -s 10.110.12.18 

	  tcpcopy would capture MySQL packets(assume MysQL online listens on 3306 port) on current server, 
      send these packets to the target port '3306' on '10.110.12.17'(the target MySQL), 
      and connect 10.110.12.18 for asking intercept to pass response packets to it.

##Release History
+ 2014.09  v1.0    mysql-sgt-replay-module released


##Bugs and feature requests
Have a bug or a feature request? [Please open a new issue](https://github.com/session-replay-tools/mysql-sgt-replay-module/issues). Before opening any issue, please search for existing issues.


## Copyright and license

Copyright 2014 under [the BSD license](LICENSE).


