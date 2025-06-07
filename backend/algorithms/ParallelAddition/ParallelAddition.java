/**
 * ParallelAddition führt die verteilte Summenberechnung eines Arrays durch.
 * Es gibt einen Koordinator, der das Array generiert und auf die Worker verteilt,
 * sowie mehrere Worker, die jeweils einen Teil des Arrays summieren.
 * <p>
 * Unbenutzte Variablen (workerIPs, workerPorts) bleiben erhalten für zukünftige Erweiterungen.
 * Dieser Code ist ausführlich dokumentiert für eine einfache Wartung und Erweiterung.
 */

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.*;

public class ParallelAddition {
    /** Länge des zu summierenden Arrays */
    private static final int ARRAY_LENGTH = 85;

    // --- Gemeinsame Felder für Koordinator und Worker ---
    private final String ownIP;
    private final int ownPort;
    private final String coordinatorIP;
    private final int coordinatorPort;
    private final boolean isCoordinator;
    private int id;

    // Sockets und Kommunikationsstreams
    private Socket coordinatorSocket;
    private BufferedReader coordIn;
    private PrintWriter coordOut;

    // --- Nur im Koordinator aktiv ---
    private final List<Socket> workerSockets = new ArrayList<>();
    private final List<String> workerIPs = new ArrayList<>();   // Reserviert für zukünftige Erweiterung
    private final List<Integer> workerPorts = new ArrayList<>(); // Reserviert für zukünftige Erweiterung
    private int[] initialArray;

    // --- Gemeinsame Felder: Daten für Berechnung ---
    private int[] chunk;
    private int sum;

