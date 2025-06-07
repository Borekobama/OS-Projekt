#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

int main() {
    MPICommunicator comm;
    const char *server_ip = getenv("SERVER_IP") ? getenv("SERVER_IP") : "188.245.63.120";
    
    mpi_init(&comm, server_ip);
    printf("Rank: %d, Size: %d, IsContributor: %d\n", mpi_get_rank(&comm), mpi_get_size(&comm), mpi_is_contributor(&comm));

    if (mpi_is_contributor(&comm)) {
        int value = 10;
        int result = mpi_reduce(value, SUM, &comm);
        printf("Contributor: Reduced value = %d\n", result);

        int data[] = {1, 2, 3, 4, 5, 6};
        int chunkSizes[] = {2, 2, 2};
        int *chunk = mpi_scatter(data, chunkSizes, &comm);
        printf("Contributor: Scattered chunk = [%d, %d]\n", chunk[0], chunk[1]);
        free(chunk);

        int *gather_data = malloc(2 * sizeof(int));
        gather_data[0] = 10; gather_data[1] = 20;
        int **allData = mpi_gather(gather_data, 2, &comm);
        for (int i = 0; i < mpi_get_size(&comm); i++) {
            printf("Contributor: Gathered from %d: [%d, %d]\n", i, allData[i][0], allData[i][1]);
            free(allData[i]);
        }
        free(allData);
        free(gather_data);

        mpi_broadcast("Hello from Contributor", &comm);
    } else {
        int value = mpi_get_rank(&comm) * 10;
        mpi_reduce(value, SUM, &comm);

        int *chunk = mpi_scatter(NULL, NULL, &comm);
        printf("Worker %d: Scattered chunk = [%d, %d]\n", mpi_get_rank(&comm), chunk[0], chunk[1]);
        free(chunk);

        int *gather_data = malloc(2 * sizeof(int));
        gather_data[0] = mpi_get_rank(&comm) * 100; gather_data[1] = mpi_get_rank(&comm) * 200;
        mpi_gather(gather_data, 2, &comm);
        free(gather_data);

        char *message = mpi_receive_broadcast(&comm);
        printf("Worker %d: Broadcast = %s\n", mpi_get_rank(&comm), message);
        free(message);
    }

    mpi_barrier(&comm);
    mpi_finalize(&comm);
    return 0;
}