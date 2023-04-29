export output_dir_name="proftpdCpFrmCmnd"

source ../config.sh

program="~/Others/proftpd/proftpd"
traces=$traces_path/*
target="cpfrm"


# Variables specific for tracing the program                #
############################################################# 
pftpstop="kill -2 $(cat /tmp/PFTEST/PFTEST.pid)"
send_cmnd() {
    rm /tmp/out
    mkfifo /tmp/out
    tail -f /tmp/out | nc 127.0.0.1 2021 &
    pid=$!
    sleep 2
    echo "user proftpd" > /tmp/out
    sleep 2
    echo "pass proftpd" > /tmp/out
    sleep 2
    echo "$1"  > /tmp/out
    sleep 2
    echo "quit"  > /tmp/out

    pkill -2 proftpd
    rm /tmp/out
}

# Runs that execute the target feature                      #
############################################################# 
touch /tmp/file.txt
$tracer -f $target -i "file0" -r 1 -o $traces_path -- $program -n -c /tmp/PFTEST/PFTEST.conf &
sleep 8
send_cmnd "site cpfr /tmp/file.txt"
rm /tmp/file.txt
$pftpstop

# Runs that DONT execute the target feature                 #
############################################################# 
touch /tmp/file.txt
$tracer -f "cpto" -i "file0" -r 1 -o $traces_path -- $program -n -c /tmp/PFTEST/PFTEST.conf &
sleep 8
send_cmnd "site cpto /tmp"
rm /tmp/file.txt
$pftpstop




$profiler $traces $output_path
$merge $output_path
$identifier $output_path $target

touch /tmp/file.txt
$retracer -d $output_path -- $program -n -c /tmp/PFTEST/PFTEST.conf &
sleep 20
send_cmnd "site cpfr /tmp"
rm /tmp/file.txt
$pftpstop
$patcher $program $test/results/$output_dir_name/identified
