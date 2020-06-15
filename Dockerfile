FROM alpine:3.12

ENV VLSI_TOOLS_SRC=/src
ENV ACT_HOME=/install
WORKDIR = ${VLSI_TOOLS_SRC}

ADD . /src

RUN mkdir ${ACT_HOME} && \
	apk add m4 gcc g++ diffutils make libedit-dev zlib-dev m4 &&\
	./configure $ACT_HOME && \
        ./build && \
        make install && \
        make runtest