    /**
     * Einstiegspunkt der Anwendung.
     * @param args Parameter: für Koordinator (c ownIP ownPort),
     *             für Worker (w ownIP ownPort coordIP coordPort)
     */
    public static void main(String[] args) throws Exception {
        if (args.length < 3) {
            System.out.println("Usage:\nCoordinator: c <ownIP> <ownPort>\n"
                    + "Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
            return;
        }
        boolean isCoord = args[0].equalsIgnoreCase("c");
        String ownIP = args[1];
        int ownPort = Integer.parseInt(args[2]);

        if (isCoord) {
            new ParallelAddition(ownIP, ownPort).start();
        } else {
            if (args.length < 5) {
                System.out.println("Usage for Worker: w <ownIP> <ownPort> <coordinatorIP> <coordinatorPort>");
                return;
            }
            String coordIP = args[3];
            int coordPort = Integer.parseInt(args[4]);
            new ParallelAddition(ownIP, ownPort, coordIP, coordPort).start();
        }
    }

    /** Konstruktor für Koordinator */
    public ParallelAddition(String ownIP, int ownPort) {
        this.ownIP = ownIP;
        this.ownPort = ownPort;
        this.coordinatorIP = ownIP;
        this.coordinatorPort = ownPort;
        this.isCoordinator = true;
        this.id = 0;
    }

    /** Konstruktor für Worker */
    public ParallelAddition(String ownIP, int ownPort, String coordIP, int coordPort) {
        this.ownIP = ownIP;
        this.ownPort = ownPort;
        this.coordinatorIP = coordIP;
        this.coordinatorPort = coordPort;
        this.isCoordinator = false;
    }

    /** Startet den Prozess je nach Rolle */
    public void start() throws Exception {
        if (isCoordinator) runAsCoordinator();
        else runAsWorker();
    }

    // ----------------------------------------------------------------------------------------------------------------
    //  Koordinator-Methoden
    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Ablauf im Koordinator: Worker registrieren, Array erstellen, Aufgaben verteilen,
     * Summation koordinieren und Reduction durchführen.
     */
    private void runAsCoordinator() throws IOException {
        registerWorkers();                     // Wartet auf Worker
        initialArray = Utils.createArray(ARRAY_LENGTH);  // Erzeuge zufälliges Array
        sendChunksToAll();                     // Teile und sende
        coordinateSummation();                 // Koordiniere START/DONE Runden und eigene Summe
        performReduction();                    // Sammle Ergebnisse und gib Endergebnis aus
    }

    /** Wartet auf Worker-Registrierungen bis Enter gedrückt wird */
    private void registerWorkers() throws IOException {
        ServerSocket serverSocket = new ServerSocket(ownPort);
        System.out.println("[Coordinator] Waiting for workers... (press Enter to finish)");

        // Thread zum Stoppen per Enter
        new Thread(() -> {
            try { new BufferedReader(new InputStreamReader(System.in)).readLine();
                serverSocket.close();
            } catch (IOException ignored) {}
        }).start();

        int nextId = 1;
        while (true) {
            try {
                Socket worker = serverSocket.accept();
                handleRegistration(worker, nextId++);
            } catch (IOException e) {
                break; // ServerSocket geschlossen
            }
        }
    }

    /** Liest REGISTRATION-Meldung, speichert Worker-Info und sendet Worker-ID */
    private void handleRegistration(Socket worker, int assignedId) throws IOException {
        BufferedReader in = new BufferedReader(new InputStreamReader(worker.getInputStream()));
        String line = in.readLine();
        if (line != null && line.startsWith("REGISTRATION:")) {
            String[] parts = line.split(":" );
            workerIPs.add(parts[1]);       // Noch unbenutzt, für Erweiterung
            workerPorts.add(Integer.parseInt(parts[2]));
            workerSockets.add(worker);
            System.out.println("[Coordinator] Registered Worker " + assignedId
                    + " at " + parts[1] + ":" + parts[2]);

            DataOutputStream dos = new DataOutputStream(worker.getOutputStream());
            dos.writeInt(assignedId);
            dos.flush();
        }
    }

    /** Zerlegt initialArray in Chunks und verteilt sie an die Worker */
    private void sendChunksToAll() throws IOException {
        int totalNodes = workerSockets.size() + 1;  // inkl. Koordinator
        int baseSize = ARRAY_LENGTH / totalNodes;
        int extra = ARRAY_LENGTH % totalNodes;
        int idx = 0;

        for (int node = 0; node < totalNodes; node++) {
            int size = baseSize + (extra-- > 0 ? 1 : 0);
            int[] piece = Arrays.copyOfRange(initialArray, idx, idx + size);
            idx += size;

            if (node == 0) {
                this.chunk = piece;                // eigener Chunk
                logOwnChunk();
            } else {
                sendChunkToWorker(workerSockets.get(node - 1), piece);
            }
        }
    }

    /** Sendet einen Teil-Array an einen einzelnen Worker */
    private void sendChunkToWorker(Socket worker, int[] piece) throws IOException {
        DataOutputStream dos = new DataOutputStream(worker.getOutputStream());
        dos.writeInt(piece.length);
        for (int v : piece) {
            dos.writeInt(v);
        }
        dos.flush();
    }

    /** Koordiniert START und DONE Runden und berechnet eigene Teilsumme */
    private void coordinateSummation() throws IOException {
        broadcastToWorkers("START");
        calculateOwnSum();
        awaitWorkersCompletion();
        broadcastToWorkers("DONE");
    }

    /** Broadcast einer Nachricht an alle Worker */
    private void broadcastToWorkers(String msg) throws IOException {
        System.out.println("[Coordinator] Broadcast: " + msg);
        for (Socket w : workerSockets) {
            PrintWriter out = new PrintWriter(w.getOutputStream(), true);
            out.println(msg);
        }
    }

    /** Berechnet die Summe des eigenen Chunks */
    private void calculateOwnSum() {
        sum = 0;
        for (int v : chunk) sum += v;
        System.out.println("[Coordinator] Own sum: " + sum);
    }

    /** Wartet auf "COMPLETED"-Meldung aller Worker */
    private void awaitWorkersCompletion() throws IOException {
        for (Socket w : workerSockets) {
            BufferedReader in = new BufferedReader(
                    new InputStreamReader(w.getInputStream()));
            String resp = in.readLine();
            System.out.println("[Coordinator] Worker response: " + resp);
        }
    }

    /** Führt die Reduktion aller Teilsummen durch und wertet das Ergebnis aus */
    private void performReduction() throws IOException {
        int finalSum = sum;
        for (Socket w : workerSockets) {
            finalSum += receiveInt(w);
        }
        System.out.println("[Coordinator] Final Sum: " + finalSum);
        System.out.println("[Coordinator] Sum correct? "
                + Utils.isSumCorrect(finalSum, initialArray));
    }

    /** Liest einen Integer von einem Socket */
    private int receiveInt(Socket sock) throws IOException {
        DataInputStream dis = new DataInputStream(sock.getInputStream());
        return dis.readInt();
    }

    /** Gibt den eigenen Chunk im Log aus */
    private void logOwnChunk() {
        System.out.println("[Coordinator] Own chunk: "
                + Arrays.toString(chunk));
    }

    // ----------------------------------------------------------------------------------------------------------------
    //  Worker-Methoden
    // ----------------------------------------------------------------------------------------------------------------

    /** Ablauf im Worker: Registrierung, Empfang des Chunks, Bearbeitung, Senden des Ergebnisses */
    private void runAsWorker() throws IOException {
        connectAndRegister();                // Socket-Verbindung und Registrierung
        receiveIDAndChunk();                 // Lese ID und Chunk vom Koordinator
        processRounds();                     // Warte auf START/DONE und berechne Teilsumme
        sendPartialSum();                    // Sende eigenen Summenwert zurück
    }

    /** Baut Socket-Verbindung zum Koordinator auf und sendet REGISTRATION */
    private void connectAndRegister() throws IOException {
        coordinatorSocket = new Socket(coordinatorIP, coordinatorPort);
        coordOut = new PrintWriter(coordinatorSocket.getOutputStream(), true);
        coordOut.println("REGISTRATION:" + ownIP + ":" + ownPort);
        coordIn = new BufferedReader(
                new InputStreamReader(coordinatorSocket.getInputStream()));
    }

    /** Lese zugewiesene ID und den Chunk */
    private void receiveIDAndChunk() throws IOException {
        DataInputStream dis = new DataInputStream(coordinatorSocket.getInputStream());
        id = dis.readInt();                  // Eindeutige Worker-ID
        chunk = readIntArray(dis);           // Chunk empfangen
        logWorkerChunk();
    }

    /** Liest ein Array (Länge + Werte) von DataInputStream */
    private int[] readIntArray(DataInputStream dis) throws IOException {
        int len = dis.readInt();
        int[] arr = new int[len];
        for (int i = 0; i < len; i++) arr[i] = dis.readInt();
        return arr;
    }

    /** Protokoll: Zeige erhaltenen Chunk */
    private void logWorkerChunk() {
        System.out.println("[Worker " + id + "] Chunk received: "
                + Arrays.toString(chunk));
    }

    /** Wartet auf START, berechnet Summe, signalisiert Abschlusspunkt, bis DONE */
    private void processRounds() throws IOException {
        while (true) {
            String signal = coordIn.readLine();
            if (signal == null) throw new IOException("Connection lost");
            if ("START".equals(signal)) calculateAndAcknowledge();
            if ("DONE".equals(signal)) break;
        }
    }

    /** Berechnet Teilsumme und sendet "COMPLETED" an Koordinator */
    private void calculateAndAcknowledge() {
        sum = 0;
        for (int v : chunk) sum += v;
        System.out.println("[Worker " + id + "] Partial sum: " + sum);
        coordOut.println("COMPLETED");
    }

    /** Sendet die berechnete Teilsumme über DataOutputStream */
    private void sendPartialSum() throws IOException {
        DataOutputStream dos = new DataOutputStream(coordinatorSocket.getOutputStream());
        dos.writeInt(sum);
        dos.flush();
    }
}
