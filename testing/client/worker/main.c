#include "common.h"
#include "network.h"
#include "communicator.h"
#include "algorithms.h"

// Main function for the coordinator and worker processes
int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    // Check for correct number of arguments
    if (argc < 2) {
        printf("Usage:\nCoordinator: c\n");
        printf("Worker: w\n");
        return 1;
    }

    // Determine if this process is the coordinator
    bool is_coordinator = (argv[1][0] == 'c');

    char own_ip[INET_ADDRSTRLEN];
    // Get the local IP address
    if (!get_local_ip(own_ip, INET_ADDRSTRLEN)) {
        fprintf(stderr, "Failed to get local IP address\n");
        return 1;
    }

    if (is_coordinator) {
        // Coordinator-mode

        // Set up the coordinator and wait for workers to connect
        CoordinatorResult* result = setup_coordinator(own_ip, COORDINATOR_PORT);
        if (!result) {
            fprintf(stderr, "Failed to setup coordinator\n");
            return 1;
        }

        // Create communicator for the coordinator
        Communicator* comm = create_coordinator_communicator(0, result->sockets,
                                                           result->worker_count, NULL);

        // Send ready signal to workers to establish ring connections
        for (int i = 0; i < result->worker_count; i++) {
            int ready = 1;
            send(result->sockets[i], &ready, sizeof(int), 0);
        }

        // Establish ring connection to first worker if exists
        if (result->worker_count > 0) {
            sleep(1); // Give worker time to set up listener
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(result->worker_infos[0].port);
            inet_pton(AF_INET, result->worker_infos[0].ip, &addr.sin_addr);

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                comm->right_neighbor_socket = sock;
                comm->has_right_neighbor = true;
                printf("[Coordinator] Connected to right neighbor (Worker 1)\n");
            }
        }

        // Create the initial array to be distributed
        int array_length = 100;
        int* initial_array = create_random_array(array_length);
        printf("[Coordinator] Created initial array of length %d\n", array_length);

        // Execute the selected command
        printf("[Coordinator] Executing command: %s\n", result->command);

        // Distribute the array among all participants
        int* chunk_sizes = calculate_chunk_sizes(array_length, comm->size);
        int my_chunk_size;
        int* chunk = scatter(comm, initial_array, chunk_sizes, &my_chunk_size);

        printf("[Coordinator] Array distributed. My chunk: [");
        for (int i = 0; i < my_chunk_size; i++) {
            printf("%d%s", chunk[i], (i < my_chunk_size - 1) ? ", " : "");
        }
        printf("]\n");

        // Broadcast the command to all workers
        broadcast_string(comm, result->command);

        // Select the algorithm function based on the command
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

        // Display the results and validate them
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

        // Cleanup resources
        barrier(comm);
        printf("[Coordinator] Shutting down...\n");
        free(chunk);
        free(chunk_sizes);
        free(initial_array);
        free_communicator(comm);
        free_coordinator_result(result);
        printf("[Coordinator] Goodbye!\n");
    } else {
        // Worker-Modus

        // Check for coordinator IP argument
        if (argc < 3) {
            printf("Usage for Worker: w <coordinator_ip>\n");
            return 1;
        }
        const char* coordinator_ip = argv[2];

        srand(time(NULL)); // For random worker ports
        int own_port = COORDINATOR_PORT + 1 + rand() % 1000; // Dynamischer Port für Worker

        WorkerConnection* conn = connect_to_coordinator(own_ip, own_port,
                                                       coordinator_ip, COORDINATOR_PORT);
        if (!conn) {
            fprintf(stderr, "Failed to connect to coordinator\n");
            return 1;
        }

        // Wait for ready signal before establishing ring connections
        int ready;
        recv(conn->socket, &ready, sizeof(int), 0);

        // Create communicator
        Communicator* comm = create_worker_communicator(conn->id, conn->socket,
                                                       own_ip, own_port,
                                                       conn->right_neighbor_ip,
                                                       conn->right_neighbor_port);

        printf("[Worker %d] Ready and waiting for data...\n", comm->rank);

        // Receive the array chunk from the coordinator
        int chunk_length;
        int* chunk = receive_int_array(comm, 0, &chunk_length);
        printf("[Worker %d] Received chunk: [", comm->rank);
        for (int i = 0; i < chunk_length; i++) {
            printf("%d%s", chunk[i], (i < chunk_length - 1) ? ", " : "");
        }
        printf("]\n");

        // Receive the command to execute
        char* command = receive_broadcast(comm);
        printf("[Worker %d] Received command: %s\n", comm->rank, command);

        // Select the algorithm function based on the command
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

        // Execute the algorithm and free the result if needed
        if (algorithm) {
            void* result = algorithm(comm, chunk, chunk_length);
            if (result) free(result);
        }

        // Cleanup resources
        barrier(comm);
        printf("[Worker %d] Shutting down...\n", comm->rank);

        int worker_id = comm->rank;

        free(chunk);
        free(command);
        free_communicator(comm);
        free_worker_connection(conn);
        printf("[Worker %d] Worker terminated.\n", worker_id);
    }

    return 0;
}