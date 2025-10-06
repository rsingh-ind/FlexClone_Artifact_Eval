#!/bin/bash

#############################################################################################
#Note:
#       1) Run this script as root and as a background process i.e. "$./script &"
#
#################################################################################################

base_image="base-image-full.img"
#base_image_copy="base-image-full.img.snapshot"
#base_image_frnd="base-image-full.img.frnd"

fioFilesFilter="8GB_"
fioOutName="fioOut_"

curdir=$(pwd)
dev="ssd"
fs=("ext4")

module_path=$(../../module_info.sh)
outPath="${curdir}/fio_output/"



if [ $# != 1 ]
then
        echo "Usage: $0 <num iterations per fio job>"
        exit -1
fi


for i in ${fs[@]}
do
	#mount point
	mount=$(../../mount_info.sh $i $dev 0)
	dest="${mount}qemu-image-full/"

	sudo umount $mount
	mount_cmd=$(../../mount_info.sh $i $dev 1)
	$($mount_cmd)

	#cleanup old contents
	if [ ${mount} == "/" ]
        then
                echo "Error: Trying to run rm on /"
                exit -1
        fi
	rm -r "${mount}/"*

	#Bring fresh contents to dest
	mkdir $dest
	base_vm_path=$(../../vm_info.sh 0)
	base_vm_path="${base_vm_path}/${base_image}"
	echo "vm source: $base_vm_path"
	echo "Copying $base_vm_path to $dest"
	cp $base_vm_path $dest
	echo "Copying done"

	#convert vm image from qcow2 format to raw format
	cd $dest
	echo "Converting vm image from qcow2 format to raw format"
	qemu-img convert -f qcow2 -O raw $base_image "$base_image.raw"
	rm -i $base_image
	mv "$base_image.raw" $base_image
	cd $curdir


	#copy relevant files to target filesystem  
	echo "copying scripts to $i filesystem"
	cp remote.sh $dest

	#select fio files to use
	fio_grep_string="_Ext4"
	allFioJobs=$(ls | grep "^$fioFilesFilter" | grep $fio_grep_string)
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
			sshpass -p '1' ssh -p 5555 rohitvm@localhost "echo 1 | sudo -S shutdown now"
			echo "waiting for shutdown to finish and port 5555 to free up"
			sleep 120	#wait for shutdown to finish and port 5555 to free up
		
			#prepare filesystem
			echo "Remounting $i filesystem"
			sudo umount $mount
			mount_cmd=$(../../mount_info.sh $i $dev 1)
			$($mount_cmd)


			#init the benchmark process
			echo "initiating the benchmark process"
			cd $dest
			sync; echo 3 > /proc/sys/vm/drop_caches

			#boot vm
			echo "booting vm"
			./remote.sh $base_image
			sleep 30

			#Run fio inside vm
			echo "Performing operations inside vm"
			echo "Running fio on configuration file: $j"
			sshpass -p '1' ssh -p 5555 rohitvm@localhost "cd Downloads; fio $j" >> "${outPath}/${fioOutName}${j}_raw"
			
			echo "======================== $i $j $k ========================"
		done
	done

	cd $curdir

	#Shutdown vm if it is running
	echo "Shutting down vm if it is running"
	sshpass -p '1' ssh -p 5555 rohitvm@localhost "echo 1 | sudo -S shutdown now"
	echo "waiting for shutdown to finish and port 5555 to free up"
	sleep 120	#wait for shutdown to finish and port 5555 to free up

	sudo umount $mount

done
