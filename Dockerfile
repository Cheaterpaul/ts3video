FROM ubuntu:20.04

RUN apt-get update && apt-get install -y wget tar

ENV TS3VIDEO_PORT="13370"
ENV TS3VIDEO_ADMIN_PASSWORD="admin"
ENV TS3VIDEO_RESOLUTION="640x360"
ENV TS3VIDEO_BITRATE="255"

WORKDIR /app

RUN wget -O artifact.tar https://github.com/mfreiholz/ts3video/releases/download/v0.8/server_linux-debian_x86-64-0.8.tar
RUN tar -xf artifact.tar

WORKDIR /app/server_linux-debian_x86-64-0.8

RUN echo '#!/bin/bash' > replace.sh && \
    echo 'sed -i "s/^port=.*/port=${TS3VIDEO_PORT}/" default.ini' >> replace.sh && \
    echo 'sed -i "s/^adminpassword=.*/adminpassword=${TS3VIDEO_ADMIN_PASSWORD}/" default.ini' >> replace.sh && \
    echo 'sed -i "s/^maxresolution=.*/maxresolution=${TS3VIDEO_RESOLUTION}/" default.ini' >> replace.sh && \
    echo 'sed -i "s/^maxbitrate=.*/maxbitrate=${TS3VIDEO_BITRATE}/" default.ini' >> replace.sh
RUN chmod +x replace.sh


CMD ["/bin/bash", "-c", "sed '' default.init && sed '' replace.sh && ./replace.sh && ./videoserver.sh start --config default.ini && tail -f /dev/null"]