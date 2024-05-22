# libpfm dependency builder image
FROM ubuntu:22.04 as libpfm-builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt update && \
    apt install -y build-essential git devscripts debhelper dpatch python3-dev libncurses-dev swig && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 1 && \
    git clone -b smartwatts https://github.com/gfieni/libpfm4.git /root/libpfm4 && \
    cd /root/libpfm4 && \
    fakeroot debian/rules binary

# sensor builder image (build tools + development dependencies):
FROM ubuntu:22.04 as sensor-builder
ENV DEBIAN_FRONTEND=noninteractive
ARG BUILD_TYPE=Debug
ARG MONGODB_SUPPORT=ON
RUN apt update && \
    apt install -y build-essential git clang-tidy cmake pkg-config libczmq-dev libjson-c-dev libsystemd-dev uuid-dev && \
    echo "${MONGODB_SUPPORT}" |grep -iq "on" && apt install -y libmongoc-dev || true
COPY --from=libpfm-builder /root/libpfm4*.deb /tmp/
RUN dpkg -i /tmp/libpfm4_*.deb /tmp/libpfm4-dev_*.deb && \
    rm /tmp/*.deb
COPY . /usr/src/hwpc-sensor
RUN cd /usr/src/hwpc-sensor && \
    GIT_TAG=$(git describe --tags --dirty 2>/dev/null || echo "unknown") \
    GIT_REV=$(git rev-parse HEAD 2>/dev/null || echo "unknown") \
    cmake -B build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_CLANG_TIDY="clang-tidy" -DWITH_MONGODB="${MONGODB_SUPPORT}" && \
    cmake --build build --parallel $(getconf _NPROCESSORS_ONLN)

# sensor runner image (only runtime depedencies):
FROM ubuntu:22.04 as sensor-runner
ENV DEBIAN_FRONTEND=noninteractive
ARG BUILD_TYPE=Debug
ARG MONGODB_SUPPORT=ON
ARG FILE_CAPABILITY=CAP_SYS_ADMIN
RUN useradd -d /opt/powerapi -m powerapi && \
    apt update && \
    apt install -y libczmq4 libjson-c5 libcap2-bin && \
    echo "${MONGODB_SUPPORT}" |grep -iq "on" && apt install -y libmongoc-1.0-0 || true && \
    echo "${BUILD_TYPE}" |grep -iq "debug" && apt install -y libasan6 libubsan1 || true && \
    rm -rf /var/lib/apt/lists/*
COPY --from=libpfm-builder /root/libpfm4*.deb /tmp/
RUN dpkg -i /tmp/libpfm4_*.deb && \
    rm /tmp/*.deb
COPY --from=sensor-builder /usr/src/hwpc-sensor/build/hwpc-sensor /usr/bin/hwpc-sensor
RUN setcap "${FILE_CAPABILITY}+ep" /usr/bin/hwpc-sensor && \
    setcap -v "${FILE_CAPABILITY}+ep" /usr/bin/hwpc-sensor
COPY docker/entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["--help"]
