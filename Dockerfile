# libpfm dependency builder image
FROM debian:buster as libpfm-builder
RUN apt update && \
    apt install -y build-essential git devscripts debhelper dpatch python3-dev libncurses-dev swig && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 1 && \
    git clone -b smartwatts https://github.com/gfieni/libpfm4.git /root/libpfm4 && \
    cd /root/libpfm4 && \
    fakeroot debian/rules binary

# sensor builder image (build tools + development dependencies):
FROM debian:buster as sensor-builder
ARG BUILD_TYPE=Debug
ARG MONGODB_SUPPORT=ON
RUN apt update && \
    apt install -y build-essential git clang-tidy cmake pkg-config libczmq-dev libsystemd-dev uuid-dev && \
    echo "${MONGODB_SUPPORT}" |grep -iq "on" && apt install -y libmongoc-dev || true
COPY --from=libpfm-builder /root/libpfm4*.deb /tmp/
RUN dpkg -i /tmp/libpfm4_*.deb /tmp/libpfm4-dev_*.deb && \
    rm /tmp/*.deb
COPY . /usr/src/hwpc-sensor
RUN cd /usr/src/hwpc-sensor && \
    mkdir build && \
    cd build && \
    GIT_TAG=$(git describe --tags --dirty 2>/dev/null || echo "unknown") \
    GIT_REV=$(git rev-parse HEAD 2>/dev/null || echo "unknown") \
    cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_CLANG_TIDY="clang-tidy" -DWITH_MONGODB="${MONGODB_SUPPORT}" .. && \
    make -j $(getconf _NPROCESSORS_ONLN)

# sensor runner image (only runtime depedencies):
FROM debian:buster as sensor-runner
ARG BUILD_TYPE=Debug
ARG MONGODB_SUPPORT=ON
RUN apt update && \
    apt install -y libczmq4 && \
    echo "${MONGODB_SUPPORT}" |grep -iq "on" && apt install -y libmongoc-1.0-0 || true && \
    echo "${BUILD_TYPE}" |grep -iq "debug" && apt install -y libasan5 libubsan1 || true && \
    rm -rf /var/lib/apt/lists/*
COPY --from=libpfm-builder /root/libpfm4*.deb /tmp/
RUN dpkg -i /tmp/libpfm4_*_amd64.deb && \
    rm /tmp/*.deb
COPY --from=sensor-builder /usr/src/hwpc-sensor/build/hwpc-sensor /usr/bin/hwpc-sensor
ENTRYPOINT ["/usr/bin/hwpc-sensor"]
CMD ["--help"]
