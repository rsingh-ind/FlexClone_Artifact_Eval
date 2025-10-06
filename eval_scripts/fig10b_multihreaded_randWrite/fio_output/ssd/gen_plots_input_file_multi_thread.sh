#!/bin/bash
#blockSize=("4096" "3072")
blockSize=("4096")
#cache=("coldCache" "hotCache")
cache=("coldCache")
threads=("1thread" "2thread" "4thread" "8thread" "16thread" "32thread")
#threads=("1thread")
FS=("btrfs" "ext4" "flexclone" "xfs")
#Op=("seqRead" "randRead" "seqWrite" "randWrite")
Op=("randWrite")
dev="ssd"

for i in "${cache[@]}"
do
	for j in "${blockSize[@]}"
	do
		for k in "${Op[@]}"
		do
			plot_name="${dev}_${k}_${i}_${j}bs"
			> $plot_name
			echo "#TP=Throughput, SD=Std.Deviation" >> "$plot_name"
			echo "#Line 	Num      btrfs         btrfs       ext4        ext4        ext4DC      ext4DC      XFS         XFS" >> "$plot_name"
			echo "#num  threads     TP            SD          TP          SD          TP          SD          TP          SD" >> "$plot_name"

			iter=1
			for l in "${threads[@]}"
			do
				printf "%-8d" "$iter">> "$plot_name"
				l_clean="${l//[^0-9]/}"   # Remove all non-digit characters
				printf "%-2d" "$l_clean">> "$plot_name"
				((iter++))

				for m in "${FS[@]}"
				do
					#Example: fioOut_8GB_randWrite_coldCache_4096bs_16thread_asynchOff_btrfs_ssd
					resultFile="fioOut_8GB_${k}_${i}_${j}bs_${l}_asynchOff_${m}_${dev}"
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
done
