#!/bin/bash

# Define the input files and output file
file1="cpuUtil_fioOut_16GB_seqWrite_coldCache_4096bs_1thread_asynchOff_btrfs_ssd_processed"
file2="cpuUtil_fioOut_16GB_seqWrite_coldCache_4096bs_1thread_asynchOff_xfs_ssd_processed"
file3="cpuUtil_fioOut_16GB_seqWrite_coldCache_4096bs_1thread_asynchOff_ext4_ssd_processed"
file4="cpuUtil_fioOut_16GB_seqWrite_coldCache_4096bs_1thread_asynchOff_flexclone_ssd_processed"
output_file="all_seqWrite_cpu_util"

# Check if the input files exist
if [[ ! -f $file1 || ! -f $file2 || ! -f $file3  || ! -f $file4 ]]; then
  echo "One or more input files do not exist."
  exit 1
fi

# Clear or create the output file
> "$output_file"
echo -e "#Time\tBtrfs\tXFS\tExt4\tFlexClone" >> "$output_file"

# Read lines from each file and write them in the desired order
for ((i = 1; i <= 10; i++)); do
  # Read the line from each file (using `sed` to get the i-th line)
  line1=$(sed -n "${i}p" "$file1")
  line2=$(sed -n "${i}p" "$file2")
  line3=$(sed -n "${i}p" "$file3")
  line4=$(sed -n "${i}p" "$file4")
  
  # Write the lines into the output file
  j=$(($i-1))
  echo -e "${j}\t$line1\t$line2\t$line3\t$line4" >> "$output_file"
done

echo "Merging complete. Output written to $output_file"
