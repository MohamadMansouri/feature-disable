export output_dir_name="imageMagickCropButton"

source ../config.sh

program="/usr/local/bin/display"
traces=$traces_path/*
target="crop"


# Variables specific for tracing the program                #
############################################################# 
img_dir=$test"/test_imgs"
# imgs="nature.png laptop.tiff eagle.gif w3c_home.jpg"
imgs="laptop.tiff"


# Runs that execute the target feature                      #
############################################################# 
for i in ${!imgs[@]}; do
	$tracer -f $target -i "img"$i -r 1 -o $traces_path -- $program $img_dir/${imgs[$i]}
done


# Runs that DONT execute the target feature                 #
############################################################# 
for i in ${!imgs[@]}; do
	$tracer -f "other" -i "img"$i -r 1 -o $traces_path -- $program $img_dir/${imgs[$i]}
done


$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program $img_dir/${imgs[0]}
$patcher $program $test/results/$output_dir_name/identified
