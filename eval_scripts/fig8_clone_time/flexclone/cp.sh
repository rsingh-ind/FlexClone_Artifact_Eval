#!/bin/bash
#Reference: https://askubuntu.com/questions/1080113/measuring-execution-time-of-a-command-in-milliseconds
files=("4KB"  "256KB" "4MB" "32MB" "256MB" "1GB" "4GB" "16GB")

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
	echo "================ Iteration $i ===================="
	for j in "${files[@]}"
	do
		sync; echo 3 > /proc/sys/vm/drop_caches
		ts=$(date +%s%N)
		./setxattr_generic -p $j -c "${j}.child" -f "${j}.frnd"
		ns=$(($(date +%s%N) - $ts))
		#echo "Nanoseconds: $ns"
		echo -n "$j "
		echo "$ns / 1000000" | bc -l
		rm "${j}.child"
		rm "${j}.frnd"
	done
done
