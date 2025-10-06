#!/bin/bash

#Path to base vm image
if [ $# != 1 ]
then
	echo "Usage: $0 <experiment (0: qcow2 comparison experiment, 1: hot cache experiment, 2: vm migration experiment)"
	exit -1
fi

if [ $1 == 0 ]
then
	base_vm_img_path="/oldhome/rohit/FlexSnap_VM_disks/vm_cloning"
elif [ $1 == 1 ]
then
	base_vm_img_path="/oldhome/rohit/origExt4/qemu-image-full-hot-cache/"
elif [ $1 == 2 ]
then
	base_vm_img_path="/oldhome/rohit/origExt4/qemu-image-full-migration/"
else
	echo "Invalid experiment number"
	exit -1
fi

echo $base_vm_img_path

