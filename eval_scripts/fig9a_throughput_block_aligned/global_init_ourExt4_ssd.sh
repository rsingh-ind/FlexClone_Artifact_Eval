#!/bin/bash

#############################################################################################
#Note: 
#	1) Run this script as root and as a background process i.e. "$./script &"
#	2) Directory where parent file (8GB) is present should contain clean.sh (deletes old child and friend files) and setxattr binary
#################################################################################################

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit -1
fi

if [ $# != 1 ]
then
        echo "Usage: $0 <iterations>"
        echo ""
        echo "Iterations indicates number of times the experiment should be rerun before"
        echo "generating the mean and std. dev. results."
        echo ""
        exit -1
fi

fioFilesFilter="8GB_"
fioOutName="fioOut_"

coldCacheFilter="coldCache"

curdir=$(pwd)
dev="ssd"
fs="flexclone"
module_path=$(../module_info.sh)
outPath="${curdir}/fio_output/${dev}/"

#mount point
mount=$(../mount_info.sh $fs $dev 0)
if [ "$mount" == "/" ]; then
        echo "Error: $0 Aborting..Somehow mount point is '/' instead of the desired mount point"
        exit -1
fi

#Prepare setup
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


#fresh mount
cd $module_path
sudo ./remove_ext4_module.sh
sudo ./insert_ext4_module.sh
cd $curdir
mount_cmd=$(../mount_info.sh $fs $dev 1)
$($mount_cmd)
if [ $? != 0 ]
then
        echo "Error: $0 mounting failed..skipping this device.."
        exit -1
fi

#clean mount point
cd $mount
rm -r *

#prepare source file
dest="${mount}readWriteChild/"
if [ "$dest" == "/" ]; then
        echo "Error: $0 Aborting..Somehow dest is '/' instead of the desired dest"
        exit -1
fi

mkdir $dest
cd $dest
echo "Generating 8GB source file...Please wait for 2-3 mins.."
dd if=/dev/urandom of=8GB bs=1M count=8192
cd $curdir
cp clean.sh $dest
cp setxattr_generic $dest

allFioJobs=$(ls | grep "^$fioFilesFilter")
#echo $allFioJobs

allFioJobs=($allFioJobs)
#echo $allFioJobs

for i in "${allFioJobs[@]}"
do
	end=$1
	#copy relevant files to our filesystem	
	echo "copying fio file to our filesystem"
	cp $i $dest 

	for ((j=0; j<end; j++))
	do
		echo "**** $i, iteration: $j ****"
		largeFileName="8GB"	#parent filename	
		echo "source FileName: $largeFileName"

		largeFileCompletePath=$dest$largeFileName
		echo "source FilePath: $largeFileCompletePath"

		#prepare filesystem
                echo "$0 unmounting $fs from $dev.."
                mount_point=$(../mount_info.sh $fs $dev 0)
                mountpoint -q "$mount_point"
                status=$?
                if [ $status -eq 0 ]; then
                    umount "$mount_point"
                    umount_status=$?
                    if [ $umount_status -ne 0 ]; then
                        echo "Error: $0 unmounting $mount_point failed..skipping this device.."
                        break
                    fi
                fi

		
		cd $module_path
		sudo ./remove_ext4_module.sh
		sudo ./insert_ext4_module.sh
		cd $curdir
		$($mount_cmd)
		if [ $? != 0 ]
		then
			echo "Error: $0 mounting failed..skipping.."
			break
		fi

		#init the benchmark process
		cd $dest
		./clean.sh
		sync; echo 3 > /proc/sys/vm/drop_caches
		#fstrim -v .

		#create parent-child relationship
	        childFile="${largeFileName}_child"
		frndFile="${largeFileName}_frnd"
		echo "childFile: $childFile"
		echo "frndFile: $frndFile"
		echo "parentFile: $largeFileName"

		echo "Setting Parent-child relationship."
                ./setxattr_generic -c "$childFile" -p "$largeFileName" -f "$frndFile"
                echo "Parent-child relationship created."


		#Need to bring parent file in memory?
		#grep returns 0 on success and 1 on failure. So, if success, means, cold i.e. not hot. So, hot is 0 (hot=0 indicates file isn't hot i.e. is cold cache. hot=1 indicates hot cache).
		printf "%s" "$i" | grep "$coldCacheFilter" >/dev/null
		hotCache=$?
		echo "hotCache: $hotCache"

		if [ "$hotCache" == "1" ];then
			echo "HotCache!!!!! Bringing data from disk!!!"
			cat "$largeFileName" > "/dev/null"
			echo "HotCache Done!!!!!"
		fi

		#Run fio
		fioOut="${fioOutName}${i}_${fs}_${dev}"
		echo "fio output file name: $fioOut"
		echo "" >> $fioOut
		echo "=================================================" >> $fioOut 
		echo "" >> $fioOut
		nThreads=$(echo $i | egrep -o '[0-9]+thread' 2> /dev/null | egrep -o '[0-9]+' 2> /dev/null)
		nThreads=$(($nThreads-1))
		echo "fio threads pinned to cpu's 0 to $nThreads"
		echo "Running fio.."
		taskset -c 0-$nThreads fio $i | tee -a "$fioOut"
                if [ ${PIPESTATUS[0]} != 0 ]
                then
                        echo "Failed to run fio.."
                        exit -1
                fi

		./clean.sh
		echo "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF Iteration: $j of fio script: $i done FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
		cd $curdir
	done

	cd $dest
	#Save fio output file
        mv $fioOut $outPath
        if [ $? != 0 ]
        then
                echo "Error: $0 failed to save output file into '$outPath'."
                exit -1
        fi

	#delete fio script
	rm $i
	cd $curdir
	echo ""
done

sudo umount $mount
exit 0
