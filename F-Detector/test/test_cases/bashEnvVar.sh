export output_dir_name="bashEnvVar"

source ../config.sh

program="~/Others/bash/bash2"
traces=$traces_path/*
target="envfunc"


# Variables specific for tracing the program                #
############################################################# 
functions=(":")
shdir=$test"/test_sh"
files="echo.sh"


# Runs that execute the target feature                      #
############################################################# 
x="() { :;}" $tracer -f $target -i "echo" -r 1 -o $traces_path -- $program $shdir/$files



# Runs that DONT execute the target feature                 #
############################################################# 
x="1" $tracer -f "envvar" -i "echo" -r 1 -o $traces_path -- $program $shdir/$files



$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
x="() { :;}" $retracer -d $output_path -- $program $shdir/$files
$patcher $program $test/results/$output_dir_name/identified
