#!/bin/bash

# Check arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input-file> <output-file>"
    exit 1
fi

input_file="$1"
output_file="$2"

# Total number of lines in the file
total_lines=$(wc -l < "$input_file")

# Calculate line range (after skipping 3 from start, 2 from end)
start_line=1
end_line=$((total_lines))

# Extract and write to output file
awk "NR >= $start_line && NR <= $end_line { print \$4 }" "$input_file" > "$output_file"
