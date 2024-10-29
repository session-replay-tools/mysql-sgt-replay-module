# A TCPCopy Module for MySQL Session Replay

The `mysql-sgt-replay-module` is a TCPCopy extension designed to replay MySQL sessions, enabling realistic testing of MySQL applications.

For detailed information, please consult [TCPCopy](https://github.com/session-replay-tools/tcpcopy) before proceeding.

## Installation

### Installing `intercept` on the Assistant Server

1. git clone git://github.com/session-replay-tools/intercept.git
2. cd intercept
3. ./configure 
4. make
5. make install

### Installing `tcpcopy` on the Online Server

1. git clone git://github.com/session-replay-tools/tcpcopy.git
2. cd tcpcopy
3. git clone git://github.com/session-replay-tools/mysql-sgt-replay-module.git
4. ./configure --set-protocol-module=mysql-sgt-replay-module
5. make
6. make install

## Usage Guide

### 1. **On the Target Server Running MySQL Applications:**

a) Configure Routing to Direct Response Packets to the Assistant Server

For example, assuming `10.110.12.18` is the IP address of the assistant server and `10.110.12.15` is the MySQL client IP address, use the following route command to direct all responses from `10.110.12.15` to the assistant server:

`route add -host 10.110.12.15 gw 10.110.12.18`

b) Start MySQL with `--skip-grant-tables`

### 2. **On the Assistant Server Running `intercept` (Root Privilege or CAP_NET_RAW Capability Required):**

  `./intercept -F <filter> -i <device>`

Note that the filter format is the same as the pcap filter. For example:

   `./intercept -i eth0 -F 'tcp and src port 3306' -d`

In this example, `intercept` will capture response packets from a TCP-based application listening on port 3306, using the eth0 network device.

Please note that `ip_forward` is not enabled on the assistant server.

### 3) **On the Online Source Server (Root Privilege or CAP_NET_RAW Capability Required):**

`./tcpcopy -x localServerPort-targetServerIP:targetServerPort -s <intercept server> [-c <ip range>]`

For example (assuming 10.110.12.17 is the IP address of the target server):

`./tcpcopy -x 3306-10.110.12.17:3306 -s 10.110.12.18`

`tcpcopy` captures MySQL packets (assuming MySQL listens on port 3306) on the current server, modifies them as needed, and forwards them to port 3306 on `10.110.12.17` (the target MySQL server). It also connects to `10.110.12.18` to request that `intercept` forwards the response packets to it.

## Note

1. Only the complete sesssion could be replayed
2. Currently, it does not support MySQL 8.0 yet.
3. For additional assistance, visit [tcpcopy](https://github.com/session-replay-tools/tcpcopy).

## Release History

+ 2017.03  v1.0    mysql-sgt-replay-module released
+ 2024.09  v1.0    Open source fully uses English

## Bugs and Feature Requests

Have a bug or a feature request? [Please open a new issue](https://github.com/session-replay-tools/mysql-sgt-replay-module/issues). Before opening any issue, please search for existing issues.

## Copyright and License

Copyright 2024 under [the BSD license](LICENSE).
