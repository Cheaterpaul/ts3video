FROM ubuntu:latest

RUN apt-get update && apt-get install -y wget tar

WORKDIR /app

RUN wget -O artifact.tar https://github.com/mfreiholz/ts3video/releases/download/v0.8/server_linux-debian_x86-64-0.8.tar
RUN tar -xf artifact.tar
RUN ls
WORKDIR /app/server_linux-debian_x86-64-0.8
RUN ls
CMD ["./videoserver.sh", "start"]