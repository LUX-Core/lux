FROM debian:jessie

RUN apt-get -q update && \
    apt-get -qy install \
        git \
        curl \
        build-essential \
        libboost-all-dev \
        libcurl4-openssl-dev \
        libgmp-dev \
        libleveldb-dev \
        libmicrohttpd-dev \
        libminiupnpc-dev && \
    git clone --recursive https://github.com/cpp-ethereum/cpp-cpp-ethereum --branch build-on-linux --single-branch --depth 1 && \
    cpp-cpp-ethereum/scripts/install_cmake.sh --prefix /tmp && \
    cd /tmp && PATH=$PATH:/tmp/bin cmake /cpp-cpp-ethereum && \
    PATH=$PATH:/tmp/bin cmake --build /tmp -- -j8 && \
    cd /tmp && make install && \
    apt-get purge git curl build-essential -qy && \
    apt-get autoremove -qy && \
    apt-get clean && \
    rm /cpp-cpp-ethereum -rf && \
    rm /tmp/* -rf

ENTRYPOINT ["/usr/local/bin/eth"]
