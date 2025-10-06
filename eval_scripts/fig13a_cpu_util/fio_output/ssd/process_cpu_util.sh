#!/bin/bash

fs=("btrfs" "xfs" "ext4" "flexclone")
#op=("seqWrite" "randWrite")
op=("seqWrite")

for i in ${op[@]}
do
	for j in ${fs[@]}
	do
		./process_cpu_util_helper.sh cpuUtil_fioOut_16GB_${i}_coldCache_4096bs_1thread_asynchOff_${j}_ssd cpuUtil_fioOut_16GB_${i}_coldCache_4096bs_1thread_asynchOff_${j}_ssd_processed
	done
done
