import java.io.*;
import java.net.*;
import java.util.*;

public class ParallelSorting {

    private static final int ARRAY_LENGTH = 85;

    private Socket coordinatorSocket;
    private final List<Socket> workerSockets = new ArrayList<>();
    private final List<Integer> workerPorts = new ArrayList<>();
    private final List<String> workerIPs = new ArrayList<>();

    private int id;
    private final boolean isCoordinator;

    private final String ownIP;
    private final int ownPort;

    private final String coordinatorIP;
    private final int coordinatorPort;
    private Container container;
    private int[] unsortedArray;

    private BufferedReader coordinatorIn;
    private PrintWriter coordinatorOut;

    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            System.out.println("Usage:\nCoordinator: c <ownIP> <ownPort>\nWorker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
            return;
        }

        boolean isCoordinator = args[0].equalsIgnoreCase("c");
        String ownIP = args[1];
        int ownPort = Integer.parseInt(args[2]);

        if (isCoordinator) {
            new ParallelSorting(ownIP, ownPort).start();
        } else {
            if (args.length < 5) {
                System.out.println("Usage for Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
                return;
            }
            String coordinatorIP = args[3];
            int coordinatorPort = Integer.parseInt(args[4]);
            new ParallelSorting(ownIP, ownPort, coordinatorIP, coordinatorPort).start();
        }
    }

    public ParallelSorting(String ownIP, int ownPort) {
        this.isCoordinator = true;
        this.ownIP = ownIP;
        this.ownPort = ownPort;
        this.coordinatorIP = ownIP;
        this.coordinatorPort = ownPort;
        this.id = 1;
    }

    public ParallelSorting(String ownIP, int ownPort, String coordinatorIP, int coordinatorPort) {
        this.isCoordinator = false;
        this.ownIP = ownIP;
        this.ownPort = ownPort;
        this.coordinatorIP = coordinatorIP;
        this.coordinatorPort = coordinatorPort;
    }

    public void start() throws Exception {
        if (isCoordinator) {
            runAsCoordinator();
        } else {
            runAsWorker();
        }
    }

    private void runAsCoordinator() throws Exception {
        registerWorkers();
        initializeWorkers();
        initializeCoordinatorContainer();

        createArray();
        distributeArray();
        sort();
        receiveSortedChunks();
    }

    private void registerWorkers() throws IOException {
        ServerSocket serverSocket = new ServerSocket(ownPort);
        System.out.println("[Coordinator] Waiting for workers... (press Enter to finish)");

        Thread stopper = new Thread(() -> {
            try {
                new BufferedReader(new InputStreamReader(System.in)).readLine();
                serverSocket.close();
            } catch (IOException ignored) {}
        });
        stopper.start();

        while (true) {
            try {
                Socket worker = serverSocket.accept();
                BufferedReader in = new BufferedReader(new InputStreamReader(worker.getInputStream()));
                String line = in.readLine();
                if (line != null && line.startsWith("REGISTRATION:")) {
                    String[] parts = line.substring("REGISTRATION:".length()).split(":");
                    String ip = parts[0];
                    int port = Integer.parseInt(parts[1]);
                    workerIPs.add(ip);
                    workerPorts.add(port);
                    workerSockets.add(worker);
                    System.out.println("[Coordinator] New worker registered at " + ip + ":" + port);
                }
            } catch (IOException e) {
                break; // Enter pressed
            }
        }
    }

    private void initializeWorkers() throws IOException {
        for (int i = 0; i < workerSockets.size(); i++) {
            int workerId = i + 2;
            String rightIP = (i < workerIPs.size() - 1) ? workerIPs.get(i + 1) : "null";
            int rightPort = (i < workerPorts.size() - 1) ? workerPorts.get(i + 1) : -1;

            PrintWriter out = new PrintWriter(workerSockets.get(i).getOutputStream(), true);
            out.println(workerId + ";" + rightIP + ";" + rightPort);
        }
    }

    private void initializeCoordinatorContainer() throws IOException {

        TCPConnection rightConn =
                new TCPConnection(new Socket(workerIPs.get(0),
                        workerPorts.get(0)));
        container = new Container(id, null, rightConn);
    }

    private void distributeArray() throws IOException {
        int numContainers = workerSockets.size() + 1;
        int chunkSize = ARRAY_LENGTH / numContainers;
        int remainder = ARRAY_LENGTH % numContainers;

        int index = 0;
        for (int i = 0; i < numContainers; i++) {
            int size = chunkSize + (remainder-- > 0 ? 1 : 0);
            int[] chunk = Arrays.copyOfRange(unsortedArray, index, index + size);
            index += size;

            if (i == 0) {
                container.setLocalArray(chunk);
                container.logArray();
            } else {
                DataOutputStream dos = new DataOutputStream(workerSockets.get(i - 1).getOutputStream());
                dos.writeInt(chunk.length);
                for (int value : chunk) {
                    dos.writeInt(value);
                }
                dos.flush();
            }
        }
    }

