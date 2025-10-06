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
	printf "Usage: $0 <output file> <file to which calculated values should be saved>\n"
	exit -1
fi

if [ ! -f $1 ] 
then
	#Error while calculating mean and std. dev.
	echo -n " Error Error " >> $2
	exit -1
fi

while IFS=, read -r line; do  
	total_time=$line
	total_time=$(bc -l <<< $total_time/1000)	#<------------ Convert time from ms to secs
	echo $total_time
	avg_values_array+=($total_time)
	global_sum_val=$(bc -l <<< $global_sum_val+$total_time)
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
	#echo $num
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
