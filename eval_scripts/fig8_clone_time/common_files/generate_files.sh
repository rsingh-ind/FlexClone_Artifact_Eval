#!/bin/bash
#Reference: https://askubuntu.com/questions/1080113/measuring-execution-time-of-a-command-in-milliseconds

KB_files=("4" "256")
MB_files=("4" "32" "256")
GB_files=("1" "4" "16")

for i in "${KB_files[@]}"
do
	echo -e "\nGenerating ${i}KB source file.."
	dd if=/dev/urandom of="${i}KB" bs=1K count=$i
done

for i in "${MB_files[@]}"
do
	echo -e "\nGenerating ${i}MB source file.."
	dd if=/dev/urandom of="${i}MB" bs=1M count=$i
done

for i in "${GB_files[@]}"
do
	echo -e "\nGenerating ${i}GB source file.."
	num_blocks=$(($i*1024))
	dd if=/dev/urandom of="${i}GB" bs=1M count=$num_blocks
done
