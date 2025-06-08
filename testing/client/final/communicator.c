#include "communicator.h"

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

// Communicator creation and destruction
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
    
    // Ring topology - will be connected later
    comm->left_neighbor_socket = -1;
    comm->has_left_neighbor = false;
    comm->right_neighbor_socket = -1;
    comm->has_right_neighbor = false;
    
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

// Star topology communication operations
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

// Collective operations
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
    if (comm->is_root) {
        bool result = value;
        for (int i = 1; i < comm->size; i++) {
            int worker_value = receive_int(comm, i);
            result = result || (worker_value != 0);
        }
        return result;
    } else {
        send_int(comm, value ? 1 : 0, 0);
        return value;
    }
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

// Ring topology neighbor communication
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