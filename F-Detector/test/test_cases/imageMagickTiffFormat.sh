export output_dir_name="imageMagickTiffFormat"

source ../config.sh

program="/usr/local/bin/display"
traces=$traces_path/*
target="tiff"


# Variables specific for tracing the program                #
############################################################# 
autoclose=True # automate closing the images (not recommended)
img_path="~/Others/libtiff/tiff-4.0.8/test/images"
img="rgb-3c-16b.tiff"


# Runs that execute the target feature                      #
############################################################# 
$tracer -f $target -i "tiffimg" -r 1 -o $traces_path -- $program $img_path/$img

# Runs that DONT execute the target feature                 #
############################################################# 
convert $img_path/$img "/tmp/"$img.png
$tracer -f "png" -i "tiffimg" -r 1 -o $traces_path -- $program "/tmp/"$img.png
rm "/tmp/"$img.png

$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program $img_path/$img
$patcher $program $test/results/$output_dir_name/identified
