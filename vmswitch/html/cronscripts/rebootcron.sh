#!/bin/bash
file=$(head -n 1 /tmp/rbootflg)
echo $file
if [ "$file" == "reboot_true" ]; then
	> /tmp/rbootflg
	echo "mininet" | sudo -S reboot
fi 
