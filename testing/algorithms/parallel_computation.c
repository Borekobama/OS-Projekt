// ==================== parallel_computation.h ====================
#ifndef PARALLEL_COMPUTATION_H
#define PARALLEL_COMPUTATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#define MAX_WORKERS 100
#define BUFFER_SIZE 1024
#define MAX_COMMAND_LEN 32

// ==================== Data Structures ====================

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    int id;
} WorkerInfo;

typedef struct {
    int socket;
    int id;
    char own_ip[INET_ADDRSTRLEN];
    int own_port;
    char right_neighbor_ip[INET_ADDRSTRLEN];
    int right_neighbor_port;
    bool has_right_neighbor;
} WorkerConnection;

typedef struct {
    int *sockets;
    WorkerInfo *worker_infos;
    int worker_count;
    char command[MAX_COMMAND_LEN];
} CoordinatorResult;

typedef struct {
    int rank;
    int size;
    bool is_root;
    
    // Star topology connections
    int *connections;
    int connection_count;
    
    // Ring topology connections
    int left_neighbor_socket;
    int right_neighbor_socket;
    bool has_left_neighbor;
    bool has_right_neighbor;
} Communicator;

// Function pointer for algorithms
typedef void* (*AlgorithmFunc)(Communicator*, int*, int);

// ==================== Function Declarations ====================

void* read_command_thread(void* arg);

// Connection setup functions
CoordinatorResult* setup_coordinator(const char* ip, int port);
WorkerConnection* connect_to_coordinator(const char* worker_ip, int worker_port,
                                        const char* coordinator_ip, int coordinator_port);
void free_coordinator_result(CoordinatorResult* result);
void free_worker_connection(WorkerConnection* conn);

// Communicator functions
Communicator* create_coordinator_communicator(int rank, int* worker_sockets, 
                                            int worker_count, WorkerInfo* first_worker);
Communicator* create_worker_communicator(int rank, int coordinator_socket,
                                       const char* own_ip, int own_port,
                                       const char* right_neighbor_ip, int right_neighbor_port);
void free_communicator(Communicator* comm);

// Communication operations
void send_int(Communicator* comm, int value, int dest);
int receive_int(Communicator* comm, int source);
void send_int_array(Communicator* comm, int* data, int length, int dest);
int* receive_int_array(Communicator* comm, int source, int* length);
int reduce_int(Communicator* comm, int value, int (*op)(int, int));
bool reduce_bool(Communicator* comm, bool value);
void broadcast_string(Communicator* comm, const char* message);
char* receive_broadcast(Communicator* comm);
void barrier(Communicator* comm);
int* scatter(Communicator* comm, int* data, int* chunk_sizes, int* my_chunk_size);
int** gather(Communicator* comm, int* data, int length);

// Neighbor communication
void send_to_left_neighbor(Communicator* comm, int value);
void send_to_right_neighbor(Communicator* comm, int value);
int receive_from_left_neighbor(Communicator* comm);
int receive_from_right_neighbor(Communicator* comm);

// Algorithm functions
void* sum_algorithm(Communicator* comm, int* local_data, int length);
void* min_algorithm(Communicator* comm, int* local_data, int length);
void* max_algorithm(Communicator* comm, int* local_data, int length);
void* sort_algorithm(Communicator* comm, int* local_data, int length);

// Utility functions
int* create_random_array(int length);
int* calculate_chunk_sizes(int array_length, int num_processes);
void quick_sort(int* arr, int length);
void insert_from_left(int* arr, int length);
void insert_from_right(int* arr, int length);
bool validate_sum(int calculated_sum, int* original_array, int length);
bool validate_min(int calculated_min, int* original_array, int length);
bool validate_max(int calculated_max, int* original_array, int length);
bool is_sorted(int* array, int length);

// Helper functions
int min_op(int a, int b);
int max_op(int a, int b);
int sum_op(int a, int b);

#endif

// ==================== parallel_computation.c ===================

