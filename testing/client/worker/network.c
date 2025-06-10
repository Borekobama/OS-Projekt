#include "network.h"
#include <netdb.h>

// Returns the local IP address associated with the given socket file descriptor.
// On success, writes the IP as a string to ip_buffer and returns true.
// On failure, returns false.
bool get_ip_from_socket(int sock, char* ip_buffer, size_t buffer_size) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(sock, (struct sockaddr*)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, ip_buffer, buffer_size);
        return true;
    }
    return false;
}

// Returns the first local IPv4 address as a string in ip_buffer.
// On success, returns true. On failure, returns false.
bool get_local_ip(char* ip_buffer, size_t buffer_size) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname failed");
        return false;
    }

    struct hostent* host = gethostbyname(hostname);
    if (host == NULL) {
        perror("gethostbyname failed");
        return false;
    }

    struct in_addr** addr_list = (struct in_addr**)host->h_addr_list;
    if (addr_list[0] != NULL) {
        inet_ntop(AF_INET, addr_list[0], ip_buffer, buffer_size);
        return true;
    }

    fprintf(stderr, "No valid IPv4 address found\n");
    return false;
}

// Data structure for the command input thread
struct command_thread_data {
    char command[MAX_COMMAND_LEN];
    bool ready;
    int server_socket;
};

// Sets up the coordinator server, accepts worker registrations, and manages command input.
// Returns a pointer to a CoordinatorResult structure containing worker info and the chosen command.
CoordinatorResult* setup_coordinator(const char* ip, int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return NULL;
    }

    if (listen(server_socket, MAX_WORKERS) < 0) {
        perror("Listen failed");
        close(server_socket);
        return NULL;
    }

    printf("[Coordinator] Server started on %s:%d\n", ip, port);
    printf("[Coordinator] Waiting for workers...\n\n");
    printf("=== Available Commands ===\n");
    printf("  SUM  - Calculate sum of array\n");
    printf("  MIN  - Find minimum of array\n");
    printf("  MAX  - Find maximum of array\n");
    printf("  SORT - Sort array using odd-even transposition\n");
    printf("===========================\n");
    printf("Enter command when all workers are connected:\n");

    CoordinatorResult* result = malloc(sizeof(CoordinatorResult));
    result->sockets = malloc(MAX_WORKERS * sizeof(int));
    result->worker_infos = malloc(MAX_WORKERS * sizeof(WorkerInfo));
    result->worker_count = 0;

    // Thread for reading command
    pthread_t command_thread;
    struct command_thread_data command_data = { .command = "", .ready = false, .server_socket = server_socket };

    pthread_create(&command_thread, NULL, read_command_thread, &command_data);

    // Accept workers
    int current_id = 1;
    while (!command_data.ready && result->worker_count < MAX_WORKERS) {
        struct sockaddr_in worker_addr;
        socklen_t addr_len = sizeof(worker_addr);

        int worker_socket = accept(server_socket, (struct sockaddr*)&worker_addr, &addr_len);
        if (worker_socket < 0) {
            if (command_data.ready) break;
            continue;
        }

        // Read registration
        char buffer[BUFFER_SIZE];
        recv(worker_socket, buffer, BUFFER_SIZE, 0);

        if (strncmp(buffer, "REGISTRATION:", 13) == 0) {
            char worker_ip[INET_ADDRSTRLEN];
            int worker_port;
            sscanf(buffer + 13, "%[^:]:%d", worker_ip, &worker_port);

            result->sockets[result->worker_count] = worker_socket;
            strcpy(result->worker_infos[result->worker_count].ip, worker_ip);
            result->worker_infos[result->worker_count].port = worker_port;
            result->worker_infos[result->worker_count].id = current_id;

            printf("[Coordinator] Worker %d registered from %s:%d\n",
                   current_id, worker_ip, worker_port);

            // Send ID to worker
            send(worker_socket, &current_id, sizeof(int), 0);

            result->worker_count++;
            current_id++;

            printf("[Coordinator] Total workers connected: %d\n", result->worker_count);
            printf("Coordinator> ");
            fflush(stdout);
        }
    }

    // Wait for command thread
    pthread_join(command_thread, NULL);
    strcpy(result->command, command_data.command);

    // Send neighbor information
    for (int i = 0; i < result->worker_count; i++) {
        bool has_right = (i < result->worker_count - 1);
        send(result->sockets[i], &has_right, sizeof(bool), 0);

        if (has_right) {
            send(result->sockets[i], result->worker_infos[i + 1].ip, INET_ADDRSTRLEN, 0);
            send(result->sockets[i], &result->worker_infos[i + 1].port, sizeof(int), 0);
        }
    }

    close(server_socket);
    printf("[Coordinator] Network setup complete with %d workers.\n", result->worker_count);
    printf("[Coordinator] Will execute command: %s\n", result->command);

    return result;
}

