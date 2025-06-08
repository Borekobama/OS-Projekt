// ==================== ConnectionSetup.java ====================
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * Handles network setup and connection establishment
 */
public class ConnectionSetup {

    /**
     * Worker information for neighbor connections
     */
    public static class WorkerInfo {
        public final String ip;
        public final int port;
        public final int id;

        public WorkerInfo(String ip, int port, int id) {
            this.ip = ip;
            this.port = port;
            this.id = id;
        }
    }

    /**
     * Setup coordinator and wait for worker connections
     */
    public static CoordinatorResult setupCoordinator(String ip, int port) throws IOException {
        ServerSocket serverSocket = new ServerSocket(port);
        List<Socket> workerSockets = new ArrayList<>();
        List<WorkerInfo> workerInfos = new ArrayList<>();

        System.out.println(
                "[Coordinator] Server started on " + ip + ":" + port + "\n" +
                        "[Coordinator] Waiting for workers...\n" +
                        "\n=== Available Commands ===\n" +
                        "  SUM  - Calculate sum of array\n" +
                        "  MIN  - Find minimum of array\n" +
                        "  MAX  - Find maximum of array\n" +
                        "  SORT - Sort array using odd-even transposition\n" +
                        "===========================\n" +
                        "Enter command when all workers are connected:"
        );


        // Stopper Thread. Schliesst das Netzwerk, also akzeptiert keine
        // neuen Worker mehr, wenn der User den Command eingibt.
        final String[] userCommand = {null};
        Thread commandReader = new Thread(() -> {
            try {
                BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
                while (userCommand[0] == null) {
                    System.out.print("Coordinator> ");
                    String input = reader.readLine();
                    if (input != null) {
                        input = input.trim().toUpperCase();
                        if (input.equals("SUM") || input.equals("MIN") || input.equals("MAX") || input.equals("SORT")) {
                            userCommand[0] = input;
                            System.out.println("[Coordinator] Command '" + input + "' received. Stopping worker registration...");
                            serverSocket.close();
                            break;
                        } else {
                            System.out.println("Invalid command. Available: SUM, MIN, MAX, SORT");
                        }
                    }
                }
            } catch (IOException e) {
                // Expected when serverSocket.close() is called
            }
        });
        commandReader.start();

        int currentID = 1; // Worker IDs start at 1
        while (userCommand[0] == null) {
            try {
                Socket worker = serverSocket.accept();

                // Set up streams for registration
                BufferedReader in = new BufferedReader(new InputStreamReader(worker.getInputStream()));
                DataOutputStream dos = new DataOutputStream(worker.getOutputStream());

                // Read registration message
                String line = in.readLine();

                if (line != null && line.startsWith("REGISTRATION:")) {
                    String[] parts = line.substring("REGISTRATION:".length()).split(":");

                    // Lese IP & Port des Workers aus
                    String workerIP = parts[0];
                    int workerPort = Integer.parseInt(parts[1]);

                    workerSockets.add(worker);
                    workerInfos.add(new WorkerInfo(workerIP, workerPort, currentID));
                    System.out.println("[Coordinator] Worker " + currentID + " registered from " + workerIP + ":" + workerPort);

                    // Send worker its assigned ID
                    dos.writeInt(currentID);
                    dos.flush();

                    currentID++;

                    System.out.println("[Coordinator] Total workers connected: " + workerSockets.size());
                    System.out.print("Coordinator> "); // Prompt again
                }
            } catch (IOException e) {
                break; // Command was entered and server was closed
            }
        }

        // Send neighbor information to all workers
        for (int i = 0; i < workerSockets.size(); i++) {
            DataOutputStream dos = new DataOutputStream(workerSockets.get(i).getOutputStream());

            // Send right neighbor info (if exists)
            if (i < workerSockets.size() - 1) {
                WorkerInfo rightNeighbor = workerInfos.get(i + 1);
                dos.writeBoolean(true); // has right neighbor
                dos.writeUTF(rightNeighbor.ip);
                dos.writeInt(rightNeighbor.port);
            } else {
                dos.writeBoolean(false); // no right neighbor
            }
            dos.flush();
        }

        // Wait for command reader thread to finish
        try {
            commandReader.join(1000);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        // Lese den Befehl des Users aus.
        String finalCommand = userCommand[0];
        if (finalCommand == null) {
            finalCommand = "SUM";
        }

        System.out.println("[Coordinator] Network setup complete with " + workerSockets.size() + " workers.");
        System.out.println("[Coordinator] Will execute command: " + finalCommand);

        return new CoordinatorResult(workerSockets, workerInfos, finalCommand);
    }

    /**
     * Connect worker to coordinator
     */
    public static WorkerConnection connectToCoordinator(String workerIP, int workerPort,
                                                        String coordinatorIP, int coordinatorPort) throws IOException {
        System.out.println("[Worker] Connecting to coordinator at " + coordinatorIP + ":" + coordinatorPort + "...");

        Socket coordinatorSocket = new Socket(coordinatorIP, coordinatorPort);
        PrintWriter out = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        DataInputStream dis = new DataInputStream(coordinatorSocket.getInputStream());

        // Sende dem Koordiantor eigene IP und port
        out.println("REGISTRATION:" + workerIP + ":" + workerPort);

        // Receive assigned worker ID
        int workerId = dis.readInt();

        // Receive right neighbor info
        boolean hasRightNeighbor = dis.readBoolean();
        String rightNeighborIP = null;
        int rightNeighborPort = -1;

        if (hasRightNeighbor) {
            rightNeighborIP = dis.readUTF();
            rightNeighborPort = dis.readInt();
        }

        // Erst dann wenn Verbindung steht
        System.out.println("[Worker] Successfully connected with ID: " + workerId);
        if (hasRightNeighbor) {
            System.out.println("[Worker] Right neighbor at: " + rightNeighborIP + ":" + rightNeighborPort);
        } else {
            System.out.println("[Worker] No right neighbor (last worker)");
        }

        return new WorkerConnection(coordinatorSocket, workerId, workerIP, workerPort,
                rightNeighborIP, rightNeighborPort);
    }

    /**
     * Result of coordinator setup
     */
    public static class CoordinatorResult {
        public final List<Socket> workerSockets;
        public final List<WorkerInfo> workerInfos;
        public final String command;

        public CoordinatorResult(List<Socket> workerSockets, List<WorkerInfo> workerInfos, String command) {
            this.workerSockets = workerSockets;
            this.workerInfos = workerInfos;
            this.command = command;
        }
    }

    /**
     * Worker connection information
     */
    public static class WorkerConnection {
        public final Socket socket;
        public final int id;
        public final String ownIP;
        public final int ownPort;
        public final String rightNeighborIP;
        public final int rightNeighborPort;

        public WorkerConnection(Socket socket, int id, String ownIP, int ownPort,
                                String rightNeighborIP, int rightNeighborPort) {
            this.socket = socket;
            this.id = id;
            this.ownIP = ownIP;
            this.ownPort = ownPort;
            this.rightNeighborIP = rightNeighborIP;
            this.rightNeighborPort = rightNeighborPort;
        }

        public boolean hasRightNeighbor() {
            return rightNeighborIP != null && rightNeighborPort > 0;
        }
    }
}