// ==================== Main Program ====================

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage:\nCoordinator: c <ownIP> <ownPort>\n");
        printf("Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>\n");
        return 1;
    }
    
    bool is_coordinator = (argv[1][0] == 'c' || argv[1][0] == 'C');
    char* own_ip = argv[2];
    int own_port = atoi(argv[3]);
    
    if (is_coordinator) {
        // Run as coordinator
        CoordinatorResult* result = setup_coordinator(own_ip, own_port);
        if (!result) {
            fprintf(stderr, "Failed to setup coordinator\n");
            return 1;
        }
        
        // Create communicator
        WorkerInfo* first_worker = (result->worker_count > 0) ? &result->worker_infos[0] : NULL;
        Communicator* comm = create_coordinator_communicator(0, result->sockets, 
                                                           result->worker_count, first_worker);
        
        // Create initial array
        int array_length = 100;
        int* initial_array = create_random_array(array_length);
        printf("[Coordinator] Created initial array of length %d\n", array_length);
        
        // Execute command
        printf("[Coordinator] Executing command: %s\n", result->command);
        
        // Distribute array
        int* chunk_sizes = calculate_chunk_sizes(array_length, comm->size);
        int my_chunk_size;
        int* chunk = scatter(comm, initial_array, chunk_sizes, &my_chunk_size);
        
        printf("[Coordinator] Array distributed. My chunk: [");
        for (int i = 0; i < my_chunk_size; i++) {
            printf("%d%s", chunk[i], (i < my_chunk_size - 1) ? ", " : "");
        }
        printf("]\n");
        
        // Broadcast command
        broadcast_string(comm, result->command);
        
        // Execute algorithm
        AlgorithmFunc algorithm = NULL;
        if (strcasecmp(result->command, "SUM") == 0) {
            algorithm = sum_algorithm;
        } else if (strcasecmp(result->command, "MIN") == 0) {
            algorithm = min_algorithm;
        } else if (strcasecmp(result->command, "MAX") == 0) {
            algorithm = max_algorithm;
        } else if (strcasecmp(result->command, "SORT") == 0) {
            algorithm = sort_algorithm;
        }
        
        void* result_value = NULL;
        if (algorithm) {
            result_value = algorithm(comm, chunk, my_chunk_size);
        }
        
        // Display results
        if (result_value) {
            if (strcasecmp(result->command, "SUM") == 0) {
                int sum = *(int*)result_value;
                printf("[Coordinator] Final Sum: %d\n", sum);
                printf("[Coordinator] Correct? %s\n", 
                       validate_sum(sum, initial_array, array_length) ? "true" : "false");
                free(result_value);
            } else if (strcasecmp(result->command, "MIN") == 0) {
                int min = *(int*)result_value;
                printf("[Coordinator] Final Min: %d\n", min);
                printf("[Coordinator] Correct? %s\n", 
                       validate_min(min, initial_array, array_length) ? "true" : "false");
                free(result_value);
            } else if (strcasecmp(result->command, "MAX") == 0) {
                int max = *(int*)result_value;
                printf("[Coordinator] Final Max: %d\n", max);
                printf("[Coordinator] Correct? %s\n", 
                       validate_max(max, initial_array, array_length) ? "true" : "false");
                free(result_value);
            } else if (strcasecmp(result->command, "SORT") == 0) {
                int* sorted = (int*)result_value;
                printf("[Coordinator] Final sorted array: [");
                for (int i = 0; i < array_length; i++) {
                    printf("%d%s", sorted[i], (i < array_length - 1) ? ", " : "");
                }
                printf("]\n");
                printf("[Coordinator] Correctly sorted? %s\n", 
                       is_sorted(sorted, array_length) ? "true" : "false");
                free(sorted);
            }
        }
        
        // Cleanup
        barrier(comm);
        printf("[Coordinator] Shutting down...\n");
        free(chunk);
        free(chunk_sizes);
        free(initial_array);
        free_communicator(comm);
        free_coordinator_result(result);
        printf("[Coordinator] Goodbye!\n");
        
    } else {
        // Run as worker
        if (argc < 6) {
            printf("Usage for Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>\n");
            return 1;
        }
        
        char* coordinator_ip = argv[4];
        int coordinator_port = atoi(argv[5]);
        
        WorkerConnection* conn = connect_to_coordinator(own_ip, own_port, 
                                                       coordinator_ip, coordinator_port);
        if (!conn) {
            fprintf(stderr, "Failed to connect to coordinator\n");
            return 1;
        }
        
        // Create communicator
        Communicator* comm = create_worker_communicator(conn->id, conn->socket,
                                                       conn->own_ip, conn->own_port,
                                                       conn->right_neighbor_ip, 
                                                       conn->right_neighbor_port);
        
        printf("[Worker %d] Ready and waiting for data...\n", comm->rank);
        
        // Receive array chunk
        int chunk_length;
        int* chunk = receive_int_array(comm, 0, &chunk_length);
        printf("[Worker %d] Received chunk: [", comm->rank);
        for (int i = 0; i < chunk_length; i++) {
            printf("%d%s", chunk[i], (i < chunk_length - 1) ? ", " : "");
        }
        printf("]\n");
        
        // Receive command
        char* command = receive_broadcast(comm);
        printf("[Worker %d] Received command: %s\n", comm->rank, command);
        
        // Execute algorithm
        AlgorithmFunc algorithm = NULL;
        if (strcasecmp(command, "SUM") == 0) {
            algorithm = sum_algorithm;
        } else if (strcasecmp(command, "MIN") == 0) {
            algorithm = min_algorithm;
        } else if (strcasecmp(command, "MAX") == 0) {
            algorithm = max_algorithm;
        } else if (strcasecmp(command, "SORT") == 0) {
            algorithm = sort_algorithm;
        }
        
        if (algorithm) {
            algorithm(comm, chunk, chunk_length);
        }
        
        // Cleanup
        // Cleanup
        barrier(comm);
        printf("[Worker %d] Shutting down...\n", comm->rank);
        
        int worker_id = comm->rank;  // NEU: Speichern vor dem Freigeben
        
        free(chunk);
        free(command);
        free_communicator(comm);
        free_worker_connection(conn);
        printf("[Worker %d] Worker terminated.\n", worker_id);  // GEÄNDERT: worker_id statt conn->id
    }
    
    return 0;
}

