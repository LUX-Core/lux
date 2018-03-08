FROM fedora:25

RUN dnf -qy install git cmake
RUN git clone --recursive https://github.com/cpp-ethereum/cpp-cpp-ethereum --branch build-on-linux --single-branch --depth 2
RUN /cpp-cpp-ethereum/scripts/install_deps.sh

RUN cd /tmp && cmake /cpp-cpp-ethereum
RUN cd /tmp && make -j8 && make install && ldconfig

ENTRYPOINT ["/usr/local/bin/eth"]
