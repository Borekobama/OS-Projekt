// ==================== ParallelAddition.java ====================
import java.net.Socket;
import java.util.*;

/**
 * The ParallelAddition class demonstrates distributed summation of an integer array
 * using an MPI-like communication model with one coordinator and multiple workers.
 */
public class ParallelAddition {
    private static final Random random = new Random();

    private final MPICommunicator comm;
    private final int rank;
    private final boolean isCoordinator;

    private int[] initialArray;
    private int[] chunk;
    private int sum;
    private int min;
    private int max;

    // Command interface state
    private boolean networkBusy = false;
    private Scanner commandScanner;

    /**
     * Entry point of the program. Starts either a coordinator or a worker based on arguments.
     *
     * @param args Command-line arguments.
     * @throws Exception if any network or communication error occurs.
     */
    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            System.out.println("Usage:\nCoordinator: c <ownIP> <ownPort>\nWorker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
            return;
        }

        boolean isCoordinator = args[0].equalsIgnoreCase("c");
        String ownIP = args[1];
        int ownPort = Integer.parseInt(args[2]);

        if (isCoordinator) {
            new ParallelAddition(ownIP, ownPort).start();
        } else {
            if (args.length < 5) {
                System.out.println("Usage for Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
                return;
            }
            String coordinatorIP = args[3];
            int coordinatorPort = Integer.parseInt(args[4]);
            new ParallelAddition(ownIP, ownPort, coordinatorIP, coordinatorPort).start();
        }
    }

    /**
     * Constructor for the coordinator process.
     *
     * @param ip   IP address of the coordinator.
     * @param port Port on which the coordinator listens.
     * @throws Exception if network setup fails.
     */
    public ParallelAddition(String ip, int port) throws Exception {
        List<Socket> workerSockets = NetworkManager.setupCoordinatorNetwork(ip, port);
        this.comm = new MPICommunicator(0, workerSockets);
        this.rank = 0;
        this.isCoordinator = true;
        this.commandScanner = new Scanner(System.in);
    }

    /**
     * Constructor for the worker process.
     *
     * @param workerIP       The worker's own IP.
     * @param workerPort     The worker's own port.
     * @param coordinatorIP  IP address of the coordinator.
     * @param coordinatorPort Port of the coordinator.
     * @throws Exception if connection to coordinator fails.
     */
    public ParallelAddition(String workerIP, int workerPort,
                            String coordinatorIP, int coordinatorPort) throws Exception {
        NetworkManager.WorkerConnection connection =
                NetworkManager.connectToCoordinator(workerIP, workerPort, coordinatorIP, coordinatorPort);
        this.comm = new MPICommunicator(connection.id, connection.socket);
        this.rank = connection.id;
        this.isCoordinator = false;
    }

    /**
     * Starts execution based on the role (coordinator or worker).
     *
     * @throws Exception if communication or processing fails.
     */
    public void start() throws Exception {
        if (isCoordinator) {
            runAsCoordinator();
        } else {
            runAsWorker();
        }
    }

    /**
     * Logic executed by the coordinator with command interface:
     * - Waits for user commands
     * - Executes operations based on commands
     * - Manages network state
     *
     * @throws Exception if communication fails.
     */
    private void runAsCoordinator() throws Exception {
        System.out.println("\n=== Coordinator Command Interface ===");
        System.out.println("Available commands:");
        System.out.println("  NEW <length>  - Create new array with specified length");
        System.out.println("  SUM          - Calculate sum of current array");
        System.out.println("  MIN          - Find minimum of current array");
        System.out.println("  MAX          - Find maximum of current array");
        System.out.println("  EXIT         - Shutdown program");
        System.out.println("=====================================\n");

        boolean running = true;
        while (running) {
            if (!networkBusy) {
                System.out.print("Coordinator> ");
                String command = commandScanner.nextLine().trim().toUpperCase();

                if (command.isEmpty()) {
                    continue;
                }

                String[] parts = command.split("\\s+");
                String operation = parts[0];

                switch (operation) {
                    case "NEW":
                        if (parts.length < 2) {
                            System.out.println("Usage: NEW <length>");
                            break;
                        }
                        try {
                            int length = Integer.parseInt(parts[1]);
                            if (length <= 0) {
                                System.out.println("Array length must be positive!");
                                break;
                            }
                            executeNewArray(length);
                        } catch (NumberFormatException e) {
                            System.out.println("Invalid length: " + parts[1]);
                        }
                        break;

                    case "SUM":
                        if (initialArray == null) {
                            System.out.println("No array exists! Use NEW <length> first.");
                            break;
                        }
                        executeSum();
                        break;

                    case "MIN":
                        if (initialArray == null) {
                            System.out.println("No array exists! Use NEW <length> first.");
                            break;
                        }
                        executeMin();
                        break;

                    case "MAX":
                        if (initialArray == null) {
                            System.out.println("No array exists! Use NEW <length> first.");
                            break;
                        }
                        executeMax();
                        break;

                    case "EXIT":
                        running = false;
                        executeShutdown();
                        break;

                    default:
                        System.out.println("Unknown command: " + operation);
                        System.out.println("Available: NEW <length>, SUM, MIN, MAX, EXIT");
                        break;
                }
            } else {
                Thread.sleep(100); // Short pause when network is busy
            }
        }

        commandScanner.close();
    }

