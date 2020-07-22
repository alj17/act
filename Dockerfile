FROM centos:8 as builder

ADD . /src
WORKDIR /src
ENV VLSI_TOOLS_SRC=/src ACT_HOME=/src/install

RUN yum install -y 'dnf-command(config-manager)' && \
	yum config-manager --set-enabled PowerTools -y && \
	yum install -y gcc gcc-c++ diffutils make libedit-devel zlib-devel m4 && \
	mkdir install && \
	./configure $ACT_HOME && \
	./build && \
	make install && \
	make runtest
