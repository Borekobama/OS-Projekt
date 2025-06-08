// ==================== Communicator.java ====================
import java.io.*;
import java.net.*;
import java.util.*;
import java.util.function.BinaryOperator;

/**
 * Handles all communication with hybrid topology:
 * - Star topology for control (broadcast, barrier, gather)
 * - Ring topology for neighbor communication
 */
public class Communicator {
    private final int rank;
    private final int size;
    private final boolean isRoot;

    // Star topology connections (for control)
    private final List<Socket> connections = new ArrayList<>();
    private final List<DataInputStream> inputStreams = new ArrayList<>();
    private final List<DataOutputStream> outputStreams = new ArrayList<>();

    // Ring topology connections (for neighbor communication)
    private Socket leftNeighborSocket;
    private Socket rightNeighborSocket;
    private DataInputStream leftNeighborIn;
    private DataOutputStream leftNeighborOut;
    private DataInputStream rightNeighborIn;
    private DataOutputStream rightNeighborOut;

    /**
     * Constructor for the Coordinator process
     */
    public Communicator(int rank, List<Socket> workerSockets, ConnectionSetup.WorkerInfo firstWorker) throws IOException {
        assert rank == 0;
        this.rank = 0;
        this.isRoot = true;
        this.size = workerSockets.size() + 1;

        // Setup star topology connections
        for (Socket socket : workerSockets) {
            connections.add(socket);
            inputStreams.add(new DataInputStream(new BufferedInputStream(socket.getInputStream())));
            outputStreams.add(new DataOutputStream(new BufferedOutputStream(socket.getOutputStream())));
        }

        // Setup ring topology - coordinator only has right neighbor (first worker)
        if (firstWorker != null) {
            rightNeighborSocket = new Socket(firstWorker.ip, firstWorker.port);
            rightNeighborOut = new DataOutputStream(new BufferedOutputStream(rightNeighborSocket.getOutputStream()));
            rightNeighborIn = new DataInputStream(new BufferedInputStream(rightNeighborSocket.getInputStream()));
            System.out.println("[Coordinator] Connected to right neighbor (Worker 1)");
        }
    }

    /**
     * Constructor for a Worker process
     */
    public Communicator(int rank, Socket coordinatorSocket, String ownIP, int ownPort,
                        String rightNeighborIP, int rightNeighborPort) throws IOException {
        this.rank = rank;
        this.size = -1; // Worker doesn't know total size
        this.isRoot = false;

        // Setup star topology connection to coordinator
        connections.add(coordinatorSocket);
        inputStreams.add(new DataInputStream(new BufferedInputStream(coordinatorSocket.getInputStream())));
        outputStreams.add(new DataOutputStream(new BufferedOutputStream(coordinatorSocket.getOutputStream())));

        // Setup ring topology connections
        // First, accept connection from left neighbor
        if (rank > 1) {
            ServerSocket serverSocket = new ServerSocket(ownPort);
            System.out.println("[Worker " + rank + "] Waiting for left neighbor connection...");
            leftNeighborSocket = serverSocket.accept();
            leftNeighborIn = new DataInputStream(new BufferedInputStream(leftNeighborSocket.getInputStream()));
            leftNeighborOut = new DataOutputStream(new BufferedOutputStream(leftNeighborSocket.getOutputStream()));
            serverSocket.close();
            System.out.println("[Worker " + rank + "] Left neighbor connected");
        } else {
            // Worker 1 accepts connection from coordinator
            ServerSocket serverSocket = new ServerSocket(ownPort);
            System.out.println("[Worker 1] Waiting for coordinator connection...");
            leftNeighborSocket = serverSocket.accept();
            leftNeighborIn = new DataInputStream(new BufferedInputStream(leftNeighborSocket.getInputStream()));
            leftNeighborOut = new DataOutputStream(new BufferedOutputStream(leftNeighborSocket.getOutputStream()));
            serverSocket.close();
            System.out.println("[Worker 1] Coordinator connected as left neighbor");
        }

        // Then connect to right neighbor (if exists)
        if (rightNeighborIP != null && rightNeighborPort > 0) {
            rightNeighborSocket = new Socket(rightNeighborIP, rightNeighborPort);
            rightNeighborOut = new DataOutputStream(new BufferedOutputStream(rightNeighborSocket.getOutputStream()));
            rightNeighborIn = new DataInputStream(new BufferedInputStream(rightNeighborSocket.getInputStream()));
            System.out.println("[Worker " + rank + "] Connected to right neighbor");
        }
    }

    // Basic getters
    public int getRank() { return rank; }
    public int getSize() { return size; }
    public boolean isRoot() { return isRoot; }
    public boolean hasLeftNeighbor() { return leftNeighborSocket != null; }
    public boolean hasRightNeighbor() { return rightNeighborSocket != null; }

    // ========== NEIGHBOR COMMUNICATION (Ring Topology) ==========
    // Trennung von Ring und Star Logik

    public void sendToLeftNeighbor(int value) throws IOException {
        if (leftNeighborOut == null) throw new IOException("No left neighbor connection");
        leftNeighborOut.writeInt(value);
        leftNeighborOut.flush();
    }

    public void sendToRightNeighbor(int value) throws IOException {
        if (rightNeighborOut == null) throw new IOException("No right neighbor connection");
        rightNeighborOut.writeInt(value);
        rightNeighborOut.flush();
    }

    public int receiveFromLeftNeighbor() throws IOException {
        if (leftNeighborIn == null) throw new IOException("No left neighbor connection");
        return leftNeighborIn.readInt();
    }

