// ==================== NetworkManager.java ====================
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * Encapsulates the network setup, separated from the communication logic.
 */
public class NetworkManager {

    /**
     * Starts a server and waits for worker registrations.
     * Now stops accepting connections when user is ready to start commands.
     *
     * @param ip   The IP address of the coordinator.
     * @param port The port the coordinator listens on.
     * @return A list of connected worker sockets.
     * @throws IOException If the server socket fails or reading from input fails.
     */
    public static List<Socket> setupCoordinatorNetwork(String ip, int port) throws IOException {
        ServerSocket serverSocket = new ServerSocket(port);
        List<Socket> workerSockets = new ArrayList<>();

        System.out.println("[Coordinator] Server started on " + ip + ":" + port);
        System.out.println("[Coordinator] Waiting for workers...");
        System.out.println("[Coordinator] Press Enter when all workers are connected to start command interface.");

        // Background thread to detect Enter key press for termination
        Thread stopper = new Thread(() -> {
            try {
                new BufferedReader(new InputStreamReader(System.in)).readLine();
                System.out.println("[Coordinator] Stopping worker registration...");
                serverSocket.close();
            } catch (IOException ignored) {}
        });
        stopper.start();

        int currentID = 1; // Worker IDs start at 1
        while (true) {
            try {
                Socket worker = serverSocket.accept();

                // Set up streams for registration
                BufferedReader in = new BufferedReader(new InputStreamReader(worker.getInputStream()));
                DataOutputStream dos = new DataOutputStream(worker.getOutputStream());

                // Read registration message
                String line = in.readLine();

                if (line != null && line.startsWith("REGISTRATION:")) {
                    String[] parts = line.substring("REGISTRATION:".length()).split(":");
                    String workerIP = parts[0];
                    int workerPort = Integer.parseInt(parts[1]);

                    workerSockets.add(worker);
                    System.out.println("[Coordinator] Worker " + currentID + " registered from " + workerIP + ":" + workerPort);

                    // Send worker its assigned ID
                    dos.writeInt(currentID);
                    dos.flush();

                    // DON'T send initial chunk data here - workers will receive chunks via scatter when NEW command is executed

                    currentID++;

                    System.out.println("[Coordinator] Total workers connected: " + workerSockets.size());
                }
            } catch (IOException e) {
                break; // Enter key was pressed or server was closed
            }
        }

        System.out.println("[Coordinator] Network setup complete with " + workerSockets.size() + " workers.");
        return workerSockets;
    }

    /**
     * Connects a worker to the coordinator.
     *
     * @param workerIP        IP address of the worker.
     * @param workerPort      Port number of the worker.
     * @param coordinatorIP   IP address of the coordinator.
     * @param coordinatorPort Port number of the coordinator.
     * @return A WorkerConnection object containing the socket and assigned worker ID.
     * @throws IOException If the connection to the coordinator fails.
     */
    public static WorkerConnection connectToCoordinator(String workerIP, int workerPort,
                                                        String coordinatorIP, int coordinatorPort) throws IOException {
        System.out.println("[Worker] Connecting to coordinator at " + coordinatorIP + ":" + coordinatorPort + "...");

        Socket coordinatorSocket = new Socket(coordinatorIP, coordinatorPort);
        PrintWriter out = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        DataInputStream dis = new DataInputStream(coordinatorSocket.getInputStream());

        // Send registration
        out.println("REGISTRATION:" + workerIP + ":" + workerPort);

        // Receive assigned worker ID
        int workerId = dis.readInt();

        // DON'T read chunk data here - workers will receive chunks via scatter when NEW command is executed

        System.out.println("[Worker] Successfully connected with ID: " + workerId);

        return new WorkerConnection(coordinatorSocket, workerId);
    }

    /**
     * Represents a successful connection of a worker to the coordinator.
     */
    public static class WorkerConnection {
        public final Socket socket;
        public final int id;

        /**
         * Constructs a WorkerConnection with the given socket and assigned ID.
         *
         * @param socket The socket connected to the coordinator.
         * @param id     The unique ID assigned to the worker.
         */
        public WorkerConnection(Socket socket, int id) {
            this.socket = socket;
            this.id = id;
        }
    }
}