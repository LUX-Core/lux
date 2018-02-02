FROM ubuntu:16.04

RUN echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-3.9 main" >> /etc/apt/sources.list \
	&& apt-get update \
	&& apt-get install -y --no-install-recommends --allow-unauthenticated \
		cmake \
		g++ \
		make \
		llvm-3.9-dev \
		zlib1g-dev \
	&& rm -rf /var/lib/apt/lists/*
