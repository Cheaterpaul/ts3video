FROM ubuntu:latest

RUN apt-get update && apt-get install -y build-essential mesa-common-dev astyle
RUN rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY . .

RUN make


CMD ["./build-linux.bash"]