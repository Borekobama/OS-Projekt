// ==================== MPICommunicator.java ====================
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * Encapsulates MPI-like communication between processes.
 * Uses only DataInputStream/DataOutputStream for all communication.
 */
public class MPICommunicator {
    // MPI constants
    public static final int ANY_TAG = -1;
    public static final int TAG_DATA = 0;
    public static final int TAG_REDUCE = 1;
    public static final int TAG_BROADCAST = 2;

    private final int rank;           // Process ID (0 = Coordinator)
    private final int size;           // Number of processes
    private final boolean isRoot;     // true if this is the Coordinator

    // Network connections - ONLY using DataStreams
    private final List<Socket> connections = new ArrayList<>();
    private final List<DataInputStream> inputStreams = new ArrayList<>();
    private final List<DataOutputStream> outputStreams = new ArrayList<>();

    /**
     * Constructor for the Coordinator process.
     */
    public MPICommunicator(int rank, List<Socket> workerSockets) throws IOException {
        assert rank == 0;
        this.rank = 0;
        this.isRoot = true;
        this.size = workerSockets.size() + 1;

        // Setup connections to all workers - ONLY DataStreams
        for (Socket socket : workerSockets) {
            connections.add(socket);
            inputStreams.add(new DataInputStream(new BufferedInputStream(socket.getInputStream())));
            outputStreams.add(new DataOutputStream(new BufferedOutputStream(socket.getOutputStream())));
        }
    }

    /**
     * Constructor for a Worker process.
     */
    public MPICommunicator(int rank, Socket coordinatorSocket) throws IOException {
        this.rank = rank;
        this.size = -1; // Worker does not know total number of processes
        this.isRoot = false;

        connections.add(coordinatorSocket);
        inputStreams.add(new DataInputStream(new BufferedInputStream(coordinatorSocket.getInputStream())));
        outputStreams.add(new DataOutputStream(new BufferedOutputStream(coordinatorSocket.getOutputStream())));
    }

    /**
     * Returns the rank (process ID) of this process.
     */
    public int getRank() { return rank; }

    /**
     * Returns the total number of processes.
     */
    public int getSize() { return size; }

    /**
     * Returns true if this process is the root (coordinator).
     */
    public boolean isRoot() { return isRoot; }

    /**
     * MPI_Send - Blocking send operation.
     */
    public void send(Object buffer, int count, MPIDatatype datatype, int dest, int tag) throws IOException {
        DataOutputStream dos = getOutputStream(dest);

        // Send header: count, tag
        dos.writeInt(count);
        dos.writeInt(tag);

        // Send data based on datatype
        switch (datatype) {
            case INT:
                int[] intBuffer = (int[]) buffer;
                for (int i = 0; i < count; i++) {
                    dos.writeInt(intBuffer[i]);
                }
                break;
            case DOUBLE:
                double[] doubleBuffer = (double[]) buffer;
                for (int i = 0; i < count; i++) {
                    dos.writeDouble(doubleBuffer[i]);
                }
                break;
            case CHAR:
                byte[] byteBuffer = (byte[]) buffer;
                dos.write(byteBuffer, 0, count);
                break;
            default:
                throw new UnsupportedOperationException("Datatype " + datatype + " not supported");
        }
        dos.flush();
    }

    /**
     * MPI_Recv - Blocking receive operation.
     */
    public int receive(Object buffer, int maxCount, MPIDatatype datatype, int source, int tag) throws IOException {
        DataInputStream dis = getInputStream(source);

        // Receive header
        int actualCount = dis.readInt();
        int actualTag = dis.readInt();

        // Check tag (ANY_TAG accepts all)
        if (tag != ANY_TAG && tag != actualTag) {
            throw new IOException("Tag mismatch: expected " + tag + ", got " + actualTag);
        }

        // Check buffer size
        if (actualCount > maxCount) {
            throw new IOException("Buffer too small: received " + actualCount + " elements, buffer size " + maxCount);
        }

        // Receive data
        switch (datatype) {
            case INT:
                int[] intBuffer = (int[]) buffer;
                for (int i = 0; i < actualCount; i++) {
                    intBuffer[i] = dis.readInt();
                }
                break;
            case DOUBLE:
                double[] doubleBuffer = (double[]) buffer;
                for (int i = 0; i < actualCount; i++) {
                    doubleBuffer[i] = dis.readDouble();
                }
                break;
            case CHAR:
                byte[] byteBuffer = (byte[]) buffer;
                dis.readFully(byteBuffer, 0, actualCount);
                break;
            default:
                throw new UnsupportedOperationException("Datatype " + datatype + " not supported");
        }

        return actualCount;
    }

    // Convenience methods
    public void send(int[] data, int dest) throws IOException {
        send(data, data.length, MPIDatatype.INT, dest, TAG_DATA);
    }

    public void send(int value, int dest) throws IOException {
        send(new int[]{value}, 1, MPIDatatype.INT, dest, TAG_DATA);
    }

