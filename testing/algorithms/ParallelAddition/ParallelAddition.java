// ==================== ParallelAddition.java ====================
import java.net.Socket;
import java.util.*;

/**
 * The ParallelAddition class demonstrates distributed summation of an integer array
 * using an MPI-like communication model with one coordinator and multiple workers.
 */
public class ParallelAddition {
    private static final int ARRAY_LENGTH = 85;

    private static final Random random = new Random();

    private final MPICommunicator comm;
    private final int rank;
    private final boolean isCoordinator;

    private int[] initialArray;
    private int[] chunk;
    private int sum;

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
     * Logic executed by the coordinator:
     * - Creates and scatters an array
     * - Broadcasts START signal
     * - Reduces partial sums
     * - Verifies correctness and shuts down all nodes
     *
     * @throws Exception if communication fails.
     */
    private void runAsCoordinator() throws Exception {
        // Create array
        initialArray = createArray(ARRAY_LENGTH);

        // Distribute chunks (scatter)
        int[] chunkSizes = calculateChunkSizes();
        chunk = comm.scatter(initialArray, chunkSizes);
        logArray();

        // Start computation
        comm.broadcast("START");
        calculateSum();

        // Collect results and calculate final sum
        int finalSum = comm.reduce(sum, ReduceOperation.SUM);

        System.out.println("[Coordinator] Final Sum: " + finalSum);
        System.out.println("[Coordinator] Correct? " + isSumCorrect(finalSum, initialArray));

        // Graceful shutdown
        comm.broadcast("SHUTDOWN");
        comm.close();
    }

    /**
     * Logic executed by the worker:
     * - Receives chunk
     * - Waits for START signal
     * - Computes partial sum
     * - Participates in reduction
     * - Waits for shutdown
     *
     * @throws Exception if communication fails.
     */
    private void runAsWorker() throws Exception {
        // Receive chunk from coordinator
        chunk = comm.receiveIntArray(0);
        logArray();

        // Wait for START signal
        String signal = comm.receiveBroadcast();
        if (signal.equalsIgnoreCase("START")) {
            calculateSum();
        }

        // Send computed partial sum to coordinator
        comm.reduce(sum, ReduceOperation.SUM);

        // Graceful shutdown required to avoid deadlocks/race conditions
        String finish = comm.receiveBroadcast();
        if(finish.equalsIgnoreCase("SHUTDOWN")){
            comm.close();
        } else{
            throw new IllegalArgumentException("Invalid Broadcast Message");
        }
    }

    /**
     * Computes the local sum of the chunk.
     */
    private void calculateSum() {
        sum = Arrays.stream(chunk).sum();
        System.out.println("[Rank " + rank + "]: Partial sum = " + sum);
    }

    /**
     * Calculates the sizes of chunks to distribute the array fairly across processes.
     *
     * @return An array representing how many elements each process will receive.
     */
    private int[] calculateChunkSizes() {
        int numProcesses = comm.getSize();
        int baseSize = ARRAY_LENGTH / numProcesses;
        int remainder = ARRAY_LENGTH % numProcesses;

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
     * Logs the content of the local chunk to the console.
     */
    private void logArray() {
        System.out.println("[Rank " + rank + "]: Chunk " + Arrays.toString(chunk));
    }

}
