#!/bin/bash

file_recovered=("0" "1000" "2000" "3000" "4000" "5000" "6000" "7000" "8000" "9000" "10000")

plot_name="recovery_time_summary"
touch $plot_name
echo "#Files_Recovered    Mean	Stddev" >> "$plot_name"
echo "#Units: secs" >> "$plot_name"
echo "" >> "$plot_name"

i=1
j=1
for f in ${file_recovered[@]}
do
	: '
	if [ $(($j%2)) == 0 ]
	then
		j=$(($j+1))
		continue;
	fi
	'
	printf "$i" >> "$plot_name"
	printf "%12s" "$f" >> "$plot_name"
	i=$(($i+1))
	j=$(($j+1))

	result_file="recovering_${f}_files_ssd"	
	./extract_mean_throughput.sh $result_file $plot_name
	printf "\n" >> "$plot_name"
done

