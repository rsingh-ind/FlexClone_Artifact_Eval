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
	printf "Usage: $0 <file containing throughput> <file to which calculated values should be saved>\n"
	exit -1
fi

if [ ! -f $1 ] 
then
	echo " Error "
	exit -1
fi


#Sample line of throughput
#
#Recovery_time_us: 1638
#Recovery_time_us: 1682
#
while IFS=, read -r line; do  

	#skip lines that donot contain data of interest
	if [[ $line != *"throughput"* ]] 
	then
		continue
	fi
	#echo $line

	#Parse line
	#Breakdown line into parts by using '=' as separater
	#Line will be spit into 5 parts
	#We are interested in fifth element
	IFS='=' read -r str1 str2 str3 str4 str5 <<< $line

	#Parse line
	#Breakdown line into parts by using ' ' (space) as separater
	IFS=' ' read -r throughput string2 string3 string4 string5 string6 <<< $str5
	echo $throughput

	avg_values_array+=($throughput)
	global_sum_val=$(bc -l <<< $global_sum_val+$throughput)
	global_ctr=$(bc -l <<< $global_ctr+1)
done < $1;

#Find mean
global_avg_val=$(bc -l <<< $global_sum_val/$global_ctr)
#echo "global sum: $global_sum_val"
#echo "global ctr: $global_ctr"
#echo "global avg: $global_avg_val"
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
printf "%12.2f" "$std_dev" >> $2; 
#echo "std_dev: $std_dev"

#printf "\n" >> $2; 

