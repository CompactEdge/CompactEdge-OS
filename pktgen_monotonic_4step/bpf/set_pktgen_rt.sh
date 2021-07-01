#/bin/bash
pid=$(ps -efT | grep  "[b]in/pktgen --client" | awk '{$1=$1};1' | cut -d ' ' -f 2)
SAVEIFS=$IFS
IFS=$'\n'
pid=($pid)
IFS=$SAVEIFS

echo "main thread PID: ${pid[0]}"
chrt -p 90 ${pid[0]}
echo "write thread PID: ${pid[1]}"
chrt -p 90 ${pid[1]}

