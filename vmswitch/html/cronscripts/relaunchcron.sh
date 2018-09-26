#!/bin/bash
file=$(head -n 1 /tmp/rlaunchflg)
if [ "$file" == "relaunch_true" ]; then
	sudo kill -15 $(pidof "l2fwd") && sudo rm /tmp/rlaunchflg
fi 
