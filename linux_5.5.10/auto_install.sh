#!/bin/bash

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit -1
fi

log="log"
>$log

echo "################################################"
echo "Running apt update command"
echo "Tentative time for completion: 2 mins"
echo "################################################"
yes | apt update 1>>$log
if [ $? != 0 ]
then
	echo "**** Failed to run `apt update` command ****"
	exit -1
fi

#Install dependencies
echo "################################################"
echo "Installing dependencies.."
echo "Tentative time for completion: 30 mins"
echo "################################################"
yes | apt install git bc binutils bison dwarves flex gcc git gnupg2 gzip libelf-dev libncurses5-dev libssl-dev make openssl pahole perl-base rsync tar xz-utils vim btrfs-progs fio qemu-system-x86 gnuplot-x11 texlive-font-utils git-lfs 1>>$log
if [ $? != 0 ]
then
	echo "**** Failed to install dependencies ****"
	exit -1
fi

cp mkfs.xfs /usr/sbin/mkfs.xfs
if [ $? != 0 ]
then
	echo "**** Failed to install mkfs.xfs ****"
	exit -1
fi

cp filebench /usr/local/bin/filebench
if [ $? != 0 ]
then
	echo "**** Failed to install filebench ****"
	exit -1
fi

yes | apt install ./jdk-21_linux-x64_bin.deb
if [ $? != 0 ]
then
	echo "**** Failed to install jdk21 ****"
	exit -1
fi

git lfs install


#Compile kernel
echo "################################################"
echo "Compiling kernel.."
echo "Tentative time for completion: 1-2 hours"
echo "################################################"
make -j$(($(nproc) + 1)) 1>>$log
if [ $? != 0 ]
then
	echo "**** Failed to compile kernel ****"
	exit -1
fi

#Install kernel modules
echo "################################################"
echo "Installing kernel modules.."
echo "Tentative time for completion: 10 mins"
echo "################################################"
make modules_install 1>>$log
if [ $? != 0 ]
then
	echo "**** Failed to install kernel modules ****"
	exit -1
fi

#Installing kernel 
echo "################################################"
echo "Installing kernel.."
echo "Tentative time for completion: 3 mins"
echo "################################################"
make install 1>>$log
if [ $? != 0 ]
then
	echo "**** Failed to install kernel ****"
	exit -1
fi

echo "################################################"
echo "Installation successful!"
echo "Please reboot the system.."
echo "################################################"
