#!/bin/bash
# Get sidecar PID
cid=$(docker ps -f "name=k8s_POD_pktgen-edge-latency-teardown_*" | awk '{$1=$1};1' | cut -d ' ' -f 1 | tail -n 1)
echo "Sidecar ContainerID: $cid"
pid=$(docker inspect $cid -f '{{.State.Pid}}')
echo "Sidecar Container PID: $pid"
# Attach XDP Prog to container
nsenter -t $pid -n ip link set dev eth0 xdp obj 4-xdp_k8s_timestamp_in_pkt.o
