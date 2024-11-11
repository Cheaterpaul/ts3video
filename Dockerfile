FROM ubuntu:latest

RUN apt-get update && apt-get install -y wget tar

ENV TS3VIDEO_PORT="13370"
ENV TS3VIDEO_ADMIN_PASSWORD="admin"
ENV TS3VIDEO_RESOLUTION="640x360"
ENV TS3VIDEO_BITRATE="255"

WORKDIR /app

RUN wget -O artifact.tar https://github.com/mfreiholz/ts3video/releases/download/v0.8/server_linux-debian_x86-64-0.8.tar
RUN tar -xf artifact.tar

WORKDIR /app/server_linux-debian_x86-64-0.8

sed -i "s/^port=.*/port=${TS3VIDEO_PORT}/" default.ini
sed -i "s/^adminpassword=.*/adminpassword=${TS3VIDEO_ADMIN_PASSWORD}/" default.ini
sed -i "s/^maxresolution=.*/maxresolution=${TS3VIDEO_RESOLUTION}/" default.ini
sed -i "s/^maxbitrate=.*/maxbitrate=${TS3VIDEO_BITRATE}/" default.ini

CMD ["./videoserver.sh", "start", "--config", "default.ini"]