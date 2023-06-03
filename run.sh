#!/bin/bash

# Path to your shell script file
script_file="./nsl_cluster_setup.sh"

# Read and execute lines from the script file
line_number=0
while IFS= read -r line; do
    ((line_number++))
    echo "Line $line_number: $line"
    eval "$line"
    return_code=$?
    if [ $return_code -ne 0 ]; then
        echo "Error executing line $line_number: $line"
        exit $return_code
    fi
done < "$script_file"
