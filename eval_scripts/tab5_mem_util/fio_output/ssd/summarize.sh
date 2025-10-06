#!/bin/bash

file1="memUtil_fioOut_16GB_randWrite_coldCache_4096bs_32thread_asynchOff_btrfs_ssd_processed"
file2="memUtil_fioOut_16GB_randWrite_coldCache_4096bs_32thread_asynchOff_xfs_ssd_processed"
file3="memUtil_fioOut_16GB_randWrite_coldCache_4096bs_32thread_asynchOff_ext4_ssd_processed"
file4="memUtil_fioOut_16GB_randWrite_coldCache_4096bs_32thread_asynchOff_flexclone_ssd_processed"

if [ ! -f "$file1" ] || [ ! -f "$file2" ] || [ ! -f "$file3" ] || [ ! -f "$file4" ]
then
    echo "One or both files do not exist."
    exit -1
fi


# Function to find max value in a file
max () {
    local input_file="$1"
    local max_val=-999999999

    while read -r line; do
        # Skip empty lines
        [ -z "$line" ] && continue

        if (( $(echo "$line > $max_val" | bc -l) )); then
            max_val="$line"
        fi
    done < "$input_file"

    echo "$max_val"
}

# Get max from each file
max1=$(max "$file1")
#echo "max1: $max1"

max2=$(max "$file2")
#echo "max2: $max2"

max3=$(max "$file3")
#echo "max3: $max3"

max4=$(max "$file4")
#echo "max4: $max4"

out="summary"
# Print header
printf "#%-12s %-12s %-12s %-12s\n" "Btrfs" "XFS" "Ext4" "FlexClone"  > $out

# Print max values
printf "%-12s %-12s %-12s %-12s\n" "$max1" "$max2" "$max3" "$max4"  >> $out
