#!/bin/bash
function diffs() {
    diff "${@:3}" <(sort sample_output/gantt-"$1"-input_"$2") <(sort output/gantt-"$1"-input_"$2"); 
}

for cpu_type in {0..2}; do
    for sample_input_num in {0..11}; do
        # Output sample_input_num and cpu_type
        echo "Testing sample input $sample_input_num with cpu type $cpu_type"
        for i in {1..1000}; do
            # don't allow proj1 to output to stdout
            ./proj1 "$cpu_type" sample_input/input_"$sample_input_num" > /dev/null
            diffs "$cpu_type" "$sample_input_num"
        done
    done
done
