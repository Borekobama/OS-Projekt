#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#include "common.h"
#include "network.h"
#include "communicator.h"
#include "algorithms.h"

void register_with_ui_backend(const char* id) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    const char* url = "http://188.245.63.120:3000/register";
    char json[256];
    snprintf(json, sizeof(json),
        "{\"id\":\"%s\",\"name\":\"%s\",\"status\":\"connected\"}",
        id, id);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Registration failed: %s\n", curl_easy_strerror(res));
    } else {
        printf("[Worker] Registered with backend as %s\n", id);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3 || argv[1][0] != 'w') {
        printf("Usage: w <coordinator_ip>\n");
        return 1;
    }

    const char* coordinator_ip = argv[2];
    char own_ip[INET_ADDRSTRLEN];
    if (!get_local_ip(own_ip, INET_ADDRSTRLEN)) {
        fprintf(stderr, "Failed to get local IP address\n");
        return 1;
    }

    srand(time(NULL));
    int own_port = COORDINATOR_PORT + 1 + rand() % 1000;

    char worker_id[64];
    snprintf(worker_id, sizeof(worker_id), "%s:%d", own_ip, own_port);
    register_with_ui_backend(worker_id);

    WorkerConnection* conn = connect_to_coordinator(own_ip, own_port,
                                                    coordinator_ip, COORDINATOR_PORT);
    if (!conn) {
        fprintf(stderr, "Failed to connect to coordinator\n");
        return 1;
    }

    int ready;
    recv(conn->socket, &ready, sizeof(int), 0);

    Communicator* comm = create_worker_communicator(conn->id, conn->socket,
                                                    own_ip, own_port,
                                                    conn->right_neighbor_ip,
                                                    conn->right_neighbor_port);

    printf("[Worker %d] Ready and waiting for data...\n", comm->rank);

    int chunk_length;
    int* chunk = receive_int_array(comm, 0, &chunk_length);
    printf("[Worker %d] Received chunk of size %d\n", comm->rank, chunk_length);

    char* command = receive_broadcast(comm);
    printf("[Worker %d] Received command: %s\n", comm->rank, command);

    AlgorithmFunc algorithm = NULL;
    if (strcasecmp(command, "SUM") == 0) {
        algorithm = sum_algorithm;
    } else if (strcasecmp(command, "MIN") == 0) {
        algorithm = min_algorithm;
    } else if (strcasecmp(command, "MAX") == 0) {
        algorithm = max_algorithm;
    } else if (strcasecmp(command, "SORT") == 0) {
        algorithm = sort_algorithm;
    } else {
        fprintf(stderr, "[Worker %d] Unknown command: %s\n", comm->rank, command);
    }

    if (algorithm) {
        printf("[Worker %d] Executing algorithm: %s\n", comm->rank, command);
        void* result = algorithm(comm, chunk, chunk_length);
        if (result) {
            free(result);
        } else {
            printf("[Worker %d] Algorithm returned no result.\n", comm->rank);
        }
    }

    barrier(comm);
    printf("[Worker %d] Shutting down...\n", comm->rank);

    free(chunk);
    free(command);
    free_communicator(comm);
    free_worker_connection(conn);
    return 0;
}