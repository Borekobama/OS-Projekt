#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "mpi.h"

void mpi_init(MPICommunicator *comm, const char *server_ip) {
    strcpy(comm->server_ip, server_ip);
    comm->socket_fd = -1;
    
    // Initialisierungsanfrage, um Rank zu erhalten
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    char request[MAX_BUFFER];
    snprintf(request, MAX_BUFFER, "POST /init HTTP/1.1\r\nHost: %s\r\nContent-Length: 0\r\n\r\n", server_ip);
    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }

    char buffer[MAX_BUFFER];
    int bytes_read = read(sock_fd, buffer, MAX_BUFFER - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            sscanf(body, "{\"rank\": %d, \"size\": %d}", &comm->rank, &comm->size);
            comm->isContributor = (comm->rank == 1);
        }
    }
    close(sock_fd);
}

int mpi_get_rank(MPICommunicator *comm) { return comm->rank; }
int mpi_get_size(MPICommunicator *comm) { return comm->size; }
int mpi_is_contributor(MPICommunicator *comm) { return comm->isContributor; }

void mpi_send(void *buffer, int count, MPIDatatype datatype, int dest, int tag, MPICommunicator *comm) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, comm->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    char request[MAX_BUFFER], body[MAX_BUFFER];
    int body_len = 0;
    body[0] = '\0';

    switch (datatype) {
        case INT: {
            int *int_buffer = (int*)buffer;
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "{\"rank\": %d, \"tag\": %d, \"data\": [", comm->rank, tag);
            for (int i = 0; i < count; i++) {
                body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "%d%s", int_buffer[i], i < count - 1 ? "," : "");
            }
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "]}");
            break;
        }
        case DOUBLE: {
            double *double_buffer = (double*)buffer;
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "{\"rank\": %d, \"tag\": %d, \"data\": [", comm->rank, tag);
            for (int i = 0; i < count; i++) {
                body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "%f%s", double_buffer[i], i < count - 1 ? "," : "");
            }
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "]}");
            break;
        }
        case CHAR: {
            char *char_buffer = (char*)buffer;
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "{\"rank\": %d, \"tag\": %d, \"data\": \"", comm->rank, tag);
            for (int i = 0; i < count; i++) {
                body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "%c", char_buffer[i]);
            }
            body_len += snprintf(body + body_len, MAX_BUFFER - body_len, "\"}");
            break;
        }
    }

    snprintf(request, MAX_BUFFER,
             "POST /message?dest=%d HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s",
             dest, comm->server_ip, body_len, body);

    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }

    char response[MAX_BUFFER];
    int bytes_read = read(sock_fd, response, MAX_BUFFER - 1);
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
    }
    close(sock_fd);
}

int mpi_receive(void *buffer, int maxCount, MPIDatatype datatype, int source, int tag, MPICommunicator *comm) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, comm->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    char request[MAX_BUFFER];
    snprintf(request, MAX_BUFFER,
             "GET /receive?source=%d&tag=%d&rank=%d HTTP/1.1\r\nHost: %s\r\n\r\n",
             source, tag, comm->rank, comm->server_ip);

    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }

    char response[MAX_BUFFER];
    int bytes_read = read(sock_fd, response, MAX_BUFFER - 1);
    if (bytes_read <= 0) { close(sock_fd); return 0; }
    response[bytes_read] = '\0';

    char *body = strstr(response, "\r\n\r\n");
    if (!body) { close(sock_fd); return 0; }
    body += 4;

    int count = 0;
    switch (datatype) {
        case INT: {
            int *int_buffer = (int*)buffer;
            char *data = strstr(body, "\"data\": [") + 8;
            char *end = strstr(data, "]");
            if (!end) { close(sock_fd); return 0; }
            *end = '\0';
            char *token = strtok(data, ",");
            while (token && count < maxCount) {
                int_buffer[count++] = atoi(token);
                token = strtok(NULL, ",");
            }
            break;
        }
        case DOUBLE: {
            double *double_buffer = (double*)buffer;
            char *data = strstr(body, "\"data\": [") + 8;
            char *end = strstr(data, "]");
            if (!end) { close(sock_fd); return 0; }
            *end = '\0';
            char *token = strtok(data, ",");
            while (token && count < maxCount) {
                double_buffer[count++] = atof(token);
                token = strtok(NULL, ",");
            }
            break;
        }
        case CHAR: {
            char *char_buffer = (char*)buffer;
            char *data = strstr(body, "\"data\": \"") + 9;
            char *end = strstr(data, "\"");
            if (!end) { close(sock_fd); return 0; }
            *end = '\0';
            strncpy(char_buffer, data, maxCount);
            count = strlen(data);
            break;
        }
    }

    close(sock_fd);
    return count;
}

