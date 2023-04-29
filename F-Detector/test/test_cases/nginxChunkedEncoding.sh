export output_dir_name="nginxChunkedEncoding"

source ../config.sh

program="~/Others/nginx/nginx-bin/sbin/nginx"
traces=$traces_path/*
target="chunked"


# Variables specific for tracing the program                #
############################################################# 
files="index.html"
nstop="sudo /sbin/start-stop-daemon --quiet --stop --retry QUIT/5 --pidfile ~/Others/nginx/nginx-bin/logs/nginx.pid" 
target="chunked"


# Runs that execute the target feature                      #
############################################################# 
sudo $tracer -f $target -i "dummy" -r 1 -o $traces_path -- $program 
sleep 2
curl -H "Transfer-Encoding: Chunked" -X GET  'http://127.0.0.1/index.html'
$nstop




# Runs that DONT execute the target feature                 #
############################################################# 
sudo $tracer -f "identity" -i "dummy" -r 1 -o $traces_path -- $program 
sleep 2
curl -H "Transfer-Encoding: Identity" -X GET  'http://127.0.0.1/index.html'
$nstop




sudo chown -R $(whoami) $traces
$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
sudo $retracer -d $output_path -- $program
sleep 5
curl -H "Transfer-Encoding: Chunked" -X GET  'http://127.0.0.1/index.html'
$nstop
$patcher $program $test/results/$output_dir_name/identified
