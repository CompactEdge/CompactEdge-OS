#!/bin/bash

#ifconfig enp137s0 mtu 1500
ip link set dev enp137s0 xdpdrv obj 1-xdp_timestamp_in_pkt.o

