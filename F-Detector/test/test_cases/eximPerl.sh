export output_dir_name="eximPerl"

source ../config.sh

program="~/Others/exim-bin/exim"
traces=$traces_path/*
target="perl"


# Variables specific for tracing the program                #
############################################################# 
pid="~/Others/exim-bin/exim-daemon.pid"
slp_time=8
noperlconf="~/Others/exim-bin/exim.conf.noperl"
perlconf="~/Others/exim-bin/exim.conf.perl"
conf="~/Others/exim-bin/exim.conf"

function kill_daemon {
	sleep $slp_time
	pkill -2 exim 
}


# Runs that execute the target feature                      #
############################################################# 
ln -sf $perlconf $conf
$tracer -f $target -i "config" -r 1 -o $traces_path -- $program -bdf &
kill_daemon
ln -sf $noperlconf $conf
$tracer -f $target -i "cmdarg" -r 1 -o $traces_path -- $program -bdf -ps &
kill_daemon


# Runs that DONT execute the target feature                 #
############################################################# 
ln -sf $noperlconf $conf
$tracer -f "debug" -i "cmdarg" -r 1 -o $traces_path -- $program -bdf -N &
kill_daemon


$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target
$retracer -d $output_path -- $program -bdf -ps &
kill_daemon
$patcher $program $test/results/$output_dir_name/identified
