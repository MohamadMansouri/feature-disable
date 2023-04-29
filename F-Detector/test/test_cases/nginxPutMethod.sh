export output_dir_name="nginxPut"

source ../config.sh

program="~/Others/nginx/nginx-bin/sbin/nginx"
traces=$traces_path/*
target="PUT"


# Variables specific for tracing the program                #
############################################################# 
methods="POST GET PUT MOVE"
nstop="sudo /sbin/start-stop-daemon --quiet --stop --retry QUIT/5 --pidfile ~/Others/nginx/nginx-bin/logs/nginx.pid" 


# Runs that execute the target feature                      #
############################################################# 
sudo $tracer -f $target -i "dummy" -r 1 -o $traces_path -- $program 
sleep 2
curl -X PUT -d "hello world" 'http://127.0.0.1/uploaded/put.txt'
$nstop




# Runs that DONT execute the target feature                 #
############################################################# 
sudo $tracer -f "GET" -i "dummy" -r 1 -o $traces_path -- $program 
sleep 2
curl -X GET -d "hello world" 'http://127.0.0.1/uploaded/put.txt'
$nstop





sudo chown -R $(whoami) $traces
$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
sudo $retracer -d $output_path -- $program
sleep 5
curl -X PUT -d \"RANDOMMESSAGE\" 'http://127.0.0.1/uploaded/put.txt'
$nstop
$patcher $program $test/results/$output_dir_name/identified
