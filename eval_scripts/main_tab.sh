#!/bin/bash

if [ "$(id -u)" -ne 0 ] 
then
	echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

if [ $# != 1 ]
then
	echo "Usage: $0 <table number>"
	echo ""
	echo "Valid table numbers: < 1, 3, 4, 5>"
	echo ""
	echo "Eg: 	$0 1 	//Run experiment corresponding table 1"
	echo "	$0 4	//Run experiment corresponding table 4"
	echo ""
	exit
fi

work_dir=""
if [ $1 == "1" ]
then
	echo "Table 1 chosen"
	work_dir="tab1_duplicate_caching"	
	cd $work_dir
	./main.sh | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "3" ] 
then
	echo "Table 3 chosen"
	work_dir="tab3_rand_write_overheads"	
	cd $work_dir
	./global_init_all_singlethread.sh 1 | tee "log_$(date +%Y%m%d_%H%M%S)"

elif [ $1 == "4" ] 
then
	echo "Table 4 chosen"
	work_dir="tab4_flexclone_modes"	
	cd $work_dir
	./global_init_all_singlethread.sh 1 | tee "log_$(date +%Y%m%d_%H%M%S)"
	
elif [ $1 == "5" ] 
then
	echo "Table 5 chosen"
	work_dir="tab5_mem_util"	
	cd $work_dir
	./global_init_all_singlethread.sh 1 | tee "log_$(date +%Y%m%d_%H%M%S)"
else
	echo "Invalid table number"
	echo "Valid table numbers: 1, 3, 4, 5"
fi
