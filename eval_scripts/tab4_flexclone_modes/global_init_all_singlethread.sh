#!/bin/bash

if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

if [ $# != 1 ]
then
        echo "Usage: $0 <iterations>"
        echo ""
        echo "Iterations indicates number of times the experiment should be rerun before"
        echo "generating the mean and std. dev. results."
        echo ""
        exit
fi

pwd

echo "================================"
echo "FlexClone started"
echo "================================"
./global_init_ourExt4_ssd.sh $1
if [ $? != 0 ]
then
        exit -1
fi

cd fio_output/ssd
./summarize_see_through_cold_cache.sh
cp summary ../..
