// ==================== MPICommunicator.java ====================
import java.io.*;
import java.net.*;
import java.util.*;

/**
 * Encapsulates MPI-like communication between processes.
 * Corresponds to the MPI_Comm concept.
 */
public class MPICommunicator {
    // MPI constants
    public static final int ANY_TAG = -1;
    public static final int TAG_DATA = 0;
    public static final int TAG_REDUCE = 1;

    private final int rank;           // Process ID (0 = Coordinator)
    private final int size;           // Number of processes
    private final boolean isRoot;     // true if this is the Coordinator

    // Network connections
    private final List<Socket> connections = new ArrayList<>();
    private final List<DataInputStream> inputStreams = new ArrayList<>();
    private final List<DataOutputStream> outputStreams = new ArrayList<>();
    private final List<BufferedReader> readers = new ArrayList<>();
    private final List<PrintWriter> writers = new ArrayList<>();

    /**
     * Constructor for the Coordinator process.
     *
     * @param rank            Process rank (must be 0 for the coordinator)
     * @param workerSockets   List of sockets connected to worker processes
     * @throws IOException if an I/O error occurs
     */
    public MPICommunicator(int rank, List<Socket> workerSockets) throws IOException {
        assert rank == 0;
        this.rank = 0;
        this.isRoot = true;
        this.size = workerSockets.size() + 1;

        // Setup connections to all workers
        for (Socket socket : workerSockets) {
            connections.add(socket);
            inputStreams.add(new DataInputStream(socket.getInputStream()));
            outputStreams.add(new DataOutputStream(socket.getOutputStream()));
            readers.add(new BufferedReader(new InputStreamReader(socket.getInputStream())));
            writers.add(new PrintWriter(socket.getOutputStream(), true));
        }
    }

    /**
     * Constructor for a Worker process.
     * TODO: Change coordinator socket to List<Socket> for future scalability
     *
     * @param rank               Process rank
     * @param coordinatorSocket  Socket connected to the coordinator
     * @throws IOException if an I/O error occurs
     */
    public MPICommunicator(int rank, Socket coordinatorSocket) throws IOException {
        this.rank = rank;
        this.size = -1; // Worker does not know total number of processes
        this.isRoot = false;

        connections.add(coordinatorSocket);
        inputStreams.add(new DataInputStream(coordinatorSocket.getInputStream()));
        outputStreams.add(new DataOutputStream(coordinatorSocket.getOutputStream()));
        readers.add(new BufferedReader(new InputStreamReader(coordinatorSocket.getInputStream())));
        writers.add(new PrintWriter(coordinatorSocket.getOutputStream(), true));
    }

    /**
     * Returns the rank (process ID) of this process.
     *
     * @return rank of this process
     */
    public int getRank() { return rank; }

    /**
     * Returns the total number of processes.
     *
     * @return number of processes
     */
    public int getSize() { return size; }

    /**
     * Returns true if this process is the root (coordinator).
     *
     * @return true if root, false otherwise
     */
    public boolean isRoot() { return isRoot; }