void mpi_send_int(int value, int dest, MPICommunicator *comm) {
    mpi_send(&value, 1, INT, dest, TAG_DATA, comm);
}

int mpi_receive_int(int source, MPICommunicator *comm) {
    int buffer[1];
    mpi_receive(buffer, 1, INT, source, ANY_TAG, comm);
    return buffer[0];
}

int mpi_reduce(int value, ReduceOperation op, MPICommunicator *comm) {
    if (comm->isContributor) {
        int result = value;
        for (int i = 2; i <= comm->size; i++) {
            int buffer = mpi_receive_int(i, comm);
            switch (op) {
                case SUM: result += buffer; break;
                case MAX: if (buffer > result) result = buffer; break;
                case MIN: if (buffer < result) result = buffer; break;
            }
        }
        return result;
    } else {
        mpi_send_int(value, 1, comm);
        return value;
    }
}

int** mpi_gather(int *data, int count, MPICommunicator *comm) {
    if (comm->isContributor) {
        int **allData = malloc(comm->size * sizeof(int*));
        allData[0] = malloc(count * sizeof(int));
        memcpy(allData[0], data, count * sizeof(int));
        for (int i = 2; i <= comm->size; i++) {
            allData[i-1] = malloc(count * sizeof(int));
            mpi_receive(allData[i-1], count, INT, i, TAG_DATA, comm);
        }
        return allData;
    } else {
        mpi_send(data, count, INT, 1, TAG_DATA, comm);
        return NULL;
    }
}

int* mpi_scatter(int *data, int *chunkSizes, MPICommunicator *comm) {
    if (comm->isContributor) {
        int index = chunkSizes[0];
        for (int i = 2; i <= comm->size; i++) {
            int *chunk = malloc(chunkSizes[i-1] * sizeof(int));
            memcpy(chunk, data + index, chunkSizes[i-1] * sizeof(int));
            mpi_send(chunk, chunkSizes[i-1], INT, i, TAG_DATA, comm);
            free(chunk);
            index += chunkSizes[i-1];
        }
        int *result = malloc(chunkSizes[0] * sizeof(int));
        memcpy(result, data, chunkSizes[0] * sizeof(int));
        return result;
    } else {
        int *buffer = malloc(chunkSizes[comm->rank-1] * sizeof(int));
        mpi_receive(buffer, chunkSizes[comm->rank-1], INT, 1, TAG_DATA, comm);
        return buffer;
    }
}

void mpi_broadcast(const char *message, MPICommunicator *comm) {
    if (!comm->isContributor) {
        fprintf(stderr, "Only Contributor can broadcast\n");
        exit(1);
    }
    char request[MAX_BUFFER];
    snprintf(request, MAX_BUFFER,
             "POST /broadcast HTTP/1.1\r\nHost: %s\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s",
             comm->server_ip, strlen(message), message);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, comm->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }
    close(sock_fd);
}

char* mpi_receive_broadcast(MPICommunicator *comm) {
    if (comm->isContributor) {
        fprintf(stderr, "Contributor does not receive broadcasts\n");
        exit(1);
    }
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, comm->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    char request[MAX_BUFFER];
    snprintf(request, MAX_BUFFER,
             "GET /receive_broadcast?rank=%d HTTP/1.1\r\nHost: %s\r\n\r\n",
             comm->rank, comm->server_ip);

    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }

    char response[MAX_BUFFER];
    int bytes_read = read(sock_fd, response, MAX_BUFFER - 1);
    if (bytes_read <= 0) { close(sock_fd); return NULL; }
    response[bytes_read] = '\0';

    char *body = strstr(response, "\r\n\r\n");
    if (!body) { close(sock_fd); return NULL; }
    body += 4;

    char *message = malloc(strlen(body) + 1);
    strcpy(message, body);
    close(sock_fd);
    return message;
}

void mpi_barrier(MPICommunicator *comm) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("Socket creation failed"); exit(1); }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if (inet_pton(AF_INET, comm->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address"); exit(1);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(1);
    }

    char request[MAX_BUFFER];
    snprintf(request, MAX_BUFFER,
             "POST /barrier?rank=%d HTTP/1.1\r\nHost: %s\r\nContent-Length: 0\r\n\r\n",
             comm->rank, comm->server_ip);

    if (write(sock_fd, request, strlen(request)) < 0) {
        perror("Write failed"); exit(1);
    }

    char response[MAX_BUFFER];
    int bytes_read = read(sock_fd, response, MAX_BUFFER - 1);
    close(sock_fd);
}

void mpi_finalize(MPICommunicator *comm) {
    if (comm->socket_fd >= 0) {
        close(comm->socket_fd);
        comm->socket_fd = -1;
    }
}