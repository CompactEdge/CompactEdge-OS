#!/bin/bash
chmod +x pktgen pktgen_run.sh
docker build --tag=pktgen-latency-teardown:latest .