    /**
     * Executes NEW command - creates new array and distributes to workers
     */
    private void executeNewArray(int length) throws Exception {
        networkBusy = true;
        System.out.println("[Coordinator] Creating new array of length " + length + "...");

        // Create and distribute new array
        initialArray = createArray(length);
        int[] chunkSizes = calculateChunkSizes(length);

        // First broadcast NEW command to workers so they know to expect data
        comm.broadcast("NEW");

        // Then scatter the data
        chunk = comm.scatter(initialArray, chunkSizes);

        System.out.println("[Coordinator] Array created and distributed.");
        System.out.println("[Coordinator] My chunk: " + Arrays.toString(chunk));

        networkBusy = false;
    }

    /**
     * Executes SUM command - calculates sum across all processes
     */
    private void executeSum() throws Exception {
        networkBusy = true;
        System.out.println("[Coordinator] Calculating sum...");

        comm.broadcast("SUM");
        calculateSum();

        int finalSum = comm.reduce(sum, ReduceOperation.SUM);

        System.out.println("[Coordinator] Final Sum: " + finalSum);
        System.out.println("[Coordinator] Correct? " + isSumCorrect(finalSum, initialArray));

        networkBusy = false;
    }

    /**
     * Executes MIN command - finds minimum across all processes
     */
    private void executeMin() throws Exception {
        networkBusy = true;
        System.out.println("[Coordinator] Finding minimum...");

        comm.broadcast("MIN");
        calculateMin();

        int finalMin = comm.reduce(min, ReduceOperation.MIN);

        System.out.println("[Coordinator] Final Min: " + finalMin);
        System.out.println("[Coordinator] Correct? " + isMinCorrect(finalMin, initialArray));

        networkBusy = false;
    }

    /**
     * Executes MAX command - finds maximum across all processes
     */
    private void executeMax() throws Exception {
        networkBusy = true;
        System.out.println("[Coordinator] Finding maximum...");

        comm.broadcast("MAX");
        calculateMax();

        int finalMax = comm.reduce(max, ReduceOperation.MAX);

        System.out.println("[Coordinator] Final Max: " + finalMax);
        System.out.println("[Coordinator] Correct? " + isMaxCorrect(finalMax, initialArray));

        networkBusy = false;
    }

    /**
     * Executes EXIT command - graceful shutdown
     */
    private void executeShutdown() throws Exception {
        networkBusy = true;
        System.out.println("[Coordinator] Shutting down...");

        comm.broadcast("SHUTDOWN");
        comm.close();

        System.out.println("[Coordinator] Goodbye!");
    }

