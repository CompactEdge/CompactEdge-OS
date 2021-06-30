#!/bin/bash


tc qdisc add dev enp137s0 clsact
tc filter add dev enp137s0 ingress prio 1 handle 1 bpf da obj 2-tc_ingress_timestamp_in_pkt2.o
