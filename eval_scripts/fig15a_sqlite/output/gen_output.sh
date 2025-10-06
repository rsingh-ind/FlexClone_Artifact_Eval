#!/bin/bash
#filesystem=("ext4" "dcopy" "btrfsVanilla" "btrfsModified" "xfsVanilla" "xfsModified")
#filesystem=("ext4" "dcopy" "xfsVanilla" "xfsModified")
filesystem=("ext4" "dcopy")
workload=("tpcc" "epinions" "twitter")

plot_name="sqlite_summary"
touch $plot_name
echo "# M=mean, SD=std. dev." >> "$plot_name"
echo "#				Ext4	Ext4	ourExt4		ourExt4" >> "$plot_name"
echo "#				M	SD	M		SD" >> "$plot_name"
echo "#Units: ops/sec" >> "$plot_name"
echo "" >> "$plot_name"

for wl in ${workload[@]}
do
	printf "%24s" $wl >> "$plot_name"
	for fs in ${filesystem[@]}
	do
		if [ $fs == "ext4" ] || [ $fs == "dcopy" ]
		then
			#sample input file name: out_ext4_twitter
			file="out_${fs}_${wl}"
		elif [ $fs == "btrfsVanilla" ]
		then
			#sample input file name: out_btrfs_epinions_vanillaSqlite 
			file="out_btrfs_${wl}_vanillaSqlite"
			./extract_mean_throughput.sh $file $plot_name
		elif [ $fs == "btrfsModified" ]
		then
			#sample input file name: out_btrfs_epinions_modifiedSqlite 
			file="out_btrfs_${wl}_modifiedSqlite"
		elif [ $fs == "xfsVanilla" ]
		then
			#sample input file name: out_xfs_epinions_vanillaSqlite 
			file="out_xfs_${wl}_vanillaSqlite"
		elif [ $fs == "xfsModified" ]
		then
			#sample input file name: out_xfs_epinions_modifiedSqlite 
			file="out_xfs_${wl}_modifiedSqlite"
		fi

		if [ ! -e "$file" ] || [ ! -s "$file" ]; then
			echo "Error: File $file does not exist or is empty"
			continue
		fi
		./extract_mean_throughput.sh $file $plot_name
	done
	printf "\n" >> "$plot_name"
done

