#!/bin/bash

#############################################################################################
#Note: 
#	1) Run this script as root and as a background process i.e. "$./script &"
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

#Prepare setup
dest="${mount}frnd_file_rebuilding/"
if [ "$dest" == "/" ]; then
        echo "Error: $0 Aborting..Somehow dest is '/' instead of the desired dest"
        exit -1
fi

mkdir $dest
cd $curdir

cp filemicro_rwrite_stage1.f $dest
cp setxattr_generic $dest
cp copy_dir $dest
cp fill_child $dest
cp update_version_count $dest
cp recover $dest

#create parent files
echo "****** creating parent files ******"
cd $dest
echo 0 > /proc/sys/kernel/randomize_va_space
filebench -f filemicro_rwrite_stage1.f

#create child and frnd files
echo "****** creating child and frnd files ******"
par_absolute_path="${dest}/bigfileset/00000001"
child_absolute_path="${dest}/bigfileset_child/00000001"
frnd_absolute_path="${dest}/bigfileset_frnd/00000001"
./copy_dir $par_absolute_path $child_absolute_path $frnd_absolute_path

#fill data in child files
echo "****** filling data in child files..Will take around 20 mins ******"
./fill_child bigfileset_child/00000001 > /dev/null

total_files=10000
files_to_recover=("0" "1000" "2000" "3000" "4000" "5000" "6000" "7000" "8000" "9000" "10000")

for i in "${files_to_recover[@]}"
do
	end=$1
	#output file
	out="recovering_${i}_files_${dev}"
	for ((j=0; j<end; j++))
	do
		#update version count of some 'k' child files
		cd $dest
		echo "****** Updating version count of $i friend files artificially ******"
		./update_version_count bigfileset_child/00000001 $i

		#remount filesystem
		cd $curdir
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
		mount_cmd=$(../mount_info.sh $fs $dev 1)
		$($mount_cmd)
		if [ $? != 0 ]
		then
			echo "Error: $0 mounting failed..skipping this device.."
			exit -1
		fi

		#Trigger recovery
		cd $dest
		echo "****** Performing recovery ******"
		./recover bigfileset_frnd/00000001 1 | tee -a $out

		echo "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF Iteration: $j of recovering: $i files done FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
	done

	#Save output file
	mv $out $outPath 
done

cd $curdir
sudo umount $mount

cd fio_output/ssd
./summarize_results.sh
gnuplot plot_recovery.p
epstopdf recovery_time.eps
cp recovery_time.pdf ../..

exit 0
