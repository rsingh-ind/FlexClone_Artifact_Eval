#!/bin/bash

# Infinite loop to capture memory usage every 1 second
while true
do
    # Read MemTotal and MemFree from /proc/meminfo
    mem_total=$(grep -i MemTotal /proc/meminfo | awk '{print $2}')
    mem_free=$(grep -i MemFree /proc/meminfo | awk '{print $2}')
    
    # Calculate memory usage as percentage with precision (avoid truncating)
    mem_usage=$(echo "scale=4; ($mem_total - $mem_free) / $mem_total * 100" | bc)

    # Print the memory usage as a percentage with 2 decimal places
    printf "%% Memory Usage: %.2f\n" $mem_usage

    # Sleep for 1 second before repeating
    sleep 1
done
