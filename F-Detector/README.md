# F-Detector

F-Detector is a tool for identifing an unwanted features in a program by detecting its feature-specific edge. 
This repo contains a demo of F-Detector that runs on linux systems. 

## Requirements
- Docker
- IDA 3.4 (Check [Here](#i-dont-have-ida-what-can-i-do) if missing)


## Building the tool
- Build the docker container: `sudo docker build . -t feat-removal`
- Enter the docker container: `sudo docker run -v <ida_folder_path>:/ida -it feat-removal` _(replace ida\_folder\_path with the path to the folder containing the idat64 executable on you machine)_
- Enter the repo and make: `cd feat_removal && make`

## Testing the tool
__NOTE: Before running the tool you have to run IDA from inside the container and accept its license. This is mandatory since IDA running inside the container consider you as a new user. Without this action our tool will not be able to run IDA in the background__
- Run IDA to accept the license: `/ida/idat64`
- Enter the test directory: `cd test/test_cases`
- Run: `./busyBoxWget.sh` or `./zipUnzipCmndArg.sh`


Each of the scripts run __F-Detector__ on a specified program and feature. The intermediate results are stored in tests/results. When __F-Detector__ finishes, it outputs a new patched executable with the unwanted feature disabled. The new patched file is stored in `test` directory.

To test if the new patch executable disables the feature, run `./busybox-patched wget`. The program should get inturupted and exits without executing the feature.

## Test Cases
There are 13 test cases each disables a different feature in 9 apps. However we only put in this repo the executables of busybox
and zip. The only test cases that you can run immediately are ./busyBoxWget.sh and ./zipUnzipCmndArg.sh which disables wget feature
and unzip-argument option from busybox and zip respectively. To try the other scripts, you need to install the corresponding app and modify the script by assigning the corresponding executable to the `program` variable.

## I don't have IDA. What can I do?
IDA is required to run the static analysis. We implemented our tool using IDA 3.4. Other versions of IDA should not fail but are not tested. If you do not have IDA on your system you can still run the demo. Switch to the git branch `no-ida`. In this branch we 
included the results of IDA analysis in the repo. 
To build the repo:
- Build the docker container: `sudo docker build . -t feat-removal-noida`
- Enter the docker container: `sudo docker run -it feat-removal-noida`
- Enter the repo and make: `cd feat_removal && make`

To run the test script : 
- Enter the test directory: `cd test/test_cases`
- Run: `./busyBoxWget.sh` or `./zipUnzipCmndArg.sh`


## FAQ
__I forgot to accept IDA license before I run the script. The script is now hanging. What do I do?__

This happens because __F-Detector__ is waiting for IDA to launch but it won't. Press ctr+c, sign the license, and 
remove the corrupted temporal files by running: `rm -r /tmp/uid_0`. Then try again
