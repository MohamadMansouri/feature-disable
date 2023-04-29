FROM ubuntu:18.04
RUN apt-get update
RUN apt-get install -y build-essential libssl-dev software-properties-common
RUN add-apt-repository ppa:deadsnakes/ppa
RUN apt-get install -y libpython3.5-dev python3-pip
RUN pip3 install pwntools 
COPY . /feat_removal
