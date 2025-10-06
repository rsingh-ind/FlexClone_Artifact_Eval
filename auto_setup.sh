#!/bin/bash

##########################
#Note: Run me as sudo
##########################
userid=$(id -u)
if [ $userid != 0 ]
then
	echo "*********************************"
	echo "Run me as root"
	echo "*********************************"
	exit -1;
fi

##########################
#Check parameters
##########################
if [[ $# != 2 ]]
then
	echo "Usage: $0 <device name. Eg: /dev/vdb>, /dev/nvme0n1 <device size in GB. Eg: 512>"
	exit -1
fi


############################################
#Make sure that device exists
############################################
if [[ ! -b $1 ]]
then
	echo "*********************************"
        echo "Device $1 doesnot exists"
	echo "*********************************"
        exit -1
fi



###########################################
#Make sure that device is large enough
###########################################
num_partitions=4
min_partition_size_GB=20

each_partition_size=$(($2/${num_partitions}))
echo "each partition will be of the size: $each_partition_size GB"

if [[ $each_partition_size -lt $min_partition_size_GB ]]
then
	echo "Each partition size might be too low to continue. Using a bigger device is recommended..."
fi


###########################################
#Delete partition table on the device
###########################################
echo "*****************************************"
echo "Deleting partition table on the device"
echo "*****************************************"
dd if=/dev/zero of=$1  bs=4096 count=1



###########################################
#Create fresh partitions on the device
###########################################
echo "*****************************************"
echo "Creating fresh partition table on the device"
echo "*****************************************"

#First 3 partitions are primary
for ((i=1;i<$num_partitions;i=$((i+1))))
do
echo $i
	(
	echo n # Add a new partition
	echo p # Primary partition
	echo $i # Partition number
	echo   # First sector (Accept default)
	echo +${each_partition_size}G  # Last sector
	sleep 5
	echo w # Write changes
	) | sudo fdisk $1
done

#4th partition is extended
(
echo n # Add a new partition
echo e # extended partition
#echo 4 # Partition number
echo   # First sector (Accept default)
echo   # Last sector (Accept default)
sleep 5
echo w # Write changes
) | sudo fdisk $1

#Create logical partition inside the extended partition
#Note: filesystem cannot be created on an extended partition
(
echo n # Add a new partition
#echo p # extended partition
#echo 4 # Partition number
echo   # First sector (Accept default)
echo   # Last sector (Accept default)
sleep 5
echo w # Write changes
) | sudo fdisk $1


#################################################
#Create filesystems on newly created partitions
#################################################
btrfs_mount_point="/btrfs"
btrfs_partition="${1}1"

ext4_mount_point="/ext4"
ext4_partition="${1}2"

flexclone_mount_point="/flexclone"
flexclone_partition="${1}3"

xfs_mount_point="/xfs"
xfs_partition="${1}5"

echo "*****************************************"
echo "Creating filesystems on newly created partitions"
echo "*****************************************"

echo "*****************************************"
echo "Btrfs"
echo "*****************************************"
mkfs.btrfs -f $btrfs_partition
if [ $? != 0 ]
then
	echo "Btrfs installation failed.."
	exit
fi
echo "*****************************************"
echo "Ext4"
echo "*****************************************"
mkfs.ext4 -FF $ext4_partition
if [ $? != 0 ]
then
	echo "Ext4 installation failed.."
	exit
fi
echo "*****************************************"
echo "Ext4"
echo "*****************************************"
mkfs.ext4 -FF $flexclone_partition
if [ $? != 0 ]
then
	echo "Ext4 installation failed.."
	exit
fi
echo "*****************************************"
echo "XFS"
echo "*****************************************"
mkfs.xfs -f -m reflink=1 $xfs_partition
if [ $? != 0 ]
then
	echo "XFS installation failed.."
	exit
fi

#################################################
#create mount points for the partitions
#################################################
echo "*****************************************"
echo "Creating mount points for the partitions"
echo "*****************************************"
mkdir -p $btrfs_mount_point
if [ $? != 0 ]
then
	echo "Failed to create btrfs mountpoint: /btrfs"
	exit
fi

mkdir -p $ext4_mount_point
if [ $? != 0 ]
then
	echo "Failed to create ext4 mountpoint: /ext4"
	exit
fi

mkdir -p $flexclone_mount_point
if [ $? != 0 ]
then
	echo "Failed to create flexclone mountpoint: /flexclone"
	exit
fi

mkdir -p $xfs_mount_point
if [ $? != 0 ]
then
	echo "Failed to create flexclone mountpoint: /xfs"
	exit
fi

#################################################
#Save information about partitions and mountpoints
#################################################
echo "*****************************************"
echo "Saving information about partitions and mountpoints.."
echo "*****************************************"

FILE="eval_scripts/mount_info.sh"
#Check if the file exists
if [ ! -f "$FILE" ]; then
    echo "Error: File '$FILE' not found!"
    exit 1
fi

SEARCH_STRING="BTRFS_SSD_MOUNT_POINT=.*"
REPLACE_STRING="BTRFS_SSD_MOUNT_POINT=\"${btrfs_mount_point}/\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="BTRFS_SSD_MOUNT_DEVICE=.*"
REPLACE_STRING="BTRFS_SSD_MOUNT_DEVICE=\"${btrfs_partition}\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="EXT4_SSD_MOUNT_POINT=.*"
REPLACE_STRING="EXT4_SSD_MOUNT_POINT=\"${ext4_mount_point}/\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="EXT4_SSD_MOUNT_DEVICE=.*"
REPLACE_STRING="EXT4_SSD_MOUNT_DEVICE=\"${ext4_partition}\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="DCOPY_SSD_MOUNT_POINT=.*"
REPLACE_STRING="DCOPY_SSD_MOUNT_POINT=\"${flexclone_mount_point}/\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="DCOPY_SSD_MOUNT_DEVICE=.*"
REPLACE_STRING="DCOPY_SSD_MOUNT_DEVICE=\"${flexclone_partition}\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="XFS_SSD_MOUNT_POINT=.*"
REPLACE_STRING="XFS_SSD_MOUNT_POINT=\"${xfs_mount_point}/\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

SEARCH_STRING="XFS_SSD_MOUNT_DEVICE=.*"
REPLACE_STRING="XFS_SSD_MOUNT_DEVICE=\"${xfs_partition}\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

#########################################################
#Update location of ext4+flexclone module 
#########################################################
FILE="eval_scripts/module_info.sh"
MODULE_PATH="$(pwd)/linux_5.5.10/"
SEARCH_STRING="module_path=.*"
REPLACE_STRING="module_path=\"${MODULE_PATH}\""
sed -i "s|$SEARCH_STRING|$REPLACE_STRING|g" "$FILE"

echo "Done.."
