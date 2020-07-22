FROM centos:8 as builder

ADD . /src
WORKDIR /src
ENV VLSI_TOOLS_SRC=/src ACT_HOME=/install

RUN yum install -y 'dnf-command(config-manager)' && \
	yum config-manager --set-enabled PowerTools -y && \
	yum install -y gcc gcc-c++ diffutils make libedit-devel zlib-devel && \
	yum install -y m4 doxygen texinfo texinfo-tex && \
	mkdir /install && \
	./configure $ACT_HOME && \
	./build $ACT_HOME && \
        make -C $VLSI_TOOLS_SRC/act docs && \
	make install && \
	make runtest

FROM centos:8
ENV  ACT_HOME=/etc/act
RUN yum install -y libedit && mkdir -p /etc/act/conf /usr/share/act/doc/html
COPY --from=builder /install/bin /usr/bin
COPY --from=builder /install/include /usr/include
COPY --from=builder /install/lib /usr/lib
COPY --from=builder /install/conf /etc/act/conf
COPY --from=builder /src/act/doc/act.pdf /usr/share/act/doc/act.pdf
COPY --from=builder /src/act/html /usr/share/act/doc/html
