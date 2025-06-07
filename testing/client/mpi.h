#ifndef MPI_H
#define MPI_H

#define ANY_TAG -1
#define TAG_DATA 0
#define TAG_REDUCE 1
#define MAX_BUFFER 1024

typedef enum {
    INT,
    DOUBLE,
    CHAR
} MPIDatatype;

typedef enum {
    SUM,
    MAX,
    MIN
} ReduceOperation;

typedef struct {
    int rank;           // Prozess-ID (1 = Contributor, >1 = Worker)
    int size;           // Anzahl der Prozesse (nur für Contributor bekannt)
    int isContributor;  // 1 wenn Contributor, 0 wenn Worker
    char server_ip[16]; // Server-IP (z. B. "188.245.63.120")
    int socket_fd;      // Socket für Kommunikation mit dem Server
} MPICommunicator;

// Funktionen
void mpi_init(MPICommunicator *comm, const char *server_ip);
int mpi_get_rank(MPICommunicator *comm);
int mpi_get_size(MPICommunicator *comm);
int mpi_is_contributor(MPICommunicator *comm);
void mpi_send(void *buffer, int count, MPIDatatype datatype, int dest, int tag, MPICommunicator *comm);
int mpi_receive(void *buffer, int maxCount, MPIDatatype datatype, int source, int tag, MPICommunicator *comm);
void mpi_send_int(int value, int dest, MPICommunicator *comm);
int mpi_receive_int(int source, MPICommunicator *comm);
int mpi_reduce(int value, ReduceOperation op, MPICommunicator *comm);
int** mpi_gather(int *data, int count, MPICommunicator *comm);
int* mpi_scatter(int *data, int *chunkSizes, MPICommunicator *comm);
void mpi_broadcast(const char *message, MPICommunicator *comm);
char* mpi_receive_broadcast(MPICommunicator *comm);
void mpi_barrier(MPICommunicator *comm);
void mpi_finalize(MPICommunicator *comm);

#endif