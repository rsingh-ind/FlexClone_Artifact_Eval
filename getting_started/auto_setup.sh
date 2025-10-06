#!/bin/bash

#############################################################################################
#Note: 
#	1) Run this script as root and as a background process i.e. "$./script &"
#	2) Directory where parent file (8GB) is present should contain clean.sh (deletes old child and friend files) and setxattr binary
#################################################################################################

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

curdir=$(pwd)
dev="ssd"
fs="flexclone"
module_path=$(../eval_scripts/module_info.sh)

#mount point
mount=$(../eval_scripts/mount_info.sh $fs $dev 0)
if [ "$mount" == "/" ]; then
        echo "Error: $0 Aborting..Somehow mount point is '/' instead of the desired mount point"
        exit
fi

#Prepare setup
echo "$0 unmounting $fs from $dev.."
mount_point=$(../eval_scripts/mount_info.sh $fs $dev 0)
mountpoint -q "$mount_point"
status=$?
if [ $status -eq 0 ]; then
    umount "$mount_point"
    umount_status=$?
    if [ $umount_status -ne 0 ]; then
	echo "Error: $0 unmounting $mount_point failed..skipping this device.."
	exit
    fi
fi


#fresh mount
cd $module_path
sudo ./remove_ext4_module.sh
sudo ./insert_ext4_module.sh
cd $curdir
mount_cmd=$(../eval_scripts/mount_info.sh $fs $dev 1)
$($mount_cmd)
if [ $? != 0 ]
then
        echo "Error: $0 mounting failed..skipping this device.."
        exit
fi

#clean mount point
cd $mount
rm -r *

#prepare source file
dest="${mount}"
if [ "$dest" == "/" ]; then
        echo "Error: $0 Aborting..Somehow dest is '/' instead of the desired dest"
        exit
fi

cd $dest
output="par"
tmp="par.tmp"
echo "Generating 1MB source file..."
dd if=/dev/urandom of=$tmp bs=1M count=1

#Convert file contents to ASCII characters
echo "Converting source file contents to ASCII format...please wait for 1-2 mins.."
# Allowed characters: space + newline + A-Z + 0-9
allowed_chars=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789
"

# Map bytes to allowed chars (0 to 37)
od -An -v -t u1 $tmp | tr -s ' ' '\n' | \
while read -r byte; do
  index=$(( byte % 38 ))
  # Extract character at index
  printf "%s" "${allowed_chars:index:1}"
done > "$output"

truncate -s 1M $output

rm $tmp
cd $curdir
cp auto_clean.sh $dest
cp auto_clone.sh $dest
cp setxattr_generic $dest
cp write $dest

echo "********* Setup done *********"
echo "Enter ${mount} to play around with FlexClone"
echo "****************************"
