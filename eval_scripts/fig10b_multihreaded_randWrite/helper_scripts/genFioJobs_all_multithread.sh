#!/bin/bash

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit 1
fi


#Usage: $./script <dummy>  <operation (read, write)>  <threadUsage (for ourExt4 Read/Write): (asynchOn, asynchOff, 0)>

./genFioJobs_multithread.sh dummy "read" asynchOff 
./genFioJobs_multithread.sh dummy write asynchOff
