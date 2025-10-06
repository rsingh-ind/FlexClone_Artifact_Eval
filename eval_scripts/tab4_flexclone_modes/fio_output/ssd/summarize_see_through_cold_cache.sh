#!/bin/bash

see_through=(	
		"fioOut_8GB_randRead_coldCache_3072bs_asynchOff_flexclone_ssd"
		"fioOut_8GB_randWrite_coldCache_3072bs_asynchOff_flexclone_ssd"
		"fioOut_8GB_randReadWrite_coldCache_3072bs_asynchOff_flexclone_ssd"
	)

output_file="summary"
> $output_file

j=0
echo "#Reporting throughput(T) and std. dev. (SD)" >> $output_file
echo "#Throughput: MBps" >> $output_file
printf %s "     ourExt4   ourExt4     ourExt4	ourExt4" >> $output_file
echo "" >> $output_file
printf %s "     See-through_Read	CoW+Shared_Write	See-through+CoW+Shared_Read	See-through+CoW+Shared_Write" >> $output_file
echo "" >> $output_file
printf %s "     T   SD	  T  SD       T   SD	  T  SD" >> $output_file
echo "" >> $output_file
for i in ${see_through[@]}
do
	if [ $j == "0" ]
	then
		./extract_mean_throughput_readers.sh $i $output_file
	elif [ $j == "1" ]
	then
		./extract_mean_throughput_writers.sh $i $output_file
	elif [ $j == "2" ]
	then
		./extract_mean_throughput_readers.sh $i $output_file
		./extract_mean_throughput_writers.sh $i $output_file
	fi
	j=$(($j+1))
done
echo "" >> $output_file
