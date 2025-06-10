#ifndef COMMON_H
#define COMMON_H

// Standard C libraries
#include <stdio.h>      // Input/Output functions
#include <stdlib.h>     // General utilities: memory allocation, process control, conversions
#include <string.h>     // String handling functions
#include <unistd.h>     // POSIX operating system API (e.g., close, read, write)
#include <arpa/inet.h>  // Functions for manipulating numeric IP addresses
#include <sys/socket.h> // Core socket definitions and functions
#include <netinet/in.h> // Internet address family definitions
#include <pthread.h>    // POSIX threads for multithreading
#include <stdbool.h>    // Boolean type support
#include <time.h>       // Date and time functions
#include <errno.h>      // Error number definitions
#include <ctype.h>      // Character type functions

// Constants
#define MAX_WORKERS 100
#define BUFFER_SIZE 1024
#define MAX_COMMAND_LEN 32
#define COORDINATOR_PORT 8081

// Data structure to store information about a worker node
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    int id;
} WorkerInfo;

// Structure representing a worker's connection details in the ring topology
typedef struct {
    int socket;
    int id;
    char own_ip[INET_ADDRSTRLEN];
    int own_port;
    char right_neighbor_ip[INET_ADDRSTRLEN];
    int right_neighbor_port;
    bool has_right_neighbor;
} WorkerConnection;

// Structure holding the result of the coordinator, including worker info and command
typedef struct {
    int *sockets;
    WorkerInfo *worker_infos;
    int worker_count;
    char command[MAX_COMMAND_LEN];
} CoordinatorResult;

// Structure representing the communicator for both star and ring topologies
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

// Function pointer type for algorithm implementations
typedef void* (*AlgorithmFunc)(Communicator*, int*, int);

#endif // COMMON_H