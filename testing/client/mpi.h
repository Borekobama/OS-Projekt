#ifndef MPI_H
#define MPI_H

typedef struct {
    int rank;
    int size;
    char server_ip[16];
    int socket_fd;
} MPICommunicator;

typedef enum {
    INT,
    DOUBLE,
    CHAR
} MPIDatatype;

typedef enum {
    TAG_INIT = 1,
    TAG_PING = 2,
    TAG_UPDATE = 3,
    TAG_ELECT = 4,
    TAG_MESSAGE = 5,
    TAG_DATA = 6,
    TAG_REDUCE = 7
} MPITag;

typedef enum {
    SUM,
    MAX,
    MIN
} ReduceOperation;

void mpi_init(MPICommunicator *comm, const char *server_ip);
int mpi_get_rank(MPICommunicator *comm);
int mpi_get_size(MPICommunicator *comm);
void mpi_send(void *buffer, int count, MPIDatatype datatype, int dest, int tag, MPICommunicator *comm);
int mpi_receive(void *buffer, int maxCount, MPIDatatype datatype, int source, int tag, MPICommunicator *comm);
int mpi_reduce(int value, ReduceOperation op, MPICommunicator *comm);
int** mpi_gather(int *data, int count, MPICommunicator *comm);
int* mpi_scatter(int *data, int *chunkSizes, MPICommunicator *comm);
void mpi_broadcast(const char *message, MPICommunicator *comm);
char* mpi_receive_broadcast(MPICommunicator *comm);
void mpi_barrier(MPICommunicator *comm);
void mpi_finalize(MPICommunicator *comm);
void mpi_update_ranks(MPICommunicator *comm);
void mpi_elect_new_coordinator(MPICommunicator *comm);

#endif