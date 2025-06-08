// ==================== ParallelComputation.java ====================
import java.net.Socket;
import java.util.*;

/**
 * Main class for parallel computation framework
 * Orchestrates distributed algorithms using MPI-like communication
 */
public class ParallelComputation {

    private final Communicator comm;

    // ID des Prozesses
    private final int rank;
    private final boolean isCoordinator;

    // Unsortiertes Array. Speichert nur der Koordinator ab
    private int[] initialArray;

    // Teilarray, das der aktuelle Prozess bearbeiten soll
    private int[] chunk;

    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            System.out.println("Usage:\nCoordinator: c <ownIP> <ownPort>\nWorker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
            return;
        }

        boolean isCoordinator = args[0].equalsIgnoreCase("c");
        String ownIP = args[1];
        int ownPort = Integer.parseInt(args[2]);

        if (isCoordinator) {
            new ParallelComputation(ownIP, ownPort);
        } else {
            if (args.length < 5) {
                System.out.println("Usage for Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
                return;
            }
            String coordinatorIP = args[3];
            int coordinatorPort = Integer.parseInt(args[4]);
            new ParallelComputation(ownIP, ownPort, coordinatorIP,
                    coordinatorPort).runAsWorker();
        }
    }

    /**
     * Constructor for the coordinator process
     */
    public ParallelComputation(String ip, int port) throws Exception {
        ConnectionSetup.CoordinatorResult result = ConnectionSetup.setupCoordinator(ip, port);

        // Get first worker info for ring connection
        ConnectionSetup.WorkerInfo firstWorker = null;
        if (!result.workerInfos.isEmpty()) {
            firstWorker = result.workerInfos.get(0);
        }

        this.comm = new Communicator(0, result.workerSockets, firstWorker);
        this.rank = 0;
        this.isCoordinator = true;

        // Create initial array
        this.initialArray = Utils.createRandomArray(100);
        System.out.println("[Coordinator] Created initial array of length " + initialArray.length);

        // Execute the user's command
        executeCommand(result.command);
    }


    /**
     * Constructor for the worker process
     */
    public ParallelComputation(String workerIP, int workerPort,
                               String coordinatorIP, int coordinatorPort) throws Exception {
        ConnectionSetup.WorkerConnection connection =
                ConnectionSetup.connectToCoordinator(workerIP, workerPort, coordinatorIP, coordinatorPort);

        this.comm = new Communicator(connection.id, connection.socket,
                connection.ownIP, connection.ownPort,
                connection.rightNeighborIP, connection.rightNeighborPort);
        this.rank = connection.id;
        this.isCoordinator = false;
    }

    /**
     * Executes the specified command using the appropriate algorithm
     */
    private void executeCommand(String command) throws Exception {
        System.out.println("[Coordinator] Executing command: " + command);

        // Distribute array to workers and coordinator
        int[] chunkSizes = Utils.calculateChunkSizes(initialArray.length, comm.getSize());
        chunk = comm.scatter(initialArray, chunkSizes);
        System.out.println("[Coordinator] Array distributed. My chunk: " + Arrays.toString(chunk));

        // Broadcast command to workers
        comm.broadcast(command);

        // Create and execute the algorithm
        Algorithm algorithm = createAlgorithm(command);
        Object result = algorithm.execute(comm, chunk);

        // Display results
        if (result != null) {
            displayResult(command, result);
        }

        // Shutdown wenn alle Teilnehmer bereit sind.
        comm.barrier();
        System.out.println("[Coordinator] Shutting down...");
        comm.close();
        System.out.println("[Coordinator] Goodbye!");
    }

    /**
     * Worker main loop
     */
    private void runAsWorker() throws Exception {
        System.out.println("[Worker " + rank + "] Ready and waiting for data...");

        try {
            // Receive array chunk
            chunk = comm.receiveScatterData();
            System.out.println("[Worker " + rank + "] Received chunk: " + Arrays.toString(chunk));

            // Receive command
            String command = comm.receiveBroadcast();
            System.out.println("[Worker " + rank + "] Received command: " + command);

            // Create and execute the algorithm
            Algorithm algorithm = createAlgorithm(command);
            algorithm.execute(comm, chunk);

            // Warte bis Koordinator bereit ist für shutdown
            comm.barrier();
            System.out.println("[Worker " + rank + "] Shutting down...");

            comm.close();
            System.out.println("[Worker " + rank + "] Worker terminated.");
        } catch (Exception e) {
            System.err.println("[Worker " + rank + "] Error: " + e.getMessage());
            e.printStackTrace();
        }
    }

    /**
     * Create algorithm instance based on command
     */
    private Algorithm createAlgorithm(String command) {
        return switch (command.toUpperCase()) {
            case "SUM" -> new Operations.Sum();
            case "MIN" -> new Operations.Min();
            case "MAX" -> new Operations.Max();
            case "SORT" -> new ParallelSort();
            default -> throw new IllegalArgumentException(
                    "Unknown algorithm: " + command);
        };
    }

    /**
     * Display results based on algorithm type
     */
    private void displayResult(String command, Object result) {
        String cmd = command.toUpperCase();
        switch (cmd) {
            case "SUM" -> {
                System.out.println("[Coordinator] Final Sum: " + result);
                System.out.println(
                        "[Coordinator] Correct? " + Utils.validateSum(
                                (Integer) result, initialArray));
            }
            case "MIN" -> {
                System.out.println("[Coordinator] Final Min: " + result);
                System.out.println(
                        "[Coordinator] Correct? " + Utils.validateMin(
                                (Integer) result, initialArray));
            }
            case "MAX" -> {
                System.out.println("[Coordinator] Final Max: " + result);
                System.out.println(
                        "[Coordinator] Correct? " + Utils.validateMax(
                                (Integer) result, initialArray));
            }
            case "SORT" -> {
                @SuppressWarnings("unchecked")
                List<Integer> sorted = (List<Integer>) result;
                System.out.println(
                        "[Coordinator] Final sorted array: " + sorted);
                System.out.println(
                        "[Coordinator] Correctly sorted? " + Utils.isSorted(
                                sorted));
            }
        }
    }
}
