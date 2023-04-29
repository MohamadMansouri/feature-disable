export output_dir_name="<program_name>"

source ../config.sh

program="<path/to/program_executable>"
traces=$traces_path/*
target="<name_of_target_feature>"


# Variables specific for tracing the program                #
############################################################# 
DUMMY=1


# Runs that execute the target feature                      #
############################################################# 
$tracer -f $target -i "<instance_name>" -r "<run_number>" -o $traces_path -- $program "<program_arguments>"



# Runs that DONT execute the target feature                 #
############################################################# 
$tracer -f "Other_feature_name" -i "<instance_name>" -r "<run_number>" -o $traces_path -- $program "<program_arguments>"



$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program "<program_argument_that_execute_target_feature>" 
$patcher $program $test/results/$output_dir_name/identified
