#!/bin/bash

#################
#Run as root
#################
if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit -1
fi

if [ $# != 2 ]
then
        echo "Usage: $0 <iterations> <working directory>"
        echo ""
        echo "Iterations indicates number of times the experiment should be rerun before"
        echo "generating the mean and std. dev. results."
	echo ""
	echo "working directory indicates the directory to be used as the current"
	echo "working directory by this script before performing the experiment"
	echo ""
        exit
fi


filesystem=("btrfs" "ext4" "flexclone" "xfs" "qemu_qcow2")
#filesystem=("flexclone")
device=("ssd")

mount_point_flag=0
mount_cmd_flag=1
umount_cmd_flag=2

copy_time_dir="copy_time"
copy_time_raw_data="copy_time_raw_data"
processing_qcow2=0

cd $2
if [ $? != 0 ]
then
	echo "Error: $0 failed to setup $2 as the current working directory."
	exit
fi
script_home=$(pwd)
for fs in "${filesystem[@]}"
do
	echo "================================"
	echo $fs
	echo "================================"

	for dev in "${device[@]}"
	do
		echo "-------------------------------"
		echo $dev
		echo "-------------------------------"

		#qcow2 uses ext4
		if [ $fs == "qemu_qcow2" ]
		then
			processing_qcow2=1
			fs="ext4"
		fi

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
		mkdir $copy_time_dir
		cd $copy_time_dir

		if [ $processing_qcow2 -eq 1 ]
		then
			fs="qemu_qcow2"
		fi

		#copy copy_time finding scripts to target filesystem
		cp "$script_home/$fs/"* .
		#ls

		#generate files before calculating copy time
		if [ $processing_qcow2 -eq 0 ]
		then
			./generate_files.sh
		fi

		#perform copy operation and calculate the copy time
		./cp.sh $1 | tee -a $copy_time_raw_data

		#copy copy_time data from respective filesystems to this scripts folder
		cp $copy_time_raw_data "${script_home}/${copy_time_raw_data}_${fs}_${dev}"
		processing_qcow=0

		cd $script_home
	done
done


#summarize result
cd $script_home
./process_copy_time_data_for_evaluation_plot.sh

#generate plot
gnuplot plot_evaluation.p
epstopdf ssd_copying_file_data_evaluation.eps
rm ssd_copying_file_data_evaluation.eps
exit 0
