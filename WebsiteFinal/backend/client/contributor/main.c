#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <curl/curl.h>

#include "common.h"
#include "network.h"
#include "communicator.h"
#include "algorithms.h"

void* coordinator_input_thread(void* arg) {
    char* command_ptr = (char*)arg;
    while (1) {
        printf("Coordinator> ");
        fflush(stdout);
        if (fgets(command_ptr, 32, stdin)) {
            command_ptr[strcspn(command_ptr, "\n")] = 0;
            if (strlen(command_ptr) > 0) break;
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        printf("Usage: c (for coordinator) or w <coordinator_ip> (for worker)\n");
        return 1;
    }

    char own_ip[INET_ADDRSTRLEN];
    if (!get_local_ip(own_ip, INET_ADDRSTRLEN)) {
        fprintf(stderr, "Could not determine local IP\n");
        return 1;
    }

    if (argv[1][0] == 'c') {
        printf("[Coordinator] Server started on %s:%d\n", own_ip, COORDINATOR_PORT);
        printf("[HTTP] Listening on port 8081...\n");

        printf("[Coordinator] Waiting for workers...\n");
        printf("\n=== Available Commands ===\n");
        printf("  SUM  - Calculate sum of array\n");
        printf("  MIN  - Find minimum of array\n");
        printf("  MAX  - Find maximum of array\n");
        printf("  SORT - Sort array using odd-even transposition\n");
        printf("===========================\n");
        printf("Enter command when all workers are connected:\n");

        CoordinatorResult* result = setup_coordinator(own_ip, COORDINATOR_PORT);
        if (!result) {
            fprintf(stderr, "Failed to set up coordinator\n");
            return 1;
        }

        Communicator* comm = create_coordinator_communicator(0, result->sockets,
                                                              result->worker_count, NULL);

        for (int i = 0; i < result->worker_count; i++) {
            int ready = 1;
            send(result->sockets[i], &ready, sizeof(int), 0);
        }

        if (result->worker_count > 0) {
            sleep(1);
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(result->worker_infos[0].port);
            inet_pton(AF_INET, result->worker_infos[0].ip, &addr.sin_addr);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                comm->right_neighbor_socket = sock;
                comm->has_right_neighbor = true;
                printf("[1)] [Coordinator] Connected to right neighbor (Worker 1)\n");
            }
        }

        printf("[Coordinator] Using array of length 100\n");
        int array_length = 100;
        int* initial_array = create_random_array(array_length);

        char command[32] = {0};
        pthread_t input_thread;
        pthread_create(&input_thread, NULL, coordinator_input_thread, command);
        pthread_join(input_thread, NULL);

        printf("[Coordinator] Command '%s' received. Stopping worker registration...\n", command);
        strncpy(result->command, command, sizeof(result->command) - 1);
        result->command[sizeof(result->command) - 1] = '\0';

        int* chunk_sizes = calculate_chunk_sizes(array_length, comm->size);
        int my_chunk_size;
        printf("[Coordinator] Scattering array...\n");
        int* chunk = scatter(comm, initial_array, chunk_sizes, &my_chunk_size);
        printf("[Coordinator] Done scattering. My chunk size: %d\n", my_chunk_size);

        broadcast_string(comm, result->command);

        AlgorithmFunc algorithm = NULL;
        if (strcasecmp(result->command, "SUM") == 0) algorithm = sum_algorithm;
        else if (strcasecmp(result->command, "MIN") == 0) algorithm = min_algorithm;
        else if (strcasecmp(result->command, "MAX") == 0) algorithm = max_algorithm;
        else if (strcasecmp(result->command, "SORT") == 0) algorithm = sort_algorithm;

        void* result_value = NULL;
        if (algorithm) {
            result_value = algorithm(comm, chunk, my_chunk_size);
        }

        if (result_value) {
            if (strcasecmp(result->command, "SUM") == 0) {
                int sum = *(int*)result_value;
                printf("[Coordinator] Final Sum: %d\n", sum);
                free(result_value);
            } else if (strcasecmp(result->command, "MIN") == 0) {
                int min = *(int*)result_value;
                printf("[Coordinator] Final Min: %d\n", min);
                free(result_value);
            } else if (strcasecmp(result->command, "MAX") == 0) {
                int max = *(int*)result_value;
                printf("[Coordinator] Final Max: %d\n", max);
                free(result_value);
            } else if (strcasecmp(result->command, "SORT") == 0) {
                int* sorted = (int*)result_value;
                printf("[Coordinator] Final sorted array: [");
                for (int i = 0; i < array_length; i++) {
                    printf("%d%s", sorted[i], (i < array_length - 1) ? ", " : "");
                }
                printf("]\n");
                free(sorted);
            }
        }

        barrier(comm);
        free(chunk);
        free(chunk_sizes);
        free(initial_array);
        free_communicator(comm);
        free_coordinator_result(result);

        printf("[Coordinator] Goodbye!\n");
    }

    return 0;
}