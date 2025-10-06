#!/bin/sh

if [ $# != 2 ]
then
	echo "Usage: $0 <disk image Eg: base-image-full.img> <port num Eg: 5555>"
	exit -1
fi

qemu_binary=qemu-system-x86_64  # path to qemu binary
disk_image="$1"  # path to operating system disk image
RAM_SIZE=4096 # RAM size for VM in MB.
n_cpu=8  #number of cpu to be used in VM.

# echo "ssh -p 5555 <user>@localhost"

#$qemu_binary -L pc-bios -enable-kvm -m $RAM_SIZE -boot c -hda $disk_image -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::5555-:22  &
#qemu-system-x86_64 -L pc-bios -enable-kvm -m 16384 -smp 8 -drive file=$1,format=raw,cache=none -boot c  -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22
#qemu-system-x86_64 -L pc-bios -enable-kvm -m 16384 -smp 8 -drive file=$1,format=raw,cache=none -boot c -device virtio-net,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22
qemu-system-x86_64 -L pc-bios -enable-kvm -m $RAM_SIZE -smp $n_cpu -drive file=$1,cache=writeback -boot c -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22