// ==================== Connection Setup Implementation ====================

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
    server_addr.sin_addr.s_addr = INADDR_ANY;
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
    struct {
        char command[MAX_COMMAND_LEN];
        bool ready;
        int server_socket;
    } command_data = { .command = "", .ready = false, .server_socket = server_socket };
    
    pthread_create(&command_thread, NULL, (void*)read_command_thread, &command_data);
    
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

void* read_command_thread(void* arg) {
    struct {
        char command[MAX_COMMAND_LEN];
        bool ready;
        int server_socket;
    }* data = arg;
    
    char input[MAX_COMMAND_LEN];
    while (!data->ready) {
        printf("Coordinator> ");
        fflush(stdout);
        
        if (fgets(input, MAX_COMMAND_LEN, stdin)) {
            input[strcspn(input, "\n")] = 0; // Remove newline
            
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
                close(data->server_socket);
                break;
            } else {
                printf("Invalid command. Available: SUM, MIN, MAX, SORT\n");
            }
        }
    }
    return NULL;
}

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
    
    // Send registration
    char registration[BUFFER_SIZE];
    snprintf(registration, BUFFER_SIZE, "REGISTRATION:%s:%d", worker_ip, worker_port);
    send(sock, registration, strlen(registration) + 1, 0);
    
    // Receive ID
    int worker_id;
    recv(sock, &worker_id, sizeof(int), 0);
    
    // Receive neighbor info
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

// ==================== Communicator Implementation ====================

Communicator* create_coordinator_communicator(int rank, int* worker_sockets, 
                                            int worker_count, WorkerInfo* first_worker) {
    Communicator* comm = malloc(sizeof(Communicator));
    comm->rank = 0;
    comm->is_root = true;
    comm->size = worker_count + 1;
    
    // Star topology
    comm->connection_count = worker_count;
    comm->connections = malloc(worker_count * sizeof(int));
    memcpy(comm->connections, worker_sockets, worker_count * sizeof(int));
    
    // Ring topology - connect to first worker if exists
    comm->left_neighbor_socket = -1;
    comm->has_left_neighbor = false;
    comm->right_neighbor_socket = -1;
    comm->has_right_neighbor = false;
    
    if (first_worker) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(first_worker->port);
        inet_pton(AF_INET, first_worker->ip, &addr.sin_addr);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            comm->right_neighbor_socket = sock;
            comm->has_right_neighbor = true;
            printf("[Coordinator] Connected to right neighbor (Worker 1)\n");
        }
    }
    
    return comm;
}

