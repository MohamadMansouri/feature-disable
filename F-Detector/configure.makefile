PROJECT_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
PIN_HOME := $(PROJECT_PATH)pin

# ida64 executable path
IDA_EXE = "/ida/ida64"
# idat64 executable path
IDA_T_EXE = "/ida/idat64"


export PIN_HOME
export IDA_EXE
export IDA_T_EXE