    public int receiveFromRightNeighbor() throws IOException {
        if (rightNeighborIn == null) throw new IOException("No right neighbor connection");
        return rightNeighborIn.readInt();
    }

    // ========== STAR TOPOLOGY COMMUNICATION ==========

    public void send(int[] data, int dest) throws IOException {
        DataOutputStream dos = getOutputStream(dest);
        dos.writeInt(data.length);
        for (int value : data) {
            dos.writeInt(value);
        }
        dos.flush();
    }

    public void send(int value, int dest) throws IOException {
        send(new int[]{value}, dest);
    }

    /**
     * Generic receive single value operation
     * @param <T> the type to receive
     * @param source the source process rank
     * @param serializer handles type conversion
     * @return the received value
     */
    public <T> T receive(int source, Serializers.Serializer<T> serializer) throws IOException {
        return serializer.receive(getInputStream(source));
    }

// Spezifische Methoden für primitive Arrays (effizienter als Generics)

    /**
     * Receive integer array
     */
    public int[] receiveIntArray(int source) throws IOException {
        DataInputStream dis = getInputStream(source);
        int count = dis.readInt();

        int[] buffer = new int[count];
        for (int i = 0; i < count; i++) {
            buffer[i] = dis.readInt();
        }
        return buffer;
    }

// Convenience-Methoden für einzelne Werte

    /**
     * Receive single integer
     */
    public int receiveInt(int source) throws IOException {
        return receive(source, Serializers.INTEGER);
    }

    /**
     * Receive scatter data (specific to int arrays)
     */
    public int[] receiveScatterData() throws IOException {
        if (isRoot) {
            throw new IllegalStateException("Root doesn't receive scatter data");
        }
        return receiveIntArray(0);
    }


    /**
     * Generic reduce operation
     * @param <T> the type to reduce
     * @param value the local value
     * @param op the binary operator
     * @param serializer handles type conversion
     */
    public <T> T reduce(T value, BinaryOperator<T> op, Serializers.Serializer<T> serializer) throws IOException {
        if (isRoot) {
            T result = value;
            for (int i = 1; i < size; i++) {
                T workerValue = serializer.receive(inputStreams.get(i - 1));
                result = op.apply(result, workerValue);
            }
            return result;
        } else {
            serializer.send(value, outputStreams.get(0));
            return value;
        }
    }

// Convenience-Methoden für häufige Typen

    /**
     * Reduce operation for integers
     */
    public int reduce(int value, BinaryOperator<Integer> op) throws IOException {
        return reduce(value, op, Serializers.INTEGER);
    }

    /**
     * Reduce operation for booleans using OR operation
     * (returns true if any process has true)
     */
    public boolean reduce(boolean value) throws IOException {
        return reduce(value, (a, b) -> a || b, Serializers.BOOLEAN);
    }

    public int[][] gather(int[] data) throws IOException {
        if (isRoot) {
            int[][] allData = new int[size][];
            allData[0] = data;

            for (int i = 1; i < size; i++) {
                allData[i] = receiveIntArray(i);
            }
            return allData;
        } else {
            send(data, 0);
            return null;
        }
    }

    public int[] scatter(int[] data, int[] chunkSizes) throws IOException {
        if (isRoot) {
            int index = chunkSizes[0];
            for (int i = 1; i < size; i++) {
                int[] chunk = Arrays.copyOfRange(data, index, index + chunkSizes[i]);
                send(chunk, i);
                index += chunkSizes[i];
            }
            return Arrays.copyOfRange(data, 0, chunkSizes[0]);
        } else {
            throw new IllegalStateException("Workers should use receiveScatterData() instead");
        }
    }

    public void broadcast(String message) throws IOException {
        if (!isRoot) {
            throw new IllegalStateException("Only root can broadcast");
        }

        byte[] messageBytes = message.getBytes("UTF-8");
        for (DataOutputStream dos : outputStreams) {
            dos.writeInt(messageBytes.length);
            dos.write(messageBytes);
            dos.flush();
        }
    }

    public String receiveBroadcast() throws IOException {
        if (isRoot) {
            throw new IllegalStateException("Root doesn't receive broadcasts");
        }

        DataInputStream dis = inputStreams.get(0);
        int length = dis.readInt();

        byte[] messageBytes = new byte[length];
        dis.readFully(messageBytes);

        return new String(messageBytes, "UTF-8");
    }

    public void barrier() throws IOException {
        if (isRoot) {
            for (int i = 1; i < size; i++) {
                receiveInt(i);
            }
            for (int i = 1; i < size; i++) {
                send(1, i);
            }
        } else {
            send(1, 0);
            receiveInt(0);
        }
    }

    private DataOutputStream getOutputStream(int dest) {
        if (isRoot && dest > 0 && dest <= connections.size()) {
            return outputStreams.get(dest - 1);
        } else if (!isRoot && dest == 0) {
            return outputStreams.get(0);
        }
        throw new IllegalArgumentException("Invalid destination: " + dest);
    }

    private DataInputStream getInputStream(int source) {
        if (isRoot && source > 0 && source <= connections.size()) {
            return inputStreams.get(source - 1);
        } else if (!isRoot && source == 0) {
            return inputStreams.get(0);
        }
        throw new IllegalArgumentException("Invalid source: " + source);
    }

    public void close() throws IOException {
        // Close star topology connections
        for (DataOutputStream dos : outputStreams) {
            dos.flush();
        }
        for (Socket socket : connections) {
            socket.close();
        }

        // Close ring topology connections
        if (leftNeighborSocket != null) leftNeighborSocket.close();
        if (rightNeighborSocket != null) rightNeighborSocket.close();
    }
}