Communicator* create_worker_communicator(int rank, int coordinator_socket,
                                       const char* own_ip, int own_port,
                                       const char* right_neighbor_ip, int right_neighbor_port) {
    Communicator* comm = malloc(sizeof(Communicator));
    comm->rank = rank;
    comm->is_root = false;
    comm->size = -1; // Workers don't know total size
    
    // Star topology - connection to coordinator
    comm->connection_count = 1;
    comm->connections = malloc(sizeof(int));
    comm->connections[0] = coordinator_socket;
    
    // Ring topology
    comm->left_neighbor_socket = -1;
    comm->has_left_neighbor = false;
    comm->right_neighbor_socket = -1;
    comm->has_right_neighbor = false;
    
    // Accept connection from left neighbor
    if (rank > 1) {
        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(own_port);
        
        bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
        listen(server_sock, 1);
        
        printf("[Worker %d] Waiting for left neighbor connection...\n", rank);
        comm->left_neighbor_socket = accept(server_sock, NULL, NULL);
        comm->has_left_neighbor = true;
        close(server_sock);
        printf("[Worker %d] Left neighbor connected\n", rank);
    } else if (rank == 1) {
        // Worker 1 accepts from coordinator
        int server_sock = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(own_port);
        
        bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
        listen(server_sock, 1);
        
        printf("[Worker 1] Waiting for coordinator connection...\n");
        comm->left_neighbor_socket = accept(server_sock, NULL, NULL);
        comm->has_left_neighbor = true;
        close(server_sock);
        printf("[Worker 1] Coordinator connected as left neighbor\n");
    }
    
    // Connect to right neighbor if exists
    if (right_neighbor_port > 0) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(right_neighbor_port);
        inet_pton(AF_INET, right_neighbor_ip, &addr.sin_addr);
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            comm->right_neighbor_socket = sock;
            comm->has_right_neighbor = true;
            printf("[Worker %d] Connected to right neighbor\n", rank);
        }
    }
    
    return comm;
}

void free_communicator(Communicator* comm) {
    if (comm->connections) {
        for (int i = 0; i < comm->connection_count; i++) {
            close(comm->connections[i]);
        }
        free(comm->connections);
    }
    
    if (comm->left_neighbor_socket >= 0) close(comm->left_neighbor_socket);
    if (comm->right_neighbor_socket >= 0) close(comm->right_neighbor_socket);
    
    free(comm);
}

// ==================== Communication Operations ====================

void send_int(Communicator* comm, int value, int dest) {
    int sock = -1;
    
    if (comm->is_root && dest > 0 && dest <= comm->connection_count) {
        sock = comm->connections[dest - 1];
    } else if (!comm->is_root && dest == 0) {
        sock = comm->connections[0];
    }
    
    if (sock >= 0) {
        send(sock, &value, sizeof(int), 0);
    }
}

int receive_int(Communicator* comm, int source) {
    int sock = -1;
    
    if (comm->is_root && source > 0 && source <= comm->connection_count) {
        sock = comm->connections[source - 1];
    } else if (!comm->is_root && source == 0) {
        sock = comm->connections[0];
    }
    
    int value = 0;
    if (sock >= 0) {
        recv(sock, &value, sizeof(int), 0);
    }
    
    return value;
}

void send_int_array(Communicator* comm, int* data, int length, int dest) {
    int sock = -1;
    
    if (comm->is_root && dest > 0 && dest <= comm->connection_count) {
        sock = comm->connections[dest - 1];
    } else if (!comm->is_root && dest == 0) {
        sock = comm->connections[0];
    }
    
    if (sock >= 0) {
        send(sock, &length, sizeof(int), 0);
        send(sock, data, length * sizeof(int), 0);
    }
}

int* receive_int_array(Communicator* comm, int source, int* length) {
    int sock = -1;
    
    if (comm->is_root && source > 0 && source <= comm->connection_count) {
        sock = comm->connections[source - 1];
    } else if (!comm->is_root && source == 0) {
        sock = comm->connections[0];
    }
    
    if (sock >= 0) {
        recv(sock, length, sizeof(int), 0);
        int* data = malloc(*length * sizeof(int));
        recv(sock, data, *length * sizeof(int), 0);
        return data;
    }
    
    *length = 0;
    return NULL;
}

