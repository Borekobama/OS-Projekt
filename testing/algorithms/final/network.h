#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"

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