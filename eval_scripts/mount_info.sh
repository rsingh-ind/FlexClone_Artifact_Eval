#!/bin/bash

if [ $# != 3 ]
then
	echo "Usage: $0 <filesystem/directory name (btrfs/ext4/flexclone/xfs/nfs)> <device (hdd/ssd)> <what to print? mount point (0) or mount command (1) or umount command (2)>"
	echo "Eg: $0 btrfs ssd 1"
        exit -1
fi

##################################
# SSD
##################################
BTRFS_SSD_MOUNT_POINT="/btrfs/"
BTRFS_SSD_MOUNT_DEVICE="/dev/vdb1"

EXT4_SSD_MOUNT_POINT="/ext4/"
EXT4_SSD_MOUNT_DEVICE="/dev/vdb2"

DCOPY_SSD_MOUNT_POINT="/flexclone/"
DCOPY_SSD_MOUNT_DEVICE="/dev/vdb3"

XFS_SSD_MOUNT_POINT="/xfs/"
XFS_SSD_MOUNT_DEVICE="/dev/vdb5"

##################################
# mount, umount commands for SSD
##################################
BTRFS_SSD_MOUNT_CMD="sudo mount -t btrfs $BTRFS_SSD_MOUNT_DEVICE $BTRFS_SSD_MOUNT_POINT"
EXT4_SSD_MOUNT_CMD="sudo mount -t ext4 $EXT4_SSD_MOUNT_DEVICE $EXT4_SSD_MOUNT_POINT"
DCOPY_SSD_MOUNT_CMD="sudo mount -t ext4-module $DCOPY_SSD_MOUNT_DEVICE $DCOPY_SSD_MOUNT_POINT"
XFS_SSD_MOUNT_CMD="sudo mount -t xfs $XFS_SSD_MOUNT_DEVICE $XFS_SSD_MOUNT_POINT"

BTRFS_SSD_UMOUNT_CMD="sudo umount $BTRFS_SSD_MOUNT_POINT"
EXT4_SSD_UMOUNT_CMD="sudo umount $EXT4_SSD_MOUNT_POINT"
DCOPY_SSD_UMOUNT_CMD="sudo umount $DCOPY_SSD_MOUNT_POINT"
XFS_SSD_UMOUNT_CMD="sudo umount $XFS_SSD_MOUNT_POINT"


###############################
#btrfs
###############################
if [ $1 == "btrfs" ] && [ $2 == "ssd" ] && [ $3 == "0" ]
then
        echo $BTRFS_SSD_MOUNT_POINT

elif [ $1 == "btrfs" ] && [ $2 == "ssd" ] && [ $3 == "1" ]
then
        echo $BTRFS_SSD_MOUNT_CMD

elif [ $1 == "btrfs" ] && [ $2 == "ssd" ] && [ $3 == "2" ]
then
        echo $BTRFS_SSD_UMOUNT_CMD

###############################
#ext4
###############################
elif [ $1 == "ext4" ] && [ $2 == "ssd" ] && [ $3 == "0" ]
then
        echo $EXT4_SSD_MOUNT_POINT

elif [ $1 == "ext4" ] && [ $2 == "ssd" ] && [ $3 == "1" ]
then
        echo $EXT4_SSD_MOUNT_CMD

elif [ $1 == "ext4" ] && [ $2 == "ssd" ] && [ $3 == "2" ]
then
        echo $EXT4_SSD_UMOUNT_CMD

###############################
#flexclone
###############################
elif [ $1 == "flexclone" ] && [ $2 == "ssd" ] && [ $3 == "0" ]
then
        echo $DCOPY_SSD_MOUNT_POINT

elif [ $1 == "flexclone" ] && [ $2 == "ssd" ] && [ $3 == "1" ]
then
        echo $DCOPY_SSD_MOUNT_CMD

elif [ $1 == "flexclone" ] && [ $2 == "ssd" ] && [ $3 == "2" ]
then
        echo $DCOPY_SSD_UMOUNT_CMD

###############################
#XFS
###############################
elif [ $1 == "xfs" ] && [ $2 == "ssd" ] && [ $3 == "0" ]
then
        echo $XFS_SSD_MOUNT_POINT

elif [ $1 == "xfs" ] && [ $2 == "ssd" ] && [ $3 == "1" ]
then
        echo $XFS_SSD_MOUNT_CMD

elif [ $1 == "xfs" ] && [ $2 == "ssd" ] && [ $3 == "2" ]
then
        echo $XFS_SSD_UMOUNT_CMD
else
	echo "Error: Invalid argument"
fi
