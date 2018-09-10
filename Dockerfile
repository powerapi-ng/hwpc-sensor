# builder image (build tools + development dependencies):
FROM debian:buster as builder
RUN apt update && \
    apt install -y build-essential cmake pkg-config libczmq-dev libpfm4-dev libcgroup-dev libmongoc-dev
COPY . /root/smartwatts-sensor
RUN cd /root/smartwatts-sensor && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=release .. && \
    make

# runner image (only runtime depedencies):
FROM debian:buster as runner
RUN apt update && \
    apt install -y libczmq4 libpfm4 libcgroup1 libmongoc-1.0-0 && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /root/smartwatts-sensor/build/smartwatts-sensor /usr/bin/smartwatts-sensor
ENTRYPOINT ["/usr/bin/smartwatts-sensor"]
CMD ["--help"]