    public int[] receiveIntArray(int source) throws IOException {
        return receiveIntArray(source, ANY_TAG);
    }

    public int[] receiveIntArray(int source, int tag) throws IOException {
        DataInputStream dis = getInputStream(source);
        int count = dis.readInt();
        int actualTag = dis.readInt();

        // Tag check
        if (tag != ANY_TAG && tag != actualTag) {
            throw new IOException("Tag mismatch: expected " + tag + ", got " + actualTag);
        }

        int[] buffer = new int[count];
        for (int i = 0; i < count; i++) {
            buffer[i] = dis.readInt();
        }
        return buffer;
    }

    public int receiveInt(int source) throws IOException {
        int[] buffer = new int[1];
        receive(buffer, 1, MPIDatatype.INT, source, ANY_TAG);
        return buffer[0];
    }

    /**
     * Worker method to receive scatter data from coordinator
     */
    public int[] receiveScatterData() throws IOException {
        if (isRoot) {
            throw new IllegalStateException("Root doesn't receive scatter data");
        }
        return receiveIntArray(0, TAG_DATA);
    }

    /**
     * MPI_Reduce - Collects values from all processes and reduces them
     */
    public int reduce(int value, ReduceOperation op) throws IOException {
        if (isRoot) {
            int result = value; // Start with own value

            // Collect values from all workers
            for (int i = 1; i < size; i++) {
                int[] buffer = new int[1];
                receive(buffer, 1, MPIDatatype.INT, i, TAG_REDUCE);
                result = op.apply(result, buffer[0]);
            }
            return result;
        } else {
            // Worker sends its value to root
            send(new int[]{value}, 1, MPIDatatype.INT, 0, TAG_REDUCE);
            return value; // Worker doesn't get result back (like in MPI)
        }
    }

    /**
     * MPI_Gather - Collects arrays from all processes at root
     */
    public int[][] gather(int[] data) throws IOException {
        if (isRoot) {
            int[][] allData = new int[size][];
            allData[0] = data; // Own data

            // Collect data from all workers
            for (int i = 1; i < size; i++) {
                allData[i] = receiveIntArray(i);
            }
            return allData;
        } else {
            // Worker sends its data to root
            send(data, 0);
            return null; // Worker doesn't get collected data
        }
    }

    /**
     * Scatter - Distributes array chunks to all processes
     */
    public int[] scatter(int[] data, int[] chunkSizes) throws IOException {
        if (isRoot) {
            // Distribute chunks to all workers
            int index = chunkSizes[0]; // Skip own chunk
            for (int i = 1; i < size; i++) {
                int[] chunk = Arrays.copyOfRange(data, index, index + chunkSizes[i]);
                send(chunk, chunkSizes[i], MPIDatatype.INT, i, TAG_DATA);
                index += chunkSizes[i];
            }
            // Return own chunk
            return Arrays.copyOfRange(data, 0, chunkSizes[0]);
        } else {
            // This should not be called by workers - they use receiveScatterData() instead
            throw new IllegalStateException("Workers should use receiveScatterData() instead");
        }
    }

    /**
     * MPI_Bcast - Broadcast from root to all others using DataStreams
     */
    public void broadcast(String message) throws IOException {
        if (!isRoot) {
            throw new IllegalStateException("Only root can broadcast");
        }

        // Convert string to bytes and broadcast
        byte[] messageBytes = message.getBytes("UTF-8");

        for (DataOutputStream dos : outputStreams) {
            dos.writeInt(messageBytes.length); // Send length first
            dos.writeInt(TAG_BROADCAST);       // Send tag
            dos.write(messageBytes);           // Send message bytes
            dos.flush();
        }
    }

    /**
     * Receive broadcast message using DataStreams
     */
    public String receiveBroadcast() throws IOException {
        if (isRoot) {
            throw new IllegalStateException("Root doesn't receive broadcasts");
        }

        DataInputStream dis = inputStreams.get(0);
        int length = dis.readInt();
        int tag = dis.readInt();

        if (tag != TAG_BROADCAST) {
            throw new IOException("Expected broadcast tag, got: " + tag);
        }

        byte[] messageBytes = new byte[length];
        dis.readFully(messageBytes);

        return new String(messageBytes, "UTF-8");
    }

    /**
     * MPI_Barrier - Synchronization barrier
     */
    public void barrier() throws IOException {
        if (isRoot) {
            // Coordinator waits for all workers
            for (int i = 1; i < size; i++) {
                int[] buffer = new int[1];
                receive(buffer, 1, MPIDatatype.INT, i, ANY_TAG);
                System.out.println("[Coordinator] Barrier response from rank " + i);
            }
        } else {
            // Worker sends ready signal
            send(new int[]{1}, 1, MPIDatatype.INT, 0, TAG_DATA);
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
        // Flush all output streams before closing
        for (DataOutputStream dos : outputStreams) {
            dos.flush();
        }

        for (Socket socket : connections) {
            socket.close();
        }
    }
}