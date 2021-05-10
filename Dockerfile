FROM ubuntu:20.04 as build

ADD tzdata.sh /root/tzdata.sh

USER root

RUN chmod 755 /root/tzdata.sh && /root/tzdata.sh

WORKDIR /home/user/

RUN apt-get update && apt-get install build-essential cmake git -y

RUN git clone https://github.com/mavlink/MAVSDK.git &&\
    cd MAVSDK &&\
    git checkout v0.39.0 &&\
    git submodule update --init --recursive

RUN cd MAVSDK &&\
    cmake -Bbuild/default -DCMAKE_BUILD_TYPE=Release -H. &&\
    cmake --build build/default -j12 &&\
    cmake --build build/default --target install &&\
    ldconfig

COPY server /usr/server

RUN mkdir /usr/server/build && cd /usr/server/build && cmake .. && make -j4

EXPOSE 14540/udp
EXPOSE 6969

CMD ["/usr/server/build/server"]

# if you want to reduce container size try this (may require changing cmake)

# FROM alpine:latest as runtime

# WORKDIR /usr/

# COPY --from=build /usr/server/build/server /usr/server

# EXPOSE 14540
# EXPOSE 6969

# CMD ["./server"]


