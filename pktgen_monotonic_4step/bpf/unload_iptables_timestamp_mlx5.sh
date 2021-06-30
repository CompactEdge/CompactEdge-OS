#!/bin/bash
iptables -D TSTAMP -m bpf --object-pinned /sys/fs/bpf/iptables_timestamp -j LOG
iptables -D FORWARD -i enp137s0 -p udp --dport 5000 -j TSTAMP
iptables -X TSTAMP

rm -rf /sys/fs/bpf/iptables_timestamp

