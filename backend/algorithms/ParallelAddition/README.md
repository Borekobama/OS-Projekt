**NetworkManager.java**
Handles TCP socket setup and connection management. Sets up coordinator server to accept worker connections, manages worker registration process, and assigns unique IDs to workers.

**ParallelAddition.java**
Main application class that demonstrates distributed array summation. Creates and distributes array chunks across workers, coordinates computation phases via broadcasts, and aggregates partial results using reduction operations.

**MPICommunicator.java**
Encapsulates MPI-like communication between processes using TCP sockets. Provides methods for point-to-point communication (send/receive), collective operations (reduce, gather, scatter, broadcast), and process synchronization (barrier).

**MPIDatatype.java**
Enum defining supported data types for MPI-style communication (INT, DOUBLE, CHAR, FLOAT). Used to specify data formats in send/receive operations.

**ReduceOperation.java**
Functional interface for reduction operations with predefined implementations (SUM, MAX, MIN). Allows custom reduction logic to be applied during collective reduce operations.