int reduce_int(Communicator* comm, int value, int (*op)(int, int)) {
    if (comm->is_root) {
        int result = value;
        for (int i = 1; i < comm->size; i++) {
            int worker_value = receive_int(comm, i);
            result = op(result, worker_value);
        }
        return result;
    } else {
        send_int(comm, value, 0);
        return value;
    }
}

bool reduce_bool(Communicator* comm, bool value) {
    int int_value = value ? 1 : 0;
    int result = reduce_int(comm, int_value, max_op);
    return result != 0;
}

void broadcast_string(Communicator* comm, const char* message) {
    if (!comm->is_root) return;
    
    int len = strlen(message) + 1;
    for (int i = 0; i < comm->connection_count; i++) {
        send(comm->connections[i], &len, sizeof(int), 0);
        send(comm->connections[i], message, len, 0);
    }
}

char* receive_broadcast(Communicator* comm) {
    if (comm->is_root) return NULL;
    
    int len;
    recv(comm->connections[0], &len, sizeof(int), 0);
    
    char* message = malloc(len);
    recv(comm->connections[0], message, len, 0);
    
    return message;
}

void barrier(Communicator* comm) {
    if (comm->is_root) {
        // Receive from all workers
        for (int i = 1; i < comm->size; i++) {
            receive_int(comm, i);
        }
        // Send acknowledgment to all
        for (int i = 1; i < comm->size; i++) {
            send_int(comm, 1, i);
        }
    } else {
        send_int(comm, 1, 0);
        receive_int(comm, 0);
    }
}

int* scatter(Communicator* comm, int* data, int* chunk_sizes, int* my_chunk_size) {
    if (comm->is_root) {
        int index = chunk_sizes[0];
        
        // Send chunks to workers
        for (int i = 1; i < comm->size; i++) {
            send_int_array(comm, &data[index], chunk_sizes[i], i);
            index += chunk_sizes[i];
        }
        
        // Return coordinator's chunk
        *my_chunk_size = chunk_sizes[0];
        int* my_chunk = malloc(chunk_sizes[0] * sizeof(int));
        memcpy(my_chunk, data, chunk_sizes[0] * sizeof(int));
        return my_chunk;
    } else {
        return receive_int_array(comm, 0, my_chunk_size);
    }
}

int** gather(Communicator* comm, int* data, int length) {
    if (comm->is_root) {
        int** all_data = malloc(comm->size * sizeof(int*));
        
        // Store coordinator's data
        all_data[0] = malloc(length * sizeof(int));
        memcpy(all_data[0], data, length * sizeof(int));
        
        // Receive from workers
        for (int i = 1; i < comm->size; i++) {
            int worker_length;
            all_data[i] = receive_int_array(comm, i, &worker_length);
        }
        
        return all_data;
    } else {
        send_int_array(comm, data, length, 0);
        return NULL;
    }
}

// ==================== Neighbor Communication ====================

void send_to_left_neighbor(Communicator* comm, int value) {
    if (comm->has_left_neighbor) {
        send(comm->left_neighbor_socket, &value, sizeof(int), 0);
    }
}

void send_to_right_neighbor(Communicator* comm, int value) {
    if (comm->has_right_neighbor) {
        send(comm->right_neighbor_socket, &value, sizeof(int), 0);
    }
}

int receive_from_left_neighbor(Communicator* comm) {
    int value = 0;
    if (comm->has_left_neighbor) {
        recv(comm->left_neighbor_socket, &value, sizeof(int), 0);
    }
    return value;
}

int receive_from_right_neighbor(Communicator* comm) {
    int value = 0;
    if (comm->has_right_neighbor) {
        recv(comm->right_neighbor_socket, &value, sizeof(int), 0);
    }
    return value;
}

// ==================== Algorithm Implementations ====================

void* sum_algorithm(Communicator* comm, int* local_data, int length) {
    int local_sum = 0;
    for (int i = 0; i < length; i++) {
        local_sum += local_data[i];
    }
    
    printf("[Rank %d] Local sum: %d\n", comm->rank, local_sum);
    
    if (comm->is_root) {
        int result = reduce_int(comm, local_sum, sum_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_sum, sum_op);
        return NULL;
    }
}

