#ifndef COMMON_H
#define COMMON_H

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

// Constants
#define MAX_WORKERS 100
#define BUFFER_SIZE 1024
#define MAX_COMMAND_LEN 32
#define COORDINATOR_PORT 8081

// Data Structures
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

#endif // COMMON_H