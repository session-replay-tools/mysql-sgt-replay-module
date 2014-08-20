#A TCPCopy module for MySQL Replay

mysql-sgt-replay-module is a TCPCopy module that can be used to replay MySQL sessions to support real testing of MySQL applications. 

Please refer to [TCPCopy](https://github.com/session-replay-tools/tcpcopy) for more details before reading the following.

##Installation

###Getting intercept installed on the assistant server
1. git clone git://github.com/session-replay-tools/intercept.git
2. cd intercept
3. ./configure 
4. make
5. make install


###Getting tcpcopy installed on the online server
1. git clone git://github.com/session-replay-tools/tcpcopy.git
2. cd tcpcopy
3. git clone git://github.com/session-replay-tools/mysql-sgt-replay-module.git
4. ./configure --set-protocol-module=mysql-sgt-replay-module
5. make
6. make install


##Usage guide
 
###1) On the target server which runs MySQL applications:
	    a) Set route command to route response packets to the assistant server

        For example:

	    Assume 10.110.12.18 is the IP address of the assistant server and 
        10.110.12.15 is the MySQL client IP address. 
        We set the following route command to route all responses to the 
        10.110.12.15 to the assistant server.

           route add -host 10.110.12.15 gw 10.110.12.18
        
        b) Start MySQL with --skip-grant-tables

###2) On the assistant server which runs intercept(root privilege is required):

	   ./intercept -F <filter> -i <device,> 
	  
	  Note that the filter format is the same as the pcap filter.
	  For example:

	  ./intercept -i eth0 -F 'tcp and src port 3306' -d

	  intercept will capture response packets of the TCP based application which 
      listens on port 3306 from device eth0 
    
	
###3) On the online source server (root privilege is required):
      
	  ./tcpcopy -x localServerPort-targetServerIP:targetServerPort -s <intercept server,> 
	  
      For example(assume 10.110.12.17 is the IP address of the target server):

	  ./tcpcopy -x 3306-10.110.12.17:3306 -s 10.110.12.18 

	  tcpcopy would capture MySQL packets(assume MySQL listens on 3306 port) on current 
      server, do the necessary modifications and send these packets to the target port 
      '3306' on '10.110.12.17'(the target MySQL), and connect 10.110.12.18 for asking 
      intercept to pass response packets to it.

##Release History
+ 2014.09  v1.0    mysql-sgt-replay-module released


##Bugs and feature requests
Have a bug or a feature request? [Please open a new issue](https://github.com/session-replay-tools/mysql-sgt-replay-module/issues). Before opening any issue, please search for existing issues.


## Copyright and license

Copyright 2014 under [the BSD license](LICENSE).


