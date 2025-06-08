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
int first_client = 1; // Flag für den ersten Client

void init_client_list(ClientList *list) {
    list->capacity = 1; // Anfängliche Kapazität
    list->clients = malloc(list->capacity * sizeof(Client));
    if (!list->clients) {
        perror("Memory allocation failed in init_client_list");
        exit(1);
    }
    printf("Client list initialized with capacity %d\n", list->capacity);
    list->count = 0;
}

void add_client(ClientList *list, int rank, const char *ip, int port, int socket_fd) {
    printf("Attempting to add client with rank %d, ip %s, port %d, socket_fd %d\n", rank, ip, port, socket_fd);
    if (list->count >= list->capacity) {
        list->capacity *= 2; // Verdopple die Kapazität
        Client *new_clients = realloc(list->clients, list->capacity * sizeof(Client));
        if (!new_clients) {
            perror("Memory reallocation failed in add_client");
            exit(1);
        }
        list->clients = new_clients;
        printf("Client list capacity increased to %d\n", list->capacity);
    }
    list->clients[list->count].rank = rank;
    strcpy(list->clients[list->count].ip, ip);
    list->clients[list->count].port = port;
    list->clients[list->count].socket_fd = socket_fd;
    list->count++;
    printf("Client with rank %d added successfully, total count: %d\n", rank, list->count);
}

void remove_client(ClientList *list, int socket_fd) {
    printf("Attempting to remove client with socket_fd %d\n", socket_fd);
    for (int i = 0; i < list->count; i++) {
        if (list->clients[i].socket_fd == socket_fd) {
            list->clients[i] = list->clients[list->count - 1]; // Letztes Element kopieren
            list->count--;
            size--;
            printf("Client with socket_fd %d removed, new count: %d, new size: %d\n", socket_fd, list->count, size);
            break;
        }
    }
}

void send_to_client(ClientList *list, int dest_rank, const char *message) {
    printf("Attempting to send to client with rank %d\n", dest_rank);
    for (int i = 0; i < list->count; i++) {
        if (list->clients[i].rank == dest_rank && list->clients[i].socket_fd >= 0) {
            if (write(list->clients[i].socket_fd, message, strlen(message)) < 0) {
                perror("Write failed in send_to_client");
            } else {
                printf("Successfully sent message to client with rank %d\n", dest_rank);
            }
            break;
        }
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_BUFFER];

    printf("Starting server initialization...\n");
    init_client_list(&client_list);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Socket created with fd %d\n", server_fd);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    printf("Server address configured: INADDR_ANY, port %d\n", PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }
    printf("Bind successful\n");

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(1);
    }
    printf("Listen successful, server listening on port %d...\n", PORT);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }
        printf("New client connection accepted with fd %d\n", client_fd);

        int bytes_read = read(client_fd, buffer, MAX_BUFFER - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Received %d bytes from client fd %d: %s\n", bytes_read, client_fd, buffer);
            char *body = strstr(buffer, "\r\n\r\n");
            if (body) { body += 4; }

            if (strstr(buffer, "POST /init") == buffer) {
                int rank = first_client ? 1 : 0; // Erster Client ist Coordinator (Rank 1)
                first_client = 0; // Nach dem ersten Client deaktivieren
                size++;
                char client_ip[16];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                int client_port = ntohs(client_addr.sin_port);

                add_client(&client_list, rank, client_ip, client_port, client_fd);

                char response[MAX_BUFFER];
                snprintf(response, MAX_BUFFER,
                         "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /init response");
                } else {
                    printf("Sent /init acknowledgment to client fd %d with rank %d\n", client_fd, rank);
                }
            }
            else if (strstr(buffer, "POST /message") == buffer) {
                int dest;
                sscanf(buffer, "POST /message?dest=%d", &dest);
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\": \"ok\"}";
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /message response");
                } else {
                    printf("Sent /message response to client fd %d\n", client_fd);
                }
                send_to_client(&client_list, dest, body);
            }
            else if (strstr(buffer, "GET /receive") == buffer) {
                int source, tag, rank;
                sscanf(buffer, "GET /receive?source=%d&tag=%d&rank=%d", &source, &tag, &rank);
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /receive response");
                } else {
                    printf("Sent /receive response to client fd %d\n", client_fd);
                }
            }
            else if (strstr(buffer, "POST /broadcast") == buffer) {
                for (int i = 0; i < client_list.count; i++) {
                    if (client_list.clients[i].rank > 1 && client_list.clients[i].socket_fd >= 0) {
                        if (write(client_list.clients[i].socket_fd, body, strlen(body)) < 0) {
                            perror("Write failed for broadcast to client");
                        } else {
                            printf("Broadcast sent to client with rank %d\n", client_list.clients[i].rank);
                        }
                    }
                }
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"status\": \"ok\"}";
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /broadcast response");
                } else {
                    printf("Sent /broadcast response to client fd %d\n", client_fd);
                }
            }
            else if (strstr(buffer, "GET /receive_broadcast") == buffer) {
                int rank;
                sscanf(buffer, "GET /receive_broadcast?rank=%d", &rank);
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /receive_broadcast response");
                } else {
                    printf("Sent /receive_broadcast response to client fd %d\n", client_fd);
                }
            }
            else if (strstr(buffer, "POST /barrier") == buffer) {
                int rank;
                sscanf(buffer, "POST /barrier?rank=%d", &rank);
                barrier_count++;
                if (barrier_count >= size - 1) {
                    for (int i = 0; i < client_list.count; i++) {
                        if (client_list.clients[i].rank > 1 && client_list.clients[i].socket_fd >= 0) {
                            if (write(client_list.clients[i].socket_fd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", 38) < 0) {
                                perror("Write failed for barrier to client");
                            } else {
                                printf("Barrier response sent to client with rank %d\n", client_list.clients[i].rank);
                            }
                        }
                    }
                    barrier_count = 0;
                }
                char response[MAX_BUFFER] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
                if (write(client_fd, response, strlen(response)) < 0) {
                    perror("Write failed for /barrier response");
                } else {
                    printf("Sent /barrier response to client fd %d\n", client_fd);
                }
            }
        }

        if (bytes_read <= 0) {
            remove_client(&client_list, client_fd);
            close(client_fd);
            printf("Client with fd %d disconnected\n", client_fd);
        }
    }

    free(client_list.clients);
    close(server_fd);
    printf("Server shutting down\n");
    return 0;
}