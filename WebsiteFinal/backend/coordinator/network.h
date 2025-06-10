#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

// Function to get IP address
bool get_ip_from_socket(int sock, char* ip_buffer, size_t buffer_size);
bool get_local_ip(char* ip_buffer, size_t buffer_size);

// Thread function
void* read_command_thread(void* arg);

// Connection setup functions
CoordinatorResult* setup_coordinator(const char* ip, int port);
WorkerConnection* connect_to_coordinator(const char* worker_ip, int worker_port,
                                        const char* coordinator_ip, int coordinator_port);

// Cleanup functions
void free_coordinator_result(CoordinatorResult* result);
void free_worker_connection(WorkerConnection* conn);

#endif // NETWORK_H