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

#$qemu_binary -L pc-bios -enable-kvm -m $RAM_SIZE  -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22  -drive file=$disk_image,format=raw,if=none,id=drive-ide0-0-0,cache=writeback -drive file=/oldhome/rohit/FlexSnap_VM_disks/seethrough/32GB_disk,format=raw,if=virtio -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1

#$qemu_binary -nographic -L pc-bios -enable-kvm -m $RAM_SIZE  -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22  -drive file=$disk_image,format=raw,if=none,id=drive-ide0-0-0,cache=writeback -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive id=disk1,file=/oldhome/rohit/FlexSnap_VM_disks/seethrough/1GB,if=none,format=raw,readonly=on -device virtio-blk-pci,drive=disk1,serial=SHARED01 &

$qemu_binary -L pc-bios -enable-kvm -m $RAM_SIZE  -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22  -drive file=$disk_image,format=raw,if=none,id=drive-virtio0,cache=writeback -device virtio-blk-pci,drive=drive-virtio0,bootindex=1 -drive id=disk1,file=/oldhome/rohit/FlexSnap_VM_disks/seethrough/1GB,if=none,format=raw,readonly=on -device virtio-blk-pci,drive=disk1,serial=SHARED01 &

#$qemu_binary -L pc-bios -enable-kvm -m $RAM_SIZE  -smp $n_cpu -device e1000,netdev=hostnet0 -netdev user,id=hostnet0,hostfwd=tcp::$2-:22  -drive file=$disk_image,format=raw,if=none,id=drive-ide0-0-0,cache=writeback -device ide-hd,bus=ide.0,unit=0,drive=drive-ide0-0-0,id=ide0-0-0,bootindex=1 -drive id=disk1,file=/oldhome/rohit/FlexSnap_VM_disks/seethrough/32GB_disk,if=none,format=raw,readonly=on -device virtio-blk-pci,drive=disk1,serial=SHARED01


