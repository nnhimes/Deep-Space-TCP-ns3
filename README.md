# Deep-Space-TCP-ns3
A comparison of TCP variants in the NS3 tool under deep space conditions. For ECE 773, Spring 2021

This repo is just for the Scratch directory in NS-3

Before running, the TCP Connection Timeout value must be updated to handle deep space propagation delays

`In /ns-x.xx/src/internet/model/tcp-socket.cc change line 82 regarding the "ConnTimout" second value`
For Earth to communicate with very far away objects such as Proxima Centauri, this should be set to a second value such as 999999999.

`./waf --run tcpCompare`
