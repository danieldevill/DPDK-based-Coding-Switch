#!/bin/bash
file=$(head -n 1 /tmp/rlaunchflg)
echo $file
if [ "$file" == "relaunch_true" ]; then
	> /tmp/rlaunchflg
	echo "mininet" | sudo pkill -9 -f l2fwd-nc
fi 
