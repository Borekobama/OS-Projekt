#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 80
#define MAX_BUFFER 1024

typedef struct {
    int rank;
    char ip[16];
    int port;
    int socket_fd;
} Client;

typedef struct {
    Client *clients;
    int capacity;
    int count;
} ClientList;

ClientList client_list = {NULL, 0, 0};
int next_rank = 1; // Erster Client wird Contributor (Rank 1)
int size = 0;      // Anzahl der verbundenen Prozesse
int barrier_count = 0;

void init_client_list(ClientList *list) {
    list->capacity = 10; // Anfängliche Kapazität
    list->clients = malloc(list->capacity * sizeof(Client));
    if (!list->clients) { perror("Memory allocation failed"); exit(1); }
    list->count = 0;
}

void add_client(ClientList *list, int rank, const char *ip, int port, int socket_fd) {
    if (list->count >= list->capacity) {
        list->capacity *= 2; // Verdopple die Kapazität
        list->clients = realloc(list->clients, list->capacity * sizeof(Client));
        if (!list->clients) { perror("Memory reallocation failed"); exit(1); }
    }
    list->clients[list->count].rank = rank;
    strcpy(list->clients[list->count].ip, ip);
    list->clients[list->count].port = port;
    list->clients[list->count].socket_fd = socket_fd;
    list->count++;
}

void remove_client(ClientList *list, int socket_fd) {
    for (int i = 0; i < list->count; i++) {
        if (list->clients[i].socket_fd == socket_fd) {
            list->clients[i] = list->clients[list->count - 1]; // Letztes Element kopieren
            list->count--;
            size--;
            break;
        }
    }
}

void send_to_client(ClientList *list, int dest_rank, const char *message) {
    for (int i = 0; i < list->count; i++) {
        if (list->clients[i].rank == dest_rank && list->clients[i].socket_fd >= 0) {
            write(list->clients[i].socket_fd, message, strlen(message));
            break;
        }
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUFFER];

    init_client_list(&client_list);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("Socket creation failed"); exit(1); }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed"); exit(1);
    }

    if (listen(server_fd, 10) < 0) { perror("Listen failed"); exit(1); }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("Accept failed"); continue; }

        int bytes_read = read(client_fd, buffer, MAX_BUFFER - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            char *body = strstr(buffer, "\r\n\r\n");
            if (body) { body += 4; }

            if (strstr(buffer, "POST /init") == buffer) {
                int rank = next_rank++;
                size++;
                char client_ip[16];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                int client_port = ntohs(client_addr.sin_port);

                add_client(&client_list, rank, client_ip, client_port, client_fd);

                char response[MAX_BUFFER];
                snprintf(response, MAX_BUFFER,
                         "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n{\"rank\": %d, \"size\": %d}",
                         strlen("{\"rank\": , \"size\": }") + 20, rank, size);
                write(client_fd, response, strlen(response));
            }
            else if (strstr(buffer, "POST /message") == buffer) {
                int dest;
                sscanf(buffer, "POST /message?dest=%d", &dest);
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\": \"ok\"}";
                write(client_fd, response, strlen(response));
                send_to_client(&client_list, dest, body);
            }
            else if (strstr(buffer, "GET /receive") == buffer) {
                int source, tag, rank;
                sscanf(buffer, "GET /receive?source=%d&tag=%d&rank=%d", &source, &tag, &rank);
                // Hier müsste eine Warteschlange für Nachrichten implementiert werden
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                write(client_fd, response, strlen(response));
            }
            else if (strstr(buffer, "POST /broadcast") == buffer) {
                for (int i = 0; i < client_list.count; i++) {
                    if (client_list.clients[i].rank > 1 && client_list.clients[i].socket_fd >= 0) {
                        write(client_list.clients[i].socket_fd, body, strlen(body));
                    }
                }
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\": \"ok\"}";
                write(client_fd, response, strlen(response));
            }
            else if (strstr(buffer, "GET /receive_broadcast") == buffer) {
                int rank;
                sscanf(buffer, "GET /receive_broadcast?rank=%d", &rank);
                // Hier müsste eine Warteschlange für Broadcasts implementiert werden
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                write(client_fd, response, strlen(response));
            }
            else if (strstr(buffer, "POST /barrier") == buffer) {
                int rank;
                sscanf(buffer, "POST /barrier?rank=%d", &rank);
                barrier_count++;
                if (barrier_count >= size - 1) {
                    for (int i = 0; i < client_list.count; i++) {
                        if (client_list.clients[i].rank > 1 && client_list.clients[i].socket_fd >= 0) {
                            write(client_list.clients[i].socket_fd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 38);
                        }
                    }
                    barrier_count = 0;
                }
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                write(client_fd, response, strlen(response));
            }
        }

        if (bytes_read <= 0) {
            remove_client(&client_list, client_fd);
            close(client_fd);
        }
    }

    free(client_list.clients);
    close(server_fd);
    return 0;
}