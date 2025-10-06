#!/bin/sh

if [ $# != 1 ]
then
	echo "Syntax: $0 <disk image name Eg: base-image-full.img>"
	echo "Assumes, $0 and disk image are in same directory"
	exit 1
fi


qemu_binary=qemu-system-x86_64  # path to qemu binary
disk_image=$1  # path to operating system disk image
RAM_SIZE=16384 # RAM size for VM in MB.
n_cpu=1  #number of cpu to be used in VM.

# echo "ssh -p 5555 <user>@localhost"

#$qemu_binary -L pc-bios -enable-kvm -m $RAM_SIZE -boot c -hda $disk_image -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::5555-:22  &
$qemu_binary -nographic -L pc-bios -enable-kvm -m $RAM_SIZE -boot c -hda $disk_image -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::5555-:22  &
