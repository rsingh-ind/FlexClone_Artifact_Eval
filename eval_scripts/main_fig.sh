#!/bin/bash

if [ "$(id -u)" -ne 0 ] 
then
	echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

if [ $# != 2 ]
then
	echo "Usage: $0 <figure number> <iterations>"
	echo ""
	echo "Valid figure numbers: < 8, 9a, 9b, 10a, 10b, 11, 12, 13a, 13b, 1 >"
	echo "Iterations indicates number of times the experiment should be rerun before"
	echo "generating the mean and std. dev. results."
	echo ""
	echo "Eg: 	$0 8 10	//Run experiment corresponding figure 8 ten times"
	echo "	$0 13a 1	//Run experiment corresponding figure 13a only once"
	echo ""
	exit
fi

work_dir=""
if [ $1 == "8" ]
then
	echo "Fig 8 chosen"
	work_dir="fig8_clone_time"	
	./fig8_clone_time/main.sh $2 $work_dir | tee "$work_dir/log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "9a" ] 
then
	echo "Fig 9a chosen"
	work_dir="fig9a_throughput_block_aligned"	
	cd $work_dir
	./global_init_all_singlethread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "9b" ] 
then
	echo "Fig 9b chosen"
	work_dir="fig9b_throughput_non_block_aligned"	
	cd $work_dir
	./global_init_all_singlethread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"
	
elif [ $1 == "10a" ] 
then
	echo "Fig 10a chosen"
	work_dir="fig10a_multihreaded_randRead"	
	cd $work_dir
	./global_init_all_multithread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"
	
elif [ $1 == "10b" ] 
then
	echo "Fig 10b chosen"
	work_dir="fig10b_multihreaded_randWrite"	
	cd $work_dir
	./global_init_all_multithread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"
	
elif [ $1 == "11" ] 
then
	echo "Fig 11 chosen"
	work_dir="fig11_write_to_par"	
	cd $work_dir
	./global_init_all_singlethread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "12" ] 
then
	echo "Fig 12 chosen"
	work_dir="fig12_recovery_using_version_count"	
	cd $work_dir
	./auto.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "13a" ] 
then
	echo "Fig 13a chosen"
	work_dir="fig13a_cpu_util"	
	cd $work_dir
	./global_init_all_singlethread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "13b" ] 
then
	echo "Fig 13b chosen"
	work_dir="fig13b_cpu_util"	
	cd $work_dir
	./global_init_all_singlethread.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "15a" ] 
then
	echo "Fig 15a chosen"
	work_dir="fig15a_sqlite"	
	cd $work_dir
	./auto.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "15b" ] 
then
	echo "Fig 15b chosen"
	work_dir="fig15b_sqlite"	
	cd $work_dir
	./auto.sh $2 | tee "log_$(date +%Y%m%d_%H%M%S)"
else
	echo "Invalid figure number"
	echo "Valid figure numbers (comma separated list): 8, 9a, 9b, 10a, 10b, 11, 12, 13a, 13b, 15a, 15b"
fi
