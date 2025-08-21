FROM arm64v8/ubuntu:22.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libbluetooth-dev libglib2.0-dev ca-certificates && \
    update-ca-certificates

WORKDIR /opt
RUN git clone --depth=1 https://github.com/labapart/gattlib.git
WORKDIR /opt/gattlib/build
RUN cmake .. -DGATTLIB_SHARED_LIB=OFF -DGATTLIB_ENABLE_EXAMPLES=OFF
RUN make -j && make install
RUN ldconfig

WORKDIR /ws
COPY . /ws

FROM build AS compile
RUN mkdir -p build && \
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j
RUN mkdir -p /out && cp build/bin/ble_min /out/

FROM scratch AS export
COPY --from=compile /out /out
