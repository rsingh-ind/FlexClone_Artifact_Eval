#!/bin/bash

#############################################################################################
#Note:
#       1) Run this script as root and as a background process i.e. "$./script &"
#
#################################################################################################

base_image="base-image-full.img"
base_image_copy="base-image-full.img.snapshot"
base_image_frnd="base-image-full.img.frnd"

fioFilesFilter="8GB_"
fioOutName="fioOut_"

curdir=$(pwd)
dev="ssd"
fs=("dcopy")
#fs=("dcopy" "ext4")

module_path=$(../../module_info.sh)
outPath="${curdir}/fio_output/"

if [ $# != 1 ]
then
        echo "Usage: $0 <num iterations per fio job>"
        exit -1
fi

pkill qemu
for i in ${fs[@]}
do
	#mount point
	mount=$(../../mount_info.sh $i $dev 0)
	dest="${mount}qemu-image-full/"

	sudo umount $mount
	mount_cmd=$(../../mount_info.sh $i $dev 1)
	if [ $i == "dcopy" ]
	then
		echo "Re-inserting ext4-module"
		cd $module_path
		sudo ./remove_ext4_module.sh
		sudo ./insert_ext4_module.sh
		cd $curdir
	fi
	$($mount_cmd)

	#cleanup old contents
	if [ ${mount} == "/" ]
	then
		echo "Error: Trying to run rm on /"
		exit -1
	fi
	rm -r "${mount}/"*

	#Bring fresh contents to dest
	rm -r $dest
	mkdir $dest
	base_vm_path=$(../../vm_info.sh 0)
	base_vm_path="${base_vm_path}/${base_image}"
	echo "vm source: $base_vm_path"
	echo "Copying $base_vm_path to $dest"
	cp $base_vm_path $dest
	echo "Copying done"

	#convert vm image from qcow2 format to raw format
	cd $dest
	if [ $i != "ext4" ]
	then
		echo "Converting vm image from qcow2 format to raw format"
		qemu-img convert -f qcow2 -O raw $base_image "$base_image.raw"
		rm $base_image
		mv "$base_image.raw" $base_image
	fi
	cd $curdir


	#copy relevant files to target filesystem  
	echo "copying scripts to $i filesystem"
	cp remote.sh $dest
	if [ $i == "dcopy" ]
	then
		echo "copying setxattr to $i filesystem"
		cp setxattr_generic $dest
	fi


	allFioJobs=$(ls | grep "^$fioFilesFilter")
	#echo $allFioJobs

	allFioJobs=($allFioJobs)
	#echo $allFioJobs


	for j in "${allFioJobs[@]}"
	do
		end=$1
		for ((k=0; k<end; k++))
		do
			cd $curdir
			#Shutdown vm if it is running
			#Note:
			#echo 1 sends '1' as the password
			#sudo -S accepts password from stdin
			#Putting commands in double quotes next to ssh command runs the commands (mentioned in double quotes) inside the remote server
			echo "Shutting down vm if it is running"
			sshpass -p '1' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 5555 rohitvm@localhost "echo 1 | sudo -S shutdown now"
			echo "waiting for shutdown to finish and port 5555 to free up"
			pkill qemu-system-x86
			sleep 120	#wait for shutdown to finish and port 5555 to free up
		
			#prepare filesystem
			echo "Remounting $i filesystem"
			sudo umount $mount
			if [ $i == "dcopy" ]
			then
				echo "Re-inserting ext4-module"
				cd $module_path
				sudo ./remove_ext4_module.sh
				sudo ./insert_ext4_module.sh
				cd $curdir
			fi
			mount_cmd=$(../../mount_info.sh $i $dev 1)
			$($mount_cmd)


			#init the benchmark process
			echo "initiating the benchmark process"
			cd $dest

			#create child vm
			echo "creating child vm"
			if [ $i == "dcopy" ]
			then
				rm $base_image_copy
				rm $base_image_frnd
				./setxattr_generic -c $base_image_copy -p $base_image -f $base_image_frnd
			elif [ $i == "ext4" ]
			then
				rm $base_image_copy
				cp $base_image $base_image_copy
			else
				rm $base_image_copy
				cp --reflink $base_image $base_image_copy
			fi
			sync; echo 3 > /proc/sys/vm/drop_caches
			fstrim -v .

			#boot child vm
			echo "booting child vm"
			./remote.sh $base_image_copy
			sleep 30

			#Run fio inside child vm
			echo "Performing operations inside child vm"
			echo "Running fio on configuration file: $j"
			sshpass -p '1' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 5555 rohitvm@localhost "cd Downloads; fio $j;" | tee -a "${outPath}/${fioOutName}${j}_${i}"
			
			echo "======================== $i $j $k ========================"
		done
	done

	cd $curdir

	#Shutdown vm if it is running
	echo "Shutting down vm if it is running"
	sshpass -p '1' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 5555 rohitvm@localhost "echo 1 | sudo -S shutdown now"
	echo "waiting for shutdown to finish and port 5555 to free up"
	sleep 120	#wait for shutdown to finish and port 5555 to free up

	sudo umount $mount

done
