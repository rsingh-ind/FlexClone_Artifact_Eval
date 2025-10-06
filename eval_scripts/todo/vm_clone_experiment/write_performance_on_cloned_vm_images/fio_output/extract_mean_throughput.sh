#!/bin/bash

global_ctr=0	#number of times experiment has been repeated
global_sum_val=0 #sum of averages across all experiments
global_avg_val=0 #mean i.e. average value obtained after dividing global_sum_val by global_ctr

declare -a avg_values_array	#store avg. throughput values in array. This array is used to calculate std. dev.
std_dev=0	#std. deviation of avg. throughput values
tmp_sum=0

num_threads=0	

if [[ $# != 2 ]]
then
	printf "Usage: $0 <fio output file> <file to which calculated values should be saved>\n"
	exit -1
fi

if [ ! -f $1 ] 
then
	#Error while calculating mean and std. dev.
	echo -n " Error Error " >> $2
	exit -1
fi


#Extract the number of fio threads performing the operation
num_threads=$(head -n 10 $1 | grep "thread" | cut -d " " -f 2)
echo $num_threads


#Sample line of throughput
#WRITE: bw=747MiB/s (784MB/s), 747MiB/s-747MiB/s (784MB/s-784MB/s), io=8192MiB (8590MB), run=10963-10963msec
#READ: bw=38.9MiB/s (40.8MB/s), 38.9MiB/s-38.9MiB/s (40.8MB/s-40.8MB/s), io=8192MiB (8590MB), run=210709-210709msec
#
while IFS=, read -r line; do  

	#skip lines that donot contain throughput
	if [[ $line != *"READ"* ]] && [[ $line != *"WRITE"* ]]; then
		continue
	fi
	#echo $line

	#Parse throughput line
	#Breakdown line into parts by using ' ' (space) as separater
	#Line will be spit into 8 parts
	#We are interested in second element
	IFS=' ' read -r operation throughput a b c d e f <<< $line
	echo $throughput

	#extract thoughput value + units
	IFS='=' read -ra throughput_array <<< $throughput
	throughput_value_with_units=${throughput_array[1]}
	echo $throughput_value_with_units

	#extract thoughput value excluding units
	#extracted throughput should be in MiBps
	if [[ $throughput == *"GiB"* ]]; then
		IFS='G' read -ra throughput_array <<< $throughput_value_with_units
		throughput_value=${throughput_array[0]}
		throughput_value=$(bc -l <<< ${throughput_array[0]}*1024)
	elif [[ $throughput == *"MiB"* ]]; then
		IFS='M' read -ra throughput_array <<< $throughput_value_with_units
		throughput_value=${throughput_array[0]}
	elif [[ $throughput == *"KiB"* ]]; then
		IFS='K' read -ra throughput_array <<< $throughput_value_with_units
		throughput_value=${throughput_array[0]}
		throughput_value=$(bc -l <<< ${throughput_array[0]}/1024)
	fi
	echo $throughput_value

	throughput_value=$(bc -l <<< $throughput_value/$num_threads)
	echo $throughput_value

	avg_values_array+=($throughput_value)

	global_sum_val=$(bc -l <<< $global_sum_val+$throughput_value)
	global_ctr=$(bc -l <<< $global_ctr+1)
done < $1;

#printf "%s" "$1" >> $2; 

#Find mean
global_avg_val=$(bc -l <<< $global_sum_val/$global_ctr)
#printf " Mean (MiBps): %s" "$global_avg_val" >> $2; 
printf "%12.2f" "$global_avg_val" >> $2; 

#Finding std. deviation
tmp_sum=0
for num in "${avg_values_array[@]}"
do
	#difference = every number minus mean
	#Here, we are finding square of each difference
	tmp=`echo "($global_avg_val-$num)^2" | bc`
	#echo "square of each difference: $tmp"

	#Add up the squared differences
	tmp_sum=$(bc -l <<< $tmp_sum+$tmp)
	#echo "Squared differences: $tmp_sum"
done

#sum of the squared differences / total elements
variance=$(bc -l <<< $tmp_sum/$global_ctr)
#echo $variance

#square root of the variance
std_dev=$(echo "scale=2; sqrt($variance);" | bc)
#printf " Std. dev (MiBps): %s" "$std_dev" >> $2; 
printf "%12.2f" "$std_dev" >> $2; 

#printf "\n" >> $2; 
