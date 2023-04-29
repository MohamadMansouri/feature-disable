export output_dir_name="busyBoxWgetApp"

source ../config.sh

program=$test/"busybox"
traces=$traces_path/*
target="wget"


# Variables specific for tracing the program                #
############################################################# 
DUMMY=1


# Runs that execute the target feature                      #
############################################################# 
$tracer -f $target -i "dummy" -r 1 -o $traces_path -- $program $target



# Runs that DONT execute the target feature                 #
############################################################# 
$tracer -f "sleep" -i "dummy" -r 1 -o $traces_path -- $program "sleep"



$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program $target 
$patcher $program $test/results/$output_dir_name/identified