#!/bin/bash
#blockSize=("4096" "3072")
blockSize=("4096")
#cache=("coldCache" "hotCache")
cache=("coldCache")
#threads=("1thread" "2thread" "4thread" "8thread" "16thread" "32thread")
threads=("1thread")
FS=("btrfs" "xfs" "flexclone")
#Op=("seqRead" "randRead" "seqWrite" "randWrite")
Op=("seqWrite")
dev="ssd"
#children=("1" "2" "4" "8" "16")
children=("1" "2" "4" "8")

for i in "${cache[@]}"
do
	for j in "${blockSize[@]}"
	do
		plot_name="${dev}_${i}_${j}bs"
		> $plot_name
		echo "#TP=Throughput, SD=Std.Deviation" >> "$plot_name"
		echo "#           btrfs         btrfs       ext4DC      ext4DC      XFS         XFS" >> "$plot_name"
		echo "#           TP            SD          TP          SD          TP          SD" >> "$plot_name"

		for k in "${Op[@]}"
		do
			for c in "${children[@]}"
			do
				echo -n "${c}" >> "$plot_name"

				for l in "${FS[@]}"
				do
					#Eg: fioOut_8GB_seqWrite_coldCache_4096bs_1thread_asynchOff_flexclone_ssd_16kids
					resultFile="fioOut_8GB_${k}_${i}_${j}bs_1thread_asynchOff_${l}_${dev}_${c}kids"
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
