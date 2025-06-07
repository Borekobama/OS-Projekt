FROM gcc:latest
WORKDIR /app
COPY . .
RUN gcc -o app server.c
CMD ["./app"]