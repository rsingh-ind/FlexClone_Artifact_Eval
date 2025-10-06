#!/bin/bash
#Reference: https://askubuntu.com/questions/1080113/measuring-execution-time-of-a-command-in-milliseconds
files=("4K" "256K" "4M" "32M" "256M" "1G" "4G" "16G")

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

if [ $# != 1 ]
then
        echo "Usage: $0 <iterations>"
        echo ""
        echo "Iterations indicates number of times the experiment should be rerun before"
        echo "generating the mean and std. dev. results."
        echo ""
        exit
fi

for((i=1; i<=$1; i++ ))
do
        echo "================ $i ===================="
        for j in "${files[@]}"
        do
                #Disk image creation format example:
                #qemu-img create -f qcow2 -o preallocation=full image.qcow2 1G
                qemu-img create -f qcow2 -o preallocation=full $j $j 1>/dev/null 2> /dev/null

                sync; echo 3 > /proc/sys/vm/drop_caches
                ts=$(date +%s%N)

                #Snapshot creation format example:
                #qemu-img create -f qcow2 -b centos-cleaninstall.img -F qcow2 snapshot.img
                qemu-img create -f qcow2 -b $j -F qcow2 "${j}_copy" 1>/dev/null 2> /dev/null

                #echo $(($(date +%s%N) - $ts))
                ns=$(($(date +%s%N) - $ts))
                #echo "Nanoseconds: $ns"
                echo -n "$j "
                echo "$ns / 1000000" | bc -l
                rm "${j}_copy"
                #ms=$((ns / 1000000))
        done
done
