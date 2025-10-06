#!/bin/bash

# Define the input files and output file
file1="btrfs_mem_usage"
file2="xfs_mem_usage"
file3="flexsnap_mem_usage"
output_file="all_mem_usage"

# Check if the input files exist
if [[ ! -f $file1 || ! -f $file2 || ! -f $file3 ]]; then
  echo "One or more input files do not exist."
  exit 1
fi

# Clear or create the output file
> "$output_file"

# Read lines from each file and write them in the desired order
for ((i = 1; i <= 600; i++)); do
  # Read the line from each file (using `sed` to get the i-th line)
  line1=$(sed -n "${i}p" "$file1")
  line2=$(sed -n "${i}p" "$file2")
  line3=$(sed -n "${i}p" "$file3")
  
  # Write the three lines into the output file
  j=$(($i-1))
  echo -e "${j}\t$line1\t$line2\t$line3" >> "$output_file"
done

echo "Merging complete. Output written to $output_file"
