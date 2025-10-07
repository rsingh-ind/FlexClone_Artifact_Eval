#!/bin/bash

#############################################################################################
#Note:
#       1) Run this script as root and as a background process i.e. "$./script &"
#
#################################################################################################

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
        exit 1
fi


fs=("flexclone" "btrfs" "xfs" "ext4")
workload=("micro_append_stage1.f" "micro_append_stage2.f" "fileserver_stage1.f" "fileserver_stage2.f" "varmail_stage1.f" "varmail_stage2.f")
#fs=("flexclone" "btrfs" "xfs" "ext4")
#workload=("micro_append_stage1.f" "micro_append_stage2.f")
curdir=$(pwd)

dev="ssd"
module_path=$(../module_info.sh)

outPath="${curdir}/output/"

echo 0 > /proc/sys/kernel/randomize_va_space

for i in "${fs[@]}"
do
	#mount point
	mount=$(../mount_info.sh $i $dev 0)
	if [ "$mount" == "/" ]; then
		echo "Error: $0 Aborting..Somehow mount point is '/' instead of the desired mount point"
		exit 1
	fi
	upper="${mount}/upper"
	lower="${mount}/lower"
	work="${mount}/work"
	merged="${mount}/merged"
	dest="${mount}/filebench/"

	#Prepare setup
        cd $curdir
	echo "$0 unmounting $mount from $dev.."
	mountpoint -q "$mount"
	status=$?
	if [ $status -eq 0 ]; then
	    umount "$mount"
	    umount_status=$?
	    if [ $umount_status -ne 0 ]; then
		echo "Error: $0 unmounting $mount failed..skipping this device.."
		exit 1
	    fi
	fi

	#fresh mount
	mount_cmd=$(../mount_info.sh $i $dev 1)
	$($mount_cmd)
	if [ $? != 0 ]
	then
		echo "Error: $0 mounting failed..skipping this device.."
		exit 1
	fi

        rm -r "${mount}"/*
	mkdir $upper
	mkdir $lower
	mkdir $work
	mkdir $merged

        echo "copying scripts to $i filesystem"
	cp *"stage1.f" $lower
	cp *"stage2.f" $lower

	for j in "${workload[@]}"
	do
		cd $curdir

		#prepare filesystem
		echo "Remounting $i filesystem"
		mountpoint -q "$merged"
		status=$?
		if [ $status -eq 0 ]; then
		    umount "$merged"
		    umount_status=$?
		    if [ $umount_status -ne 0 ]; then
			echo "Error: $0 unmounting $merged failed..skipping this device.."
			exit 1
		    fi
		fi

		mountpoint -q "$mount"
		status=$?
		if [ $status -eq 0 ]; then
		    umount "$mount"
		    umount_status=$?
		    if [ $umount_status -ne 0 ]; then
			echo "Error: $0 unmounting $mount failed..skipping this device.."
			exit 1
		    fi
		fi

		if [ $i == "flexclone" ]
		then
			echo "Re-inserting ext4-module"
			cd $module_path
			./remove_ext4_module.sh
			./insert_ext4_module.sh
			cd $curdir
		fi
		mount_cmd=$(../mount_info.sh $i $dev 1)
		$($mount_cmd)
		if [ $? != 0 ]
		then
			echo "Error: $0 mounting failed..skipping this device.."
			exit 1
		fi


		#stage 1 of workload processing
		#create directory containing files on which open/close/read/write is to be performed
		echo "workload: $j"
		if [[ $j == *"_stage1.f"* ]]	#double brackets are essential for string comparison else *"_stage1.f"* will be treated as files in cur dir with this pattern
		then
			echo "Running stage 1 workload: $j"
			cd $lower
			rm -r bigfileset
			filebench -f $j		#This will by itself delete the old bigfileset directory created by the prev. workload
			if [ $? != 0 ]
			then
				echo "Error: filebench failed..."
			fi
			continue
		fi


		#stage 2 of workload processing
		#Reuse already existing directory and run benchmark
		end=$1
		for ((k=0; k<end; k++))
		do
			cd $mount
			echo "dropping caches"
			sync; echo 3 > /proc/sys/vm/drop_caches
			#echo "triggering fstrim"
			#fstrim -v .
			echo "mounting overlayfs"
			mount -t overlay overlay -olowerdir=$lower,upperdir=$upper,workdir=$work $merged
			cd $merged
			ls
			echo "Running stage 2 workload: $j"
			filebench -f $j | tee -a "${outPath}/${i}_${j}"
			if [ ${PIPESTATUS[0]} != 0 ]
			then
				echo "Error: Filebench failed.."
				exit 1
			fi

			echo "=============================== ${i}, ${j}, iteration ${k} done ===============================" | tee -a "${outPath}/${i}_${j}"
			cd ..
			mountpoint -q "$merged"
			status=$?
			if [ $status -eq 0 ]; then
			    umount "$merged"
			    umount_status=$?
			    if [ $umount_status -ne 0 ]; then
				echo "Error: $0 unmounting $merged failed..skipping this device.."
				exit 1
			    fi
			fi
			echo "------------- Sleeping for 30 secs. merged directory should have been unmounted -------------"
			sleep 30
			echo "------------- Wokeup -------------"

			echo "Deleting $upper"
			rm -r $upper
			echo "Deleting $merged"
			rm -r $merged
			mkdir $upper
			echo "Created $upper"
			mkdir $merged
			echo "Created $merged"
			#cd $merged
			#cd ..
			#find . -maxdepth 1 -type f -exec rm -f {} \;	#delete friend files
		done
	done
	cd $curdir
	mountpoint -q "$merged"
	status=$?
	if [ $status -eq 0 ]; then
	    umount "$merged"
	    umount_status=$?
	    if [ $umount_status -ne 0 ]; then
		echo "Error: $0 unmounting $merged failed..skipping this device.."
		exit 1
	    fi
	fi

	mountpoint -q "$mount"
	status=$?
	if [ $status -eq 0 ]; then
	    umount "$mount"
	    umount_status=$?
	    if [ $umount_status -ne 0 ]; then
		echo "Error: $0 unmounting $mount failed..skipping this device.."
		exit 1
	    fi
	fi
	echo "dropping caches"
	sync; echo 3 > /proc/sys/vm/drop_caches
done

cd output
./gen_output.sh
gnuplot plot_fioseqread.p
epstopdf ssd_out_filebench_child_ops_per_sec.eps
cp ssd_out_filebench_child_ops_per_sec.pdf ..