    /**
     * Logic executed by the worker:
     * - Waits for operation commands (NEW, SUM, MIN, MAX)
     * - Receives chunks when NEW command is executed
     * - Participates in reductions
     * - Handles shutdown
     *
     * @throws Exception if communication fails.
     */
    private void runAsWorker() throws Exception {
        System.out.println("[Worker " + rank + "] Ready and waiting for commands...");

        boolean running = true;
        while (running) {
            try {
                // Wait for command from coordinator
                String command = comm.receiveBroadcast();
                System.out.println("[Worker " + rank + "] Received command: " + command);

                switch (command.toUpperCase()) {
                    case "NEW":
                        // Receive new chunk data via scatter
                        chunk = comm.receiveScatterData();
                        System.out.println("[Worker " + rank + "] Received chunk: " + Arrays.toString(chunk));
                        break;

                    case "SUM":
                        calculateSum();
                        comm.reduce(sum, ReduceOperation.SUM);
                        break;

                    case "MIN":
                        calculateMin();
                        comm.reduce(min, ReduceOperation.MIN);
                        break;

                    case "MAX":
                        calculateMax();
                        comm.reduce(max, ReduceOperation.MAX);
                        break;

                    case "SHUTDOWN":
                        running = false;
                        comm.close();
                        System.out.println("[Worker " + rank + "] Shutting down...");
                        break;

                    default:
                        System.out.println("[Worker " + rank + "] Unknown command: " + command);
                        break;
                }
            } catch (Exception e) {
                System.err.println("[Worker " + rank + "] Error: " + e.getMessage());
                break;
            }
        }
    }

    /**
     * Computes the local sum of the chunk.
     */
    private void calculateSum() {
        if (chunk == null || chunk.length == 0) {
            sum = 0;
            return;
        }

        int localSum = 0;
        for(int i : chunk){
            localSum += i;
        }
        sum = localSum;
        System.out.println("[Rank " + rank + "]: Partial sum = " + sum);
    }

    /**
     * Computes the local minimum of the chunk.
     */
    private void calculateMin() {
        if (chunk == null || chunk.length == 0) {
            min = Integer.MAX_VALUE;
            return;
        }

        int localMin = chunk[0];
        for (int i : chunk) {
            if (i < localMin) {
                localMin = i;
            }
        }
        min = localMin;
        System.out.println("[Rank " + rank + "]: Minimum = " + min);
    }

    /**
     * Computes the local maximum of the chunk.
     */
    private void calculateMax() {
        if (chunk == null || chunk.length == 0) {
            max = Integer.MIN_VALUE;
            return;
        }

        int localMax = chunk[0];
        for (int i : chunk) {
            if (i > localMax) {
                localMax = i;
            }
        }
        max = localMax;
        System.out.println("[Rank " + rank + "]: Maximum = " + max);
    }

    /**
     * Calculates the sizes of chunks to distribute the array fairly across processes.
     *
     * @param arrayLength The length of the array to distribute
     * @return An array representing how many elements each process will receive.
     */
    private int[] calculateChunkSizes(int arrayLength) {
        int numProcesses = comm.getSize();
        int baseSize = arrayLength / numProcesses;
        int remainder = arrayLength % numProcesses;

        int[] sizes = new int[numProcesses];
        for (int i = 0; i < numProcesses; i++) {
            sizes[i] = baseSize + (remainder-- > 0 ? 1 : 0);
        }
        return sizes;
    }

    /**
     * Creates an array of random integers between 1 and 10.
     *
     * @param length The desired length of the array.
     * @return A randomly initialized integer array.
     */
    public static int[] createArray(int length) {
        int[] array = new int[length];
        for (int i = 0; i < length; i++) {
            array[i] = random.nextInt(10) + 1; // values 1–10
        }
        return array;
    }

    /**
     * Checks whether the computed sum matches the expected sum.
     *
     * @param calculatedSum The result from reduction.
     * @param originalArray The original full array.
     * @return True if the sum is correct, false otherwise.
     */
    public static boolean isSumCorrect(int calculatedSum, int[] originalArray) {
        int expectedSum = 0;
        for (int value : originalArray) {
            expectedSum += value;
        }
        return calculatedSum == expectedSum;
    }

    /**
     * Checks whether the computed minimum matches the expected minimum.
     */
    public static boolean isMinCorrect(int calculatedMin, int[] originalArray) {
        int expectedMin = originalArray[0];
        for (int i : originalArray) {
            if (i < expectedMin) {
                expectedMin = i;
            }
        }
        return calculatedMin == expectedMin;
    }

    /**
     * Checks whether the computed maximum matches the expected maximum.
     */
    public static boolean isMaxCorrect(int calculatedMax, int[] originalArray) {
        int expectedMax = originalArray[0];
        for (int i : originalArray) {
            if (i > expectedMax) {
                expectedMax = i;
            }
        }
        return calculatedMax == expectedMax;
    }

    /**
     * Logs the content of the local chunk to the console.
     */
    private void logArray() {
        System.out.println("[Rank " + rank + "]: Chunk " + Arrays.toString(chunk));
    }
}