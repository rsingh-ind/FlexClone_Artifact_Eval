#!/bin/bash
blockSize=("4096")
cache=("coldCache")
threads=("1thread")
FS=("btrfs" "ext4" "xfs" "dcopy")
#FS=("Ext4" "ourExt4" "Ext4_raw")
#Op=("seqRead" "randRead" "seqWrite" "randWrite")
Op=("randWrite" "seqWrite")
dev="ssd"

for i in "${cache[@]}"
do
	plot_name="${dev}_${i}"
	touch $plot_name
	echo "#TP=Throughput, SD=Std.Deviation" >> "$plot_name"
	echo "#           btrfs        btrfs	ext4		ext4		xfs		xfs		dcopy		dcopy" >> "$plot_name"
	echo "#           TP          SD          TP         	SD          	TP          	SD		TP		SD" >> "$plot_name"

	for j in "${blockSize[@]}"
	do
		for k in "${Op[@]}"
		do
			if [ $k == "randWrite" ]
			then
				echo -n "rndWR" >> $plot_name
			elif [ $k == "seqWrite" ]
			then
				echo -n "seqWR" >> $plot_name
			else
				echo -n "Unknown Operation" >> $plot_name
			fi

			for l in "${FS[@]}"
			do
				#Eg: fioOut_8GB_seqWrite_coldCache_4096bs_Ext4_1thread_asynchOff_btrfs
				resultFile="fioOut_8GB_${k}_${i}_${j}bs_Ext4_1thread_asynchOff_${l}"
				echo "$resultFile"

				./extract_mean_throughput.sh $resultFile $plot_name 
			done
			printf "\n" >> "$plot_name"
		done
	done
done
