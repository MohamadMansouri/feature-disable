project="/feat_removal"

if [ -z $output_dir_name ]; then 
    output_dir_name="UNSPECIFIED"_$RANDOM
fi


identification="$project/identification"
test="$project/test"
storage_path=$test"/results"
mkdir -p $storage_path
pin="$project/pin/pin"

output_path="$storage_path/$output_dir_name"
traces_path="$storage_path/$output_dir_name/traces"
mkdir -p $traces_path

tracer="$pin -t $identification/bin/pin-tracer.so"
profiler="$identification/bin/profiler"
merge="$identification/bin/merge"
identifier="$identification/bin/identifier"
retracer="$pin -t $identification/bin/pin-retracer.so"
patcher="$project/patch.py"