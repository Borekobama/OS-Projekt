FROM gcc:latest
WORKDIR /app
COPY . .
RUN gcc -o app main.c mpi.c
CMD ["./app"]