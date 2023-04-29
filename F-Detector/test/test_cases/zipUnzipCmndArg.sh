export output_dir_name="zipUnzipCmnd"

source ../config.sh

program=$test"/zip"
traces=$traces_path/*
target="unzipcmnd"


# Variables specific for tracing the program                #
############################################################# 


# Runs that execute the target feature                      #
############################################################# 
echo "Hello World" > "/tmp/file.txt" &>/dev/null &
$tracer -f $target -i "file0" -r 1 -o $traces_path -- $program "/tmp/tmp.zip" "/tmp/file.txt" -T --unzip-command="echo hello from zip"
rm "/tmp/tmp.zip" &>/dev/null &
rm "/tmp/file.txt" &>/dev/null &



# Runs that DONT execute the target feature                 #
############################################################# 
echo "Hello World" > "/tmp/file.txt"
$tracer -f "execlude" -i "file0" -r 1 -o $traces_path -- $program "/tmp/tmp.zip" "/tmp/file.txt" -T --unicode="escape"
rm "/tmp/tmp.zip" &>/dev/null &
rm "/tmp/file.txt" &>/dev/null &


$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program "/tmp/tmp.zip" "/tmp/file.txt" -T --unzip-command="echo hello from zip"
echo "Hello World" > "/tmp/file.txt"
rm "/tmp/tmp.zip" &>/dev/null &
rm "/tmp/file.txt" &>/dev/null &
$patcher $program $test/results/$output_dir_name/identified
