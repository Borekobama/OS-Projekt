#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"

int main() {
    MPICommunicator comm;
    const char *server_ip = getenv("SERVER_IP") ? getenv("SERVER_IP") : "188.245.63.120";

    // Initialisierung
    mpi_init(&comm, server_ip);
    printf("Rank: %d, Size: %d, IsContributor: %d\n",
           mpi_get_rank(&comm), mpi_get_size(&comm), mpi_is_contributor(&comm));

    // Unendliche Schleife, um Verbindung aufrechtzuerhalten
    while (1) {
        printf("Rank %d (Contributor: %d) is running, Size: %d\n",
               mpi_get_rank(&comm), mpi_is_contributor(&comm), mpi_get_size(&comm));

        // Sende Heartbeat-Nachricht an den Server, um Verbindung zu bestätigen
        char request[1024];
        snprintf(request, 1024,
                 "POST /heartbeat?rank=%d HTTP/1.1\r\nHost: %s\r\nContent-Length: 0\r\n\r\n",
                 mpi_get_rank(&comm), server_ip);

        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) { perror("Socket creation failed"); continue; }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(80);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("Invalid address"); close(sock_fd); continue;
        }

        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed"); close(sock_fd); continue;
        }

        if (write(sock_fd, request, strlen(request)) < 0) {
            perror("Write failed"); close(sock_fd); continue;
        }

        char response[1024];
        int bytes_read = read(sock_fd, response, 1023);
        if (bytes_read > 0) {
            response[bytes_read] = '\0';
            printf("Rank %d: Heartbeat response: %s\n", mpi_get_rank(&comm), response);
        }

        close(sock_fd);

        // Warte 5 Sekunden vor dem nächsten Heartbeat
        sleep(5);
    }

    // Finalisierung (wird in dieser Version nicht erreicht)
    mpi_finalize(&comm);
    return 0;
}