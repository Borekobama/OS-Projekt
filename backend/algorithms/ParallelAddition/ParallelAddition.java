import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class ParallelAddition {

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
    private int[] initialArray;

    private int[] chunk;

    private int sum;

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

    // Konstruktor für Coordiantor
    public ParallelAddition(String ownIP, int ownPort) {
        this.isCoordinator = true;
        this.ownIP = ownIP;
        this.ownPort = ownPort;
        this.coordinatorIP = ownIP;
        this.coordinatorPort = ownPort;
        this.id = 0;
    }

    // Konstruktor für Worker
    public ParallelAddition(String ownIP, int ownPort, String coordinatorIP, int coordinatorPort) {
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
        // Baue das Netzwerk zu den Workern auf
        registerWorkers();

        // Erstelle zufälliges Array
        initialArray = Utils.createArray(ARRAY_LENGTH);

        // Sende Array an alle Worker. Soll mit mpi like send methode ersetzt
        // werden.
        sendToAll();

        // Führe eigentliche Addition durch
        broadcast("START");

        calculateSum();

        // Warte, dass alle Worker Bescheid gesagt haben dass sie fertig
        // sind
        barrier();

        broadcast("DONE");
        reduce();
    }


    private void runAsWorker() throws Exception {
        connectToCoordinator();

        // Soll mit receive mpi like methode ersetzt werden
        coordinatorIn = new BufferedReader(new InputStreamReader(coordinatorSocket.getInputStream()));
        coordinatorOut = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        DataInputStream dis = new DataInputStream(coordinatorSocket.getInputStream());
        id = dis.readInt();


        // Hole Teilarray ab das wir summieren sollen
        chunk = receive(dis);

        logArray();

        while (true) {
            String round = receiveBroadcast();
            if ("START".equals(round)) calculateSum();
            if ("DONE".equals(round)) break;
        }

        send();
    }

    // Führt die eigenliche Berechnung durch
    private void calculateSum() throws IOException {
        sum = 0;
        for (int i: chunk) {
            sum += i;
        }
        System.out.println("[Worker " + id + "]: Teilsumme ist " + sum);

        if(!isCoordinator){
            // Sage Koordinator Bescheid, dass Berechnung abgeschlossen ist,
            // soll mit mpi like methode send ersetzt werden
            PrintWriter out = new PrintWriter(coordinatorSocket.getOutputStream(), true);
            out.println("COMPLETED");
        }

    }


    // --------------------Netzwerkaufbau----------------------

    // Akzeptiere solange neue worker und fügt sie dem netzwerk hinzu bis dass
    // enter gedrückt wird
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

        int currentID = 0;
        while (true) {
            try {
                Socket worker = serverSocket.accept();
                currentID++;
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

                    //Sende Worker seine ID
                    DataOutputStream dos =
                            new DataOutputStream(worker.getOutputStream());
                    dos.writeInt(currentID);
                    dos.flush();
                }
            } catch (IOException e) {
                break; // Enter pressed
            }
        }
    }

    // der worker registriert sich beim coordinator
    private void connectToCoordinator() throws IOException {
        coordinatorSocket = new Socket(coordinatorIP, coordinatorPort);
        coordinatorOut = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        coordinatorOut.println("REGISTRATION:" + ownIP + ":" + ownPort);
    }


    // --------------Interprozesskommunikation--------------------------

    /*
  Implementierung in C: int MPI_Barrier( MPI_Comm comm )
  Blocks until all processes in the communicator have reached this routine.
   */
    private void barrier() throws IOException {
        for (int i = 0; i < workerSockets.size(); i++) {
            BufferedReader in = new BufferedReader(new InputStreamReader(workerSockets.get(i).getInputStream()));
            String msg = in.readLine();
            System.out.println("[Coordinator] Response from worker " + (i + 2) + ": " + msg);
        }
    }

    /*
    Implementierung in C: int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
    Performs a blocking send.
     */
    private void send() throws IOException {

        DataOutputStream dos = new DataOutputStream(coordinatorSocket.getOutputStream());
        dos.writeInt(sum);
        dos.flush();
    }

    /*
    Implementierung in C: int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status)
             Blocking receive for a message
     */
    private int[] receive(DataInputStream dis) throws IOException {
        int len = dis.readInt();
        int[] arr = new int[len];
        for (int i = 0; i < len; i++) arr[i] = dis.readInt();
        return arr;
    }


    private int receive(Socket source) throws IOException {
        DataInputStream dis = new DataInputStream(source.getInputStream());
        return dis.readInt();
    }


    private String receiveBroadcast() throws IOException {
        String line = coordinatorIn.readLine();
        if (line == null) throw new IOException("Connection lost");
        if ("DONE".equals(line)) return "DONE";
        if ("START".equals(line)) return "START";
        throw new IOException("Unexpected message: " + line);
    }

    /*
    Teilt das Array auf und sendet es an alle registrierten Worker. Speichert
     das Teilarray des coordiantors ab.
     */
    private void sendToAll() throws IOException {

        //Teile das Array auf
        int numWorkers = workerSockets.size() + 1;
        int chunkSize = ARRAY_LENGTH / numWorkers;
        int remainder = ARRAY_LENGTH % numWorkers;

        int index = 0;
        for (int i = 0; i < numWorkers; i++) {
            int size = chunkSize + (remainder-- > 0 ? 1 : 0);
            int[] chunk = Arrays.copyOfRange(initialArray, index, index + size);
            index += size;

            if (i == 0) {
                //Speichere eigenes Array
                this.chunk = chunk;
                logArray();
            } else {

                //Sende Teilarray zu den Workern
                DataOutputStream dos = new DataOutputStream(workerSockets.get(i - 1).getOutputStream());
                dos.writeInt(chunk.length);
                for (int value : chunk) {
                    dos.writeInt(value);
                }
                dos.flush();
            }
        }
    }


  /*
  C Implementierung: int MPI_Bcast( void *buffer, int count, MPI_Datatype
  datatype, int root, MPI_Comm comm )
  Broadcasts a message from the process with rank "root" to all other processes of the communicator
   */
    private void broadcast(String round) throws IOException {
        System.out.println("[Coordinator] Broadcasting round: " + round);
        for (Socket workerSocket : workerSockets) {
            PrintWriter out =
                    new PrintWriter(workerSocket.getOutputStream(), true);
            out.println(round);
        }
    }

    /*
    C Implementierung: int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, int root, MPI_Comm comm)
               Reduces values on all processes to a single value
     */
    private void reduce() throws IOException {

        // Eigene Summe hinzufügen
        int finalSum = sum;
        int workerSum = 0;

        for (Socket workerSocket : workerSockets) {
            workerSum = receive(workerSocket);
            finalSum += workerSum;
        }

        System.out.println("[Coordinator] Final Sum: " + finalSum);
        System.out.println("[Coordinator] Added correctly? " + Utils.isSumCorrect(finalSum,
                initialArray));
    }


    // ----------------Logs----------------------

    // Loggt das übergeben Teilarray
    private void logArray(){
        System.out.println("[Worker " + id + "]: Teilarray " + Arrays.toString(chunk) + " erhalten.");
    }

}
