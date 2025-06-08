#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "common.h"

// Communicator creation and destruction
Communicator* create_coordinator_communicator(int rank, int* worker_sockets, 
                                            int worker_count, WorkerInfo* first_worker);
Communicator* create_worker_communicator(int rank, int coordinator_socket,
                                       const char* own_ip, int own_port,
                                       const char* right_neighbor_ip, int right_neighbor_port);
void free_communicator(Communicator* comm);

// Star topology communication operations
void send_int(Communicator* comm, int value, int dest);
int receive_int(Communicator* comm, int source);
void send_int_array(Communicator* comm, int* data, int length, int dest);
int* receive_int_array(Communicator* comm, int source, int* length);

// Collective operations
int reduce_int(Communicator* comm, int value, int (*op)(int, int));
bool reduce_bool(Communicator* comm, bool value);
void broadcast_string(Communicator* comm, const char* message);
char* receive_broadcast(Communicator* comm);
void barrier(Communicator* comm);
int* scatter(Communicator* comm, int* data, int* chunk_sizes, int* my_chunk_size);
int** gather(Communicator* comm, int* data, int length);

// Ring topology neighbor communication
void send_to_left_neighbor(Communicator* comm, int value);
void send_to_right_neighbor(Communicator* comm, int value);
int receive_from_left_neighbor(Communicator* comm);
int receive_from_right_neighbor(Communicator* comm);

// Helper operation functions
int min_op(int a, int b);
int max_op(int a, int b);
int sum_op(int a, int b);

#endif // COMMUNICATOR_H