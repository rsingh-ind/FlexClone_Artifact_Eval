#!/bin/bash

# Check argument count
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input_file> <output_file>"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found."
    exit 2
fi

# Total number of lines in input file
total_lines=$(wc -l < "$INPUT_FILE")

# Process: skip first 3 lines and last 2 lines
awk -v start=4 -v end=$((total_lines - 2)) 'NR >= start && NR <= end {
    idle = $NF;
    usage = 100 - idle;
    printf "%.2f\n", usage
}' "$INPUT_FILE" > "$OUTPUT_FILE"
