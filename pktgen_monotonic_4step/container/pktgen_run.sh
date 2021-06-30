#!/bin/sh
/bin/pktgen --client --interface eth0 --port 5000 --out_file /k8s-shared/pktgen-100us-container-test-6h.log --runtime 21600 --warmup 8192
