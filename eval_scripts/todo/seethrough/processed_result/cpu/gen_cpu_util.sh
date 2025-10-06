#!/bin/bash

# Check if input file is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <filename>"
  exit 1
fi

# Read the file line by line
while IFS= read -r number; do
  if [[ -n "$number" ]]; then
    echo "100 - $number" | bc -l
  fi
done < "$1"
