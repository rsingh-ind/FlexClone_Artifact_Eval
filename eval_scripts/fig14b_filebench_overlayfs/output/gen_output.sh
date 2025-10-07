#!/bin/bash
filesystem=("btrfs" "ext4" "xfs" "flexclone")
workload=("micro_append" "fileserver" "varmail")

plot_name="ssd_out_filebench_child_ops_per_sec"
>$plot_name
echo "# M=mean, SD=std. dev." >> "$plot_name"
echo "#		Btrfs		Btrfs		Ext4	Ext4	XFS		XFS	FlexClone	FlexClone" >> "$plot_name"
echo "#		M		SD		M	SD	M	    SD		M		SD" >> "$plot_name"
echo "#Units: ops/sec" >> "$plot_name"
echo "" >> "$plot_name"

for wl in ${workload[@]}
do
	printf "%12s" $wl >> "$plot_name"
	for fs in ${filesystem[@]}
	do
		#sample input file name: btrfs_fileserver_stage2.f
		file="${fs}_${wl}_stage2.f"
		if [ ! -f $file ] || [ ! -s $file ]; then
			echo "Error: $file does not exist or is empty"
			continue
		fi

		./extract_mean_throughput.sh $file $plot_name
	done
	printf "\n" >> "$plot_name"
done

