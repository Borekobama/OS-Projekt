FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    iputils-ping \
    net-tools \
    dnsutils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY *.c ./
COPY *.h ./

RUN gcc -o main *.c -lpthread

EXPOSE 8081

CMD ["./main"]