    private void sort() throws IOException {
        broadcastRound("PRESORT");
        container.preSort();
        waitForResponses();

        boolean hasSwapped = true;
        int roundCounter = 1;

        while (hasSwapped) {
            broadcastRound("ODD");
            boolean localOddSwap = container.exchangeIfActive("ODD");
            boolean remoteOddSwap = waitForResponses();
            boolean oddSwaps = localOddSwap || remoteOddSwap;

            broadcastRound("EVEN");
            boolean localEvenSwap = container.exchangeIfActive("EVEN");
            boolean remoteEvenSwap = waitForResponses();
            boolean evenSwaps = localEvenSwap || remoteEvenSwap;

            hasSwapped = oddSwaps || evenSwaps;
            roundCounter++;
            System.out.println("[Coordinator] Round " + roundCounter + " complete. Swaps occurred: " + hasSwapped);
        }

        broadcastDone();
    }

    private void broadcastRound(String round) throws IOException {
        System.out.println("[Coordinator] Broadcasting round: " + round);
        for (int i = 0; i < workerSockets.size(); i++) {
            PrintWriter out = new PrintWriter(workerSockets.get(i).getOutputStream(), true);
            out.println("ROUND:" + round);
        }
    }

    private void broadcastDone() throws IOException {
        for (Socket worker : workerSockets) {
            PrintWriter out = new PrintWriter(worker.getOutputStream(), true);
            out.println("DONE");
        }
    }

    private boolean waitForResponses() throws IOException {
        boolean anySwapped = false;
        for (int i = 0; i < workerSockets.size(); i++) {
            BufferedReader in = new BufferedReader(new InputStreamReader(workerSockets.get(i).getInputStream()));
            String msg = in.readLine();
            System.out.println("[Coordinator] Response from worker " + (i + 2) + ": " + msg);
            if ("SWAPPED:true".equals(msg)) {
                anySwapped = true;
            }
        }
        return anySwapped;
    }

    private void receiveSortedChunks() throws IOException {
        List<Integer> fullSortedList = new ArrayList<>();

        int[] ownChunk = container.getLocalArray();
        for (int val : ownChunk) fullSortedList.add(val);

        for (Socket workerSocket : workerSockets) {
            DataInputStream dis = new DataInputStream(workerSocket.getInputStream());
            int length = dis.readInt();
            for (int j = 0; j < length; j++) {
                fullSortedList.add(dis.readInt());
            }
        }

        System.out.println("[Coordinator] Final sorted array: " + fullSortedList);
        System.out.println("[Coordinator] Sorted correctly? " + isSorted(fullSortedList));
    }

    private boolean isSorted(List<Integer> list) {
        for (int i = 0; i < list.size() - 1; i++) {
            if (list.get(i) > list.get(i + 1)) return false;
        }
        return true;
    }

    // Kann Deadlock entstehen wenn zuerst mit accept gewartet wird?
    private void runAsWorker() throws Exception {
        connectToCoordinator();

        coordinatorIn = new BufferedReader(new InputStreamReader(coordinatorSocket.getInputStream()));
        coordinatorOut = new PrintWriter(coordinatorSocket.getOutputStream(), true);

        String[] parts = coordinatorIn.readLine().split(";");
        id = Integer.parseInt(parts[0]);
        String rightIP = parts[1];
        int rightPort = Integer.parseInt(parts[2]);

        ServerSocket serverSocket = new ServerSocket(ownPort, 50, InetAddress.getByName(ownIP));
        Socket leftSocket = serverSocket.accept();
        TCPConnection leftConn = new TCPConnection(leftSocket);

        TCPConnection rightConn = (!"null".equals(rightIP) && rightPort != -1)
                ? new TCPConnection(new Socket(rightIP, rightPort))
                : null;

        container = new Container(id, leftConn, rightConn);

        DataInputStream dis = new DataInputStream(coordinatorSocket.getInputStream());
        int[] chunk = receiveArray(dis);
        container.setLocalArray(chunk);
        container.logArray();

        while (true) {
            String round = receiveRoundType();
            if ("DONE".equals(round)) break;
            boolean swapped = container.exchangeIfActive(round);
            sendSwapStatus(swapped);
        }

        sendSortedChunk();
    }

    private void connectToCoordinator() throws IOException {
        coordinatorSocket = new Socket(coordinatorIP, coordinatorPort);
        coordinatorOut = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        coordinatorOut.println("REGISTRATION:" + ownIP + ":" + ownPort);
    }

    private String receiveRoundType() throws IOException {
        String line = coordinatorIn.readLine();
        if (line == null) throw new IOException("Connection lost");
        if (line.startsWith("ROUND:")) return line.substring(6);
        if ("DONE".equals(line)) return "DONE";
        throw new IOException("Unexpected message: " + line);
    }

    private void sendSwapStatus(boolean swapped) {
        coordinatorOut.println("SWAPPED:" + swapped);
    }

    private void sendSortedChunk() throws IOException {
        int[] sortedChunk = container.getLocalArray();
        DataOutputStream dos = new DataOutputStream(coordinatorSocket.getOutputStream());
        dos.writeInt(sortedChunk.length);
        for (int value : sortedChunk) {
            dos.writeInt(value);
        }
        dos.flush();
    }

    private int[] receiveArray(DataInputStream dis) throws IOException {
        int len = dis.readInt();
        int[] arr = new int[len];
        for (int i = 0; i < len; i++) arr[i] = dis.readInt();
        return arr;
    }

    private void createArray() {
        Random rnd = new Random();
        unsortedArray = rnd.ints(ARRAY_LENGTH, 0, 100).toArray();
    }
}
