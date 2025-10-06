#!/bin/bash
#blockSize=("4096" "3072")
blockSize=("4096")
#cache=("coldCache" "hotCache")
cache=("coldCache")
#threads=("1thread" "2thread" "4thread" "8thread" "16thread" "32thread")
threads=("1thread")
FS=("btrfs" "ext4" "flexclone" "xfs")
Op=("seqRead" "randRead" "seqWrite" "randWrite")
dev="ssd"

for i in "${cache[@]}"
do
	for j in "${blockSize[@]}"
	do
		plot_name="${dev}_${i}_${j}bs"
		> $plot_name
		echo "#TP=Throughput, SD=Std.Deviation" >> "$plot_name"
		echo "#           btrfs         btrfs       ext4        ext4        ext4DC      ext4DC      XFS         XFS" >> "$plot_name"
		echo "#           TP            SD          TP          SD          TP          SD          TP          SD" >> "$plot_name"

		for k in "${Op[@]}"
		do
			if [ $k == "seqRead" ]
			then
				echo -n "seqRD " >> "$plot_name"
			fi
			if [ $k == "randRead" ]
			then
				echo -n "randRD" >> "$plot_name"
			fi
			if [ $k == "seqWrite" ]
			then
				echo -n "seqWR " >> "$plot_name"
			fi
			if [ $k == "randWrite" ]
			then
				echo -n "randWR" >> "$plot_name"
			fi

			for l in "${FS[@]}"
			do
				#Eg: fioOut_8GB_seqWrite_hotCache_4096bs_1thread_asynchOff_xfs_ssd
				resultFile="fioOut_8GB_${k}_${i}_${j}bs_1thread_asynchOff_${l}_${dev}"
				if [ ! -f "$resultFile" ]
				then
					echo "Error: $0 , result file $resultFile not found!"
					continue
				fi

				./extract_mean_throughput.sh $resultFile $plot_name 
			done
			printf "\n" >> "$plot_name"
		done
	done
done
