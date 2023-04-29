export output_dir_name="exiv2Insert"

source ../config.sh

program="~/Others/exiv2/build/bin/exiv2"
traces=$traces_path/*
target="insert"


# Variables specific for tracing the program                #
############################################################# 
imgfiles=$test/test_imgs

# Runs that execute the target feature                      #
############################################################# 
$tracer -f $target -i "colors" -r "1" -o $traces_path -- $program in $imgfiles"/colors.tiff"
$tracer -f $target -i "colors" -r "2" -o $traces_path -- $program -i a $imgfiles"/colors.tiff"



# Runs that DONT execute the target feature                 #
############################################################# 
$tracer -f "print" -i "colors" -r "1" -o $traces_path -- $program pr $imgfiles"/colors.tiff"
$tracer -f "remove" -i "colors" -r "1" -o $traces_path -- $program -P E $imgfiles"/colors.tiff"


# echo $output_path
$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program in $imgfiles"/colors.tiff"
$patcher $program $test/results/$output_dir_name/identified