void* min_algorithm(Communicator* comm, int* local_data, int length) {
    int local_min = local_data[0];
    for (int i = 1; i < length; i++) {
        if (local_data[i] < local_min) {
            local_min = local_data[i];
        }
    }
    
    printf("[Rank %d] Local minimum: %d\n", comm->rank, local_min);
    
    if (comm->is_root) {
        int result = reduce_int(comm, local_min, min_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_min, min_op);
        return NULL;
    }
}

void* max_algorithm(Communicator* comm, int* local_data, int length) {
    int local_max = local_data[0];
    for (int i = 1; i < length; i++) {
        if (local_data[i] > local_max) {
            local_max = local_data[i];
        }
    }
    
    printf("[Rank %d] Local maximum: %d\n", comm->rank, local_max);
    
    if (comm->is_root) {
        int result = reduce_int(comm, local_max, max_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_max, max_op);
        return NULL;
    }
}

// ==================== Parallel Sort Implementation ====================

typedef struct {
    Communicator* comm;
    int* local_data;
    int length;
} SortContext;

bool execute_phase(SortContext* ctx, const char* phase);
bool exchange_with_right(SortContext* ctx);
bool receive_from_left(SortContext* ctx);
void presort(int* data, int length);

void* sort_algorithm(Communicator* comm, int* local_data, int length) {
    SortContext ctx = {comm, local_data, length};
    
    // Phase 1: Synchronized presort
    if (comm->is_root) {
        broadcast_string(comm, "PRESORT");
    } else {
        char* cmd = receive_broadcast(comm);
        free(cmd);
    }
    
    presort(local_data, length);
    
    barrier(comm);
    printf("[Rank %d] Presort completed, waiting at barrier\n", comm->rank);
    
    // Phase 2: Odd-Even rounds
    bool global_swapped = true;
    int round = 0;
    
    while (global_swapped) {
        round++;
        printf("\n[Rank %d] ========== ROUND %d ==========\n", comm->rank, round);
        
        // ODD phase
        if (comm->is_root) {
            broadcast_string(comm, "ODD");
        } else {
            char* phase = receive_broadcast(comm);
            free(phase);
        }
        
        printf("[Rank %d] ODD PHASE - Array before: [", comm->rank);
        for (int i = 0; i < length; i++) {
            printf("%d%s", local_data[i], (i < length - 1) ? ", " : "");
        }
        printf("]\n");
        
        bool local_odd_swap = execute_phase(&ctx, "ODD");
        bool global_odd_swap = reduce_bool(comm, local_odd_swap);
        
        // EVEN phase
        if (comm->is_root) {
            broadcast_string(comm, "EVEN");
        } else {
            char* phase = receive_broadcast(comm);
            free(phase);
        }
        
        printf("[Rank %d] EVEN PHASE - Array before: [", comm->rank);
        for (int i = 0; i < length; i++) {
            printf("%d%s", local_data[i], (i < length - 1) ? ", " : "");
        }
        printf("]\n");
        
        bool local_even_swap = execute_phase(&ctx, "EVEN");
        bool global_even_swap = reduce_bool(comm, local_even_swap);
        
        // Check if we should continue
        if (comm->is_root) {
            global_swapped = global_odd_swap || global_even_swap;
            printf("[Coordinator] Round %d complete. Global swaps: %s\n", 
                   round, global_swapped ? "true" : "false");
            
            broadcast_string(comm, global_swapped ? "CONTINUE" : "DONE");
        } else {
            char* decision = receive_broadcast(comm);
            global_swapped = (strcmp(decision, "CONTINUE") == 0);
            free(decision);
        }
    }
    
    // Phase 3: Gather sorted data
    if (comm->is_root) {
        broadcast_string(comm, "GATHER");
        int** all_chunks = gather(comm, local_data, length);
        
        // Calculate total size
        int* chunk_sizes = calculate_chunk_sizes(100, comm->size); // Assuming original array size 100
        int total_size = 0;
        for (int i = 0; i < comm->size; i++) {
            total_size += chunk_sizes[i];
        }
        
        // Merge results
        int* final_result = malloc(total_size * sizeof(int));
        int index = 0;
        for (int i = 0; i < comm->size; i++) {
            memcpy(&final_result[index], all_chunks[i], chunk_sizes[i] * sizeof(int));
            index += chunk_sizes[i];
            free(all_chunks[i]);
        }
        
        free(all_chunks);
        free(chunk_sizes);
        return final_result;
    } else {
        char* gather_cmd = receive_broadcast(comm);
        free(gather_cmd);
        gather(comm, local_data, length);
        return NULL;
    }
}

