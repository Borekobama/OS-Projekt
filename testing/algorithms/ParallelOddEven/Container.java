import java.io.IOException;
import java.util.Arrays;

public class Container {
    private final int id;
    private int[] localArray;

    private final TCPConnection leftNeighborConnection;
    private final TCPConnection rightNeighborConnection;

    public Container(int id, TCPConnection left, TCPConnection right) {
        this.id = id;
        this.leftNeighborConnection = left;
        this.rightNeighborConnection = right;
    }

    public boolean exchangeIfActive(String roundType) {
        try {
            if ("PRESORT".equals(roundType)) {
                preSort();
                log("[INFO] Local presort completed.");
                return true;
            }

            boolean isOddRound = "ODD".equals(roundType);
            boolean isActive = (id % 2 == 1 && isOddRound) || (id % 2 == 0 && !isOddRound);

            log("[INFO] Round " + roundType + " - " + (isActive ? "ACTIVE" : "PASSIVE"));

            if (isActive && rightNeighborConnection != null) {
                return exchangeWithRight();
            } else if (!isActive && leftNeighborConnection != null) {
                return receiveFromLeft();
            } else {
                log("[INFO] No action required (no neighbor or not active).");
                return false;
            }

        } catch (IOException | NumberFormatException e) {
            log("[ERROR] Error during exchangeIfActive: " + e.getMessage());
            e.printStackTrace();
            return false;
        }
    }

    private boolean exchangeWithRight() throws IOException {
        int myValue = localArray[localArray.length - 1];
        sendToRight("VAL:" + myValue);

        String response = rightNeighborConnection.readLine();
        if (response == null) {
            log("[WARN] No response from right neighbor.");
            return false;
        }

        log("[RECV] from right: " + response);

        // hier logs einbauen eventuell
        int neighborValue = Integer.parseInt(response.replace("VAL:", ""));

        if (myValue > neighborValue) {
            localArray[localArray.length - 1] = neighborValue;
            SortUtils.insertFromRight(localArray);
            log("[SWAP] RIGHT: " + myValue + " > " + neighborValue);
            logArray();
            return true;
        }
        return false;
    }

    private boolean receiveFromLeft() throws IOException {
        String msg = leftNeighborConnection.readLine();
        if (msg == null) {
            log("[WARN] No message from left neighbor.");
            return false;
        }

        log("[RECV] from left: " + msg);
        int receivedValue = Integer.parseInt(msg.replace("VAL:", ""));
        int myValue = localArray[0];

        if (receivedValue > myValue) {
            localArray[0] = receivedValue;
            sendToLeft("VAL:" + myValue);
            SortUtils.insertFromLeft(localArray);
            log("[SWAP] LEFT: " + receivedValue + " > " + myValue);
            logArray();
            return true;
        } else {
            sendToLeft("VAL:" + receivedValue);
            return false;
        }
    }

    public void setLocalArray(int[] localArray) {
        this.localArray = localArray;
    }

    public int[] getLocalArray() {
        return localArray;
    }

    public void preSort() {
        SortUtils.mergeSort(localArray);
    }

    public void sendToLeft(String msg) {
        if (leftNeighborConnection != null) {
            log("[SEND] to left: " + msg);
            leftNeighborConnection.send(msg);
        }
    }

    public void sendToRight(String msg) {
        if (rightNeighborConnection != null) {
            log("[SEND] to right: " + msg);
            rightNeighborConnection.send(msg);
        }
    }

    private void log(String message) {
        System.out.println("[Container " + id + "] " + message);
    }

    public void logArray() {
        System.out.println("[Container " + id + "] Current array state: " + Arrays.toString(localArray));
    }

}