// Thread function to read a command from stdin and notify the coordinator when ready.
// Accepts only "SUM", "MIN", "MAX", or "SORT" (case-insensitive).
void* read_command_thread(void* arg) {
    struct command_thread_data* data = (struct command_thread_data*)arg;

    char input[MAX_COMMAND_LEN];
    while (!data->ready) {
        if (fgets(input, MAX_COMMAND_LEN, stdin)) {
            input[strcspn(input, "\n")] = 0;

            // Convert to uppercase
            for (int i = 0; input[i]; i++) {
                input[i] = toupper(input[i]);
            }

            if (strcmp(input, "SUM") == 0 || strcmp(input, "MIN") == 0 ||
                strcmp(input, "MAX") == 0 || strcmp(input, "SORT") == 0) {
                strcpy(data->command, input);
                data->ready = true;
                printf("[Coordinator] Command '%s' received. Stopping worker registration...\n", input);
                shutdown(data->server_socket, SHUT_RDWR);
                break;
            } else {
                printf("Invalid command. Available: SUM, MIN, MAX, SORT\n");
                printf("Coordinator> ");
                fflush(stdout);
            }
        }
    }
    return NULL;
}

// Connects a worker to the coordinator, registers the worker, and receives neighbor information.
// Returns a pointer to a WorkerConnection structure with connection details.
WorkerConnection* connect_to_coordinator(const char* worker_ip, int worker_port,
                                        const char* coordinator_ip, int coordinator_port) {
    printf("[Worker] Connecting to coordinator at %s:%d...\n", coordinator_ip, coordinator_port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    struct sockaddr_in coord_addr;
    coord_addr.sin_family = AF_INET;
    coord_addr.sin_port = htons(coordinator_port);
    inet_pton(AF_INET, coordinator_ip, &coord_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&coord_addr, sizeof(coord_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return NULL;
    }

    // Get the actual local IP address used for the connection
    char actual_ip[INET_ADDRSTRLEN];
    if (!get_ip_from_socket(sock, actual_ip, INET_ADDRSTRLEN)) {
        fprintf(stderr, "Failed to get socket IP\n");
        strcpy(actual_ip, worker_ip); // Fallback to provided IP
    }
    printf("[Worker] Using IP address: %s (was: %s)\n", actual_ip, worker_ip);

    // Send registration message with actual IP and port
    char registration[BUFFER_SIZE];
    snprintf(registration, BUFFER_SIZE, "REGISTRATION:%s:%d", actual_ip, worker_port);
    send(sock, registration, strlen(registration) + 1, 0);

    // Receive assigned worker ID
    int worker_id;
    recv(sock, &worker_id, sizeof(int), 0);

    // Receive neighbor information
    bool has_right_neighbor;
    recv(sock, &has_right_neighbor, sizeof(bool), 0);

    WorkerConnection* conn = malloc(sizeof(WorkerConnection));
    conn->socket = sock;
    conn->id = worker_id;
    strcpy(conn->own_ip, worker_ip);
    conn->own_port = worker_port;
    conn->has_right_neighbor = has_right_neighbor;

    if (has_right_neighbor) {
        recv(sock, conn->right_neighbor_ip, INET_ADDRSTRLEN, 0);
        recv(sock, &conn->right_neighbor_port, sizeof(int), 0);
        printf("[Worker] Successfully connected with ID: %d\n", worker_id);
        printf("[Worker] Right neighbor at: %s:%d\n",
               conn->right_neighbor_ip, conn->right_neighbor_port);
    } else {
        conn->right_neighbor_port = -1;
        printf("[Worker] Successfully connected with ID: %d\n", worker_id);
        printf("[Worker] No right neighbor (last worker)\n");
    }

    return conn;
}

// Frees the memory allocated for a CoordinatorResult structure and its members.
void free_coordinator_result(CoordinatorResult* result) {
    if (result) {
        free(result->sockets);
        free(result->worker_infos);
        free(result);
    }
}

// Frees the memory allocated for a WorkerConnection structure.
void free_worker_connection(WorkerConnection* conn) {
    if (conn) {
        free(conn);
    }
}