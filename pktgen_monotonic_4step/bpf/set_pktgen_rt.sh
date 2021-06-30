#/bin/bash
pid=$(ps -ef | grep -A1 "[b]in/pktgen --client" | awk '{$1=$1};1' | cut -d ' ' -f 2)
echo "PID: $pid"
chrt -p 90 $pid
