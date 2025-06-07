import java.io.*;
import java.net.Socket;

public class TCPConnection {
    private final Socket socket;
    private final BufferedReader in;
    private final PrintWriter out;

    public TCPConnection(Socket socket) throws IOException {
        this.socket = socket;
        this.in = new BufferedReader(
                new InputStreamReader(socket.getInputStream()));
        this.out = new PrintWriter(new BufferedWriter(
                new OutputStreamWriter(socket.getOutputStream())), true);
    }

    // Nachricht senden
    public void send(String message) {
        out.println(message);
        out.flush();
    }

    // Nachricht empfangen (blockierend)
    public String readLine() throws IOException {
        String line = in.readLine();
        if (line != null) {
            System.out.println("[TCPConnection] Read: " + line);
        } else {
            System.out.println("[TCPConnection] Warning: null received "
                    + "(connection may be terminated).");
        }
        return line;
    }
}
