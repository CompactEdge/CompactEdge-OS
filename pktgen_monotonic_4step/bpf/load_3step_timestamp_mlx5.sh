#!/bin/bash

#ifconfig enp137s0 mtu 1500
ip link set dev enp137s0 xdp obj 1-xdp_timestamp_in_pkt.o

#tc qdisc add dev enp137s0 clsact
#tc filter add dev enp137s0 ingress prio 1 handle 1 bpf da obj 2-tc_ingress_timestamp_in_pkt2.o

bpftool prog load 3-iptables_timestamp_in_pkt2.o /sys/fs/bpf/iptables_timestamp type socket
iptables -N TSTAMP
iptables -I INPUT -i enp137s0 -p udp --dport 32767 -j TSTAMP
iptables -I TSTAMP -i enp137s0 -m bpf --object-pinned /sys/fs/bpf/iptables_timestamp -j LOG
