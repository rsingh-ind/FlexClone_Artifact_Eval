#!/bin/bash

#################
#Run as root
#################
if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit -1
fi

filesystem=("btrfs" "xfs")
device=("ssd")

mount_point_flag=0
mount_cmd_flag=1
umount_cmd_flag=2

mount_point=""

script_home=$(pwd)
for fs in "${filesystem[@]}"
do
	echo "================================"
	echo $fs
	echo "================================"
	out_file="result_$fs"
	> $out_file

	for dev in "${device[@]}"
	do
		echo "-------------------------------"
		echo $dev
		echo "-------------------------------"

		#unmount filesystem 
		echo "$0 unmounting $fs from $dev.."
		mount_point=$(../mount_info.sh $fs $dev 0)
		mountpoint -q "$mount_point"
		status=$?
		if [ $status -eq 0 ]; then
		    umount "$mount_point"
		    umount_status=$?
		    if [ $umount_status -ne 0 ]; then
			echo "Error: $0 unmounting $mount_point failed..skipping this device.."
			exit -1
		    fi
		fi

		#mount filesystem
		echo "$0 mounting $fs on $dev.."
		mnt=$(../mount_info.sh $fs $dev $mount_cmd_flag)
		$($mnt)
		if [ $? != 0 ]
		then
			echo "Error: $0 mounting failed..skipping this device.."
			exit -1
		fi

		#Go to mount point
		mnt_pt=$(../mount_info.sh $fs $dev $mount_point_flag)
		cd $mnt_pt
		if [ $? != 0 ]
		then
			echo "Error: $0 Failed to enter mount point: $mnt_pt"
			exit -1
		fi
		
		#create fresh directory where we can run our copy time scripts
		if [ "$(pwd)" == "/" ]; then
			echo "Error: $0 Aborting..Somehow current working directory is '/' instead of the desired mount point"
			exit -1
		fi
		rm -r *

		#create source file
		src="4GB"
		clone="4GB_child"
		echo "Generating 4GB source file.."
		dd if="/dev/urandom" of="$src" bs=1M count=4096
	
		#flush caches
		echo "Flushing caches.."
		sync; echo 3 > /proc/sys/vm/drop_caches

		#bring source file to memory
		cat $src > /dev/null

		#clone source file
		echo "cloning source file.."
		ts=$(date +%s%N)
		cp --reflink $src $clone
		ns=$(($(date +%s%N) - $ts))

		clone_time_ms=$(echo "$ns / 1000000" | bc -l)
		clone_time_ms=$(printf "%.2f\n" $clone_time_ms)
		mem_usage_GB_before=$(free | awk '/^Mem:/ {printf "%.2f GB", ($2 - $4)/1024/1024}')

		#access clone
		ts=$(date +%s%N)
		echo "sequentially reading source file.."
		cat $clone > /dev/null
		ns=$(($(date +%s%N) - $ts))

		access_time_ms=$(echo "$ns / 1000000" | bc -l)
		access_time_ms=$(printf "%.2f\n" $access_time_ms)
		mem_usage_GB_after=$(free | awk '/^Mem:/ {printf "%.2f GB", ($2 - $4)/1024/1024}')

		echo "$fs: Clone time(ms): $clone_time_ms, Mem Usage after clone (GiB): $mem_usage_GB_before," | tee -a $out_file
	        echo -n "Access time(ms): $access_time_ms, Mem Usage after access (GiB): $mem_usage_GB_after" | tee -a $out_file
		echo "" | tee -a $out_file

		#Save result file
		mv $out_file $script_home
		if [ $? != 0 ]
		then
			echo "Error: $0 failed to save output file into '$script_home'."
			exit -1
		fi
		echo ""
		cd $script_home
	done
done
exit 0
