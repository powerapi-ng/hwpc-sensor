# builder image (build tools + development dependencies):
FROM debian:buster

WORKDIR /srv

COPY . /tmp/build_deb

ARG BUILD_TYPE=Debug
ARG MONGODB_SUPPORT=OFF

RUN apt update && \
    apt install -y cmake build-essential git pkg-config libbson-dev libczmq-dev libsystemd-dev uuid-dev clang-tidy cmake devscripts debhelper dpatch python3-dev libncurses-dev swig

RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 1

RUN git clone -b msr-pmu-old https://github.com/gfieni/libpfm4.git /tmp/libpfm4

RUN cd /tmp/libpfm4 && \
    fakeroot debian/rules binary

RUN dpkg -i /tmp/libpfm4_10.1_amd64.deb && \
    dpkg -i /tmp/libpfm4-dev_10.1_amd64.deb && \
    rm /tmp/*.deb

RUN apt update && \
    apt install -y cmake   && \
	echo "${MONGODB_SUPPORT}" |grep -iq "on" && apt install -y libmongoc-dev || true

RUN git clone https://github.com/powerapi-ng/hwpc-sensor.git /tmp/hwpc-sensor

RUN mkdir /tmp/hwpc-sensor/build
RUN cd /tmp/hwpc-sensor/build && \
	GIT_TAG=$(git describe --tags --dirty 2>/dev/null || echo "unknown") && \
	GIT_REV=$(git rev-parse HEAD 2>/dev/null || echo "unknown") && \
	cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_CLANG_TIDY="clang-tidy" -DWITH_MONGODB="${MONGODB_SUPPORT}" .. && \
	make -j $(getconf _NPROCESSORS_ONLN) && \
	cp /tmp/hwpc-sensor/build/hwpc-sensor  /usr/bin/hwpc-sensor
ENTRYPOINT ["/usr/bin/hwpc-sensor"]
CMD ["--help"]
