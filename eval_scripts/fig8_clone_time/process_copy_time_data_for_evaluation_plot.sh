#!/bin/bash

device=("ssd")
file_sizes=("4K" "256K" "4M" "32M" "256M" "1G" "4G" "16G")

#In evaluation plot we don't show sendfile results
copy_time_raw_data_files=("copy_time_raw_data_btrfs_" "copy_time_raw_data_ext4_" "copy_time_raw_data_flexclone_" "copy_time_raw_data_xfs_" "copy_time_raw_data_qemu_qcow2_")

for dev in ${device[@]}
do
	i=1
	plot_name=""$dev"_copying_file_data_evaluation"
	> $plot_name
	echo "#Filesize    Btrfs Avg       Btrfs Stddev   ext4 avg       ext4 stddev     flexclone avg	flexclone stddev       XFS avg         XFS stddev	Qemu avg	Qemu stddev" >> "$plot_name"
	echo "#Units: ms" >> "$plot_name"
	echo "" >> "$plot_name"

	for fsize in ${file_sizes[@]}
	do
		printf "$i" >> "$plot_name"
		printf "%12s" "$fsize" >> "$plot_name"
		i=$(($i+1))

		for copy_time_file in ${copy_time_raw_data_files[@]}
		do
			copy_time_file="$copy_time_file$dev"
			if [ ! -f $copy_time_file ] 
			then
				echo "Error: $0, result file $copy_time_file not found!"
				continue
			fi
			if [ ! -s "$copy_time_file" ]; 
			then
				echo "Error: $0, result file $copy_time_file is empty!"
				continue
			fi
			#printf "$0, processing result file $copy_time_file\n"
			./extract_mean_throughput.sh $copy_time_file $fsize $plot_name
		done
		printf "\n" >> "$plot_name"
	done
done

