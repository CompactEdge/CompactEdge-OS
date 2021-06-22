#!/bin/bash

ip link set dev em3 xdp obj 1-xdp_timestamp_in_pkt.o

tc qdisc add dev em3 clsact
tc filter add dev em3 ingress prio 1 handle 1 bpf da obj 2-tc_ingress_timestamp_in_pkt2.o

bpftool prog load 3-iptables_timestamp_in_pkt2.o /sys/fs/bpf/iptables_timestamp type socket
iptables -N TSTAMP
iptables -A INPUT -i em3 -p udp --dport 5000 -j TSTAMP
iptables -A TSTAMP -i em3 -m bpf --object-pinned /sys/fs/bpf/iptables_timestamp -j LOG