    /**
     * MPI_Send - Blocking send operation.
     *
     * @param buffer   Data buffer to send
     * @param count    Number of elements to send
     * @param datatype Type of data
     * @param dest     Destination process rank
     * @param tag      Message tag
     * @throws IOException if an I/O error occurs
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
     *
     * @param buffer   Buffer to store received data
     * @param maxCount Maximum number of elements the buffer can hold
     * @param datatype Expected datatype of incoming data
     * @param source   Source process rank
     * @param tag      Expected message tag (use ANY_TAG to accept any tag)
     * @return Actual number of elements received
     * @throws IOException if an I/O error occurs, or if buffer size is insufficient
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

    // Private helper methods for stream access would presumably follow here (not shown in the snippet).



    // Convenience-Methoden
    public void send(int[] data, int dest) throws IOException {
        send(data, data.length, MPIDatatype.INT, dest, TAG_DATA);
    }

    public void send(int value, int dest) throws IOException {
        send(new int[]{value}, 1, MPIDatatype.INT, dest, TAG_DATA);
    }

    public int[] receiveIntArray(int source) throws IOException {
        return receiveIntArray(source, ANY_TAG);
    }


    //Eventuell wegstreichen und mit generischem receive ersetzen
    public int[] receiveIntArray(int source, int tag) throws IOException {
        DataInputStream dis = getInputStream(source);
        int count = dis.readInt();
        int actualTag = dis.readInt();

        // Tag-Prüfung
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
     * MPI_Reduce - Sammelt Werte von allen Prozessen und reduziert sie
     */
    public int reduce(int value, ReduceOperation op) throws IOException {
        if (isRoot) {
            int result = value; // Startwert ist der eigene Wert

            // Sammle Werte von allen Workern
            for (int i = 1; i < size; i++) {
                int[] buffer = new int[1];
                receive(buffer, 1, MPIDatatype.INT, i, TAG_REDUCE);
                result = op.apply(result, buffer[0]);
            }
            return result;
        } else {
            // Worker sendet seinen Wert an root
            send(new int[]{value}, 1, MPIDatatype.INT, 0, TAG_REDUCE);
            return value; // Worker bekommt Ergebnis nicht zurück (wie in MPI)
        }
    }

    /**
     * MPI_Gather - Sammelt Arrays von allen Prozessen beim Rfoot
     */
    public int[][] gather(int[] data) throws IOException {
        if (isRoot) {
            int[][] allData = new int[size][];
            allData[0] = data; // Eigene Daten

            // Sammle Daten von allen Workern
            for (int i = 1; i < size; i++) {
                allData[i] = receiveIntArray(i);
            }
            return allData;
        } else {
            // Worker sendet seine Daten an root
            send(data, 0);
            return null; // Worker bekommt gesammelte Daten nicht
        }
    }

    /**
     * Scatter - Verteilt Array-Chunks an alle Prozesse
     */
    public int[] scatter(int[] data, int[] chunkSizes) throws IOException {
        if (isRoot) {
            // Verteile Chunks an alle Worker
            int index = chunkSizes[0]; // Überspringe eigenen Chunk
            for (int i = 1; i < size; i++) {
                int[] chunk = Arrays.copyOfRange(data, index, index + chunkSizes[i]);
                send(chunk, chunkSizes[i], MPIDatatype.INT, i, TAG_DATA);
                index += chunkSizes[i];
            }
            // Gib eigenen Chunk zurück
            return Arrays.copyOfRange(data, 0, chunkSizes[0]);
        } else {
            // Worker empfängt seinen Chunk
            return receiveIntArray(0, TAG_DATA);
        }
    }

    /**
     * MPI_Bcast - Broadcast von root zu allen anderen
     */
    public void broadcast(String message) throws IOException {
        if (!isRoot) {
            throw new IllegalStateException("Nur Root kann broadcast");
        }

        for (PrintWriter writer : writers) {
            writer.println(message);
        }
    }

    public String receiveBroadcast() throws IOException {
        if (isRoot) {
            throw new IllegalStateException("Root empfängt keine broadcasts");
        }
        return readers.get(0).readLine();
    }

    /**
     * MPI_Barrier - Synchronisationsbarriere aber nur für Coordinator.
     * Blockiert nicht für Worker
     */
    public void barrier() throws IOException {
        if (isRoot) {
            // Coordinator wartet auf alle Worker
            for (int i = 0; i < connections.size(); i++) {
                String response = readers.get(i).readLine();
                System.out.println("[Coordinator] Barrier response from rank " + (i + 1) + ": " + response);
            }
        } else {
            // Worker sendet Bereitschaftssignal
            writers.get(0).println("BARRIER_REACHED");
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

    private BufferedReader getReader(int source) {
        if (isRoot && source > 0 && source <= connections.size()) {
            return readers.get(source - 1);
        } else if (!isRoot && source == 0) {
            return readers.get(0);
        }
        throw new IllegalArgumentException("Invalid source: " + source);
    }

    public void close() throws IOException {
        for (Socket socket : connections) {
            socket.close();
        }
    }
}
