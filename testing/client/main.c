#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "mpi.h"

int main() {
    MPICommunicator comm;
    const char *server_ip = getenv("SERVER_IP") ? getenv("SERVER_IP") : "188.245.63.120";

    // Initialisierung
    printf("Initializing with server IP: %s\n", server_ip);
    mpi_init(&comm, server_ip);
    if (comm.rank == 0 || comm.size == 0) {
        printf("Initialization failed: Rank or Size not set. Rank: %d, Size: %d\n", comm.rank, comm.size);
        sleep(5);
        return 1; // Beende bei Fehlschlag
    }
    printf("Initialization complete. Rank: %d, Size: %d, Contributor: %s\n",
           comm.rank, comm.size,
           comm.rank == 1 ? "yes" : "no");

    // Unendliche Schleife, um Verbindung aufrechtzuerhalten
    while (1) {
        printf("Rank %d (Contributor: %s) is running, Size: %d\n",
               comm.rank,
               comm.rank == 1 ? "yes" : "no",
               comm.size);

        // Sende Heartbeat-Nachricht an den Server, um Verbindung zu bestätigen
        char request[1024] = {0};
        int request_len = snprintf(request, sizeof(request),
                 "POST /heartbeat?rank=%d HTTP/1.1\r\nHost: %s\r\nContent-Length: 0\r\n\r\n",
                 comm.rank, server_ip);
        if (request_len >= sizeof(request)) {
            printf("Rank %d: Heartbeat request buffer overflow\n", comm.rank);
            sleep(5);
            continue;
        }

        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("Socket creation failed");
            sleep(5);
            continue;
        }
        printf("Socket created for Rank %d\n", comm.rank);

        struct sockaddr_in server_addr = {0};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(80);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("Invalid address");
            close(sock_fd);
            sleep(5);
            continue;
        }
        printf("Address resolved for Rank %d\n", comm.rank);

        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            close(sock_fd);
            sleep(5);
            continue;
        }
        printf("Connected to server for Rank %d\n", comm.rank);

        if (write(sock_fd, request, strlen(request)) < 0) {
            perror("Write failed");
            close(sock_fd);
            sleep(5);
            continue;
        }
        printf("Heartbeat sent for Rank %d\n", comm.rank);

        char response[1024] = {0};
        int bytes_read = read(sock_fd, response, sizeof(response) - 1);
        if (bytes_read > 0) {
            response[bytes_read] = '\0';
            printf("Rank %d: Heartbeat response: %s\n", comm.rank, response);
        } else if (bytes_read == 0) {
            printf("Rank %d: Server closed connection\n", comm.rank);
        } else {
            perror("Read failed");
        }

        close(sock_fd);

        // Warte 5 Sekunden vor dem nächsten Heartbeat
        sleep(5);
    }

    // Finalisierung (wird in dieser Version nicht erreicht)
    mpi_finalize(&comm);
    return 0;
}