bool execute_phase(SortContext* ctx, const char* phase) {
    bool is_odd_phase = (strcmp(phase, "ODD") == 0);
    bool is_active = (ctx->comm->rank % 2 == 1 && is_odd_phase) || 
                     (ctx->comm->rank % 2 == 0 && !is_odd_phase);
    
    if (is_active && ctx->comm->has_right_neighbor) {
        printf("[Rank %d] %s PHASE: I am ACTIVE, exchanging with right neighbor\n", 
               ctx->comm->rank, phase);
        return exchange_with_right(ctx);
    } else if (!is_active && ctx->comm->has_left_neighbor) {
        printf("[Rank %d] %s PHASE: I am PASSIVE, waiting for left neighbor\n", 
               ctx->comm->rank, phase);
        return receive_from_left(ctx);
    } else {
        printf("[Rank %d] %s PHASE: No neighbor to exchange with\n", 
               ctx->comm->rank, phase);
        return false;
    }
}

bool exchange_with_right(SortContext* ctx) {
    int my_value = ctx->local_data[ctx->length - 1];
    printf("[Rank %d] ACTIVE: Sending to right: %d\n", ctx->comm->rank, my_value);
    
    send_to_right_neighbor(ctx->comm, my_value);
    int neighbor_value = receive_from_right_neighbor(ctx->comm);
    printf("[Rank %d] ACTIVE: Received from right: %d\n", ctx->comm->rank, neighbor_value);
    
    if (my_value > neighbor_value) {
        printf("[Rank %d] ACTIVE: SWAP! My %d > neighbor's %d\n", 
               ctx->comm->rank, my_value, neighbor_value);
        ctx->local_data[ctx->length - 1] = neighbor_value;
        insert_from_right(ctx->local_data, ctx->length);
        
        printf("[Rank %d] ACTIVE: Array after swap: [", ctx->comm->rank);
        for (int i = 0; i < ctx->length; i++) {
            printf("%d%s", ctx->local_data[i], (i < ctx->length - 1) ? ", " : "");
        }
        printf("]\n");
        return true;
    }
    
    printf("[Rank %d] ACTIVE: NO SWAP - My %d <= neighbor's %d\n", 
           ctx->comm->rank, my_value, neighbor_value);
    return false;
}

bool receive_from_left(SortContext* ctx) {
    int received_value = receive_from_left_neighbor(ctx->comm);
    int my_value = ctx->local_data[0];
    
    printf("[Rank %d] PASSIVE: Received from left: %d\n", ctx->comm->rank, received_value);
    printf("[Rank %d] PASSIVE: My first value: %d\n", ctx->comm->rank, my_value);
    
    if (received_value > my_value) {
        printf("[Rank %d] PASSIVE: SWAP! Neighbor's %d > my %d\n", 
               ctx->comm->rank, received_value, my_value);
        send_to_left_neighbor(ctx->comm, my_value);
        printf("[Rank %d] PASSIVE: Sent back to left: %d\n", ctx->comm->rank, my_value);
        
        ctx->local_data[0] = received_value;
        insert_from_left(ctx->local_data, ctx->length);
        
        printf("[Rank %d] PASSIVE: Array after swap: [", ctx->comm->rank);
        for (int i = 0; i < ctx->length; i++) {
            printf("%d%s", ctx->local_data[i], (i < ctx->length - 1) ? ", " : "");
        }
        printf("]\n");
        return true;
    } else {
        printf("[Rank %d] PASSIVE: NO SWAP - Neighbor's %d <= my %d\n", 
               ctx->comm->rank, received_value, my_value);
        send_to_left_neighbor(ctx->comm, received_value);
        printf("[Rank %d] PASSIVE: Sent back to left: %d\n", ctx->comm->rank, received_value);
        return false;
    }
}

void presort(int* data, int length) {
    quick_sort(data, length);
    printf("[Rank %d] Presorted: [", 0); // Will be filled with actual rank
    for (int i = 0; i < length; i++) {
        printf("%d%s", data[i], (i < length - 1) ? ", " : "");
    }
    printf("]\n");
}

// ==================== Utility Functions ====================

int* create_random_array(int length) {
    int* array = malloc(length * sizeof(int));
    srand(time(NULL));
    for (int i = 0; i < length; i++) {
        array[i] = (rand() % 99) + 1;
    }
    return array;
}

int* calculate_chunk_sizes(int array_length, int num_processes) {
    int* sizes = malloc(num_processes * sizeof(int));
    int base_size = array_length / num_processes;
    int remainder = array_length % num_processes;
    
    for (int i = 0; i < num_processes; i++) {
        sizes[i] = base_size;
        if (i < remainder) {
            sizes[i]++;
        }
    }
    
    return sizes;
}

// QuickSort implementation
void quick_sort_recursive(int* arr, int low, int high);
int partition(int* arr, int low, int high);
void swap(int* a, int* b);

void quick_sort(int* arr, int length) {
    quick_sort_recursive(arr, 0, length - 1);
}

void quick_sort_recursive(int* arr, int low, int high) {
    if (low < high) {
        int pivot_index = partition(arr, low, high);
        quick_sort_recursive(arr, low, pivot_index - 1);
        quick_sort_recursive(arr, pivot_index + 1, high);
    }
}

int partition(int* arr, int low, int high) {
    // Median-of-three pivot selection
    int mid = low + (high - low) / 2;
    if (arr[mid] < arr[low]) swap(&arr[low], &arr[mid]);
    if (arr[high] < arr[low]) swap(&arr[low], &arr[high]);
    if (arr[high] < arr[mid]) swap(&arr[mid], &arr[high]);
    swap(&arr[mid], &arr[high]);
    
    int pivot = arr[high];
    int i = low - 1;
    
    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void insert_from_left(int* arr, int length) {
    int i = 0;
    int temp = arr[i];
    while (i + 1 < length && temp > arr[i + 1]) {
        arr[i] = arr[i + 1];
        i++;
    }
    arr[i] = temp;
}

void insert_from_right(int* arr, int length) {
    int i = length - 1;
    int temp = arr[i];
    while (i - 1 >= 0 && temp < arr[i - 1]) {
        arr[i] = arr[i - 1];
        i--;
    }
    arr[i] = temp;
}

// Validation functions
bool validate_sum(int calculated_sum, int* original_array, int length) {
    int expected_sum = 0;
    for (int i = 0; i < length; i++) {
        expected_sum += original_array[i];
    }
    return calculated_sum == expected_sum;
}

bool validate_min(int calculated_min, int* original_array, int length) {
    int expected_min = original_array[0];
    for (int i = 1; i < length; i++) {
        if (original_array[i] < expected_min) {
            expected_min = original_array[i];
        }
    }
    return calculated_min == expected_min;
}

bool validate_max(int calculated_max, int* original_array, int length) {
    int expected_max = original_array[0];
    for (int i = 1; i < length; i++) {
        if (original_array[i] > expected_max) {
            expected_max = original_array[i];
        }
    }
    return calculated_max == expected_max;
}

bool is_sorted(int* array, int length) {
    for (int i = 0; i < length - 1; i++) {
        if (array[i] > array[i + 1]) {
            return false;
        }
    }
    return true;
}

// Helper operation functions
int min_op(int a, int b) {
    return (a < b) ? a : b;
}

int max_op(int a, int b) {
    return (a > b) ? a : b;
}

int sum_op(int a, int b) {
    return a + b;
}

// Cleanup functions
void free_coordinator_result(CoordinatorResult* result) {
    if (result) {
        free(result->sockets);
        free(result->worker_infos);
        free(result);
    }
}

void free_worker_connection(WorkerConnection* conn) {
    if (conn) {
        free(conn);
    }
}

// Thread function declaration
void* read_command_thread(void* arg);
