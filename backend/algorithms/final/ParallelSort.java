// ==================== ParallelSort.java ====================
import java.io.IOException;
import java.util.*;

/**
 * Distributed odd-even transposition sort
 */
public class ParallelSort implements Algorithm {
    private Communicator comm;
    private int[] localData;

    @Override
    public Object execute(Communicator comm, int[] localData) throws Exception {
        this.comm = comm;
        this.localData = localData;

        // Phase 1: Synchronisierter Presort
        if (comm.isRoot()) {
            comm.broadcast("PRESORT");
        } else {
            String cmd = comm.receiveBroadcast();
            assert "PRESORT".equals(cmd);
        }

        presort();

        // Barrier als Synchronisationsbarriere
        comm.barrier();
        System.out.println("[Rank " + comm.getRank() + "] Presort completed, waiting at barrier");

        // Phase 2: Odd-Even rounds
        boolean globalSwapped = true;
        int round = 0;

        while (globalSwapped) {
            round++;
            System.out.println("\n[Rank " + comm.getRank() + "] ========== ROUND " + round + " ==========");

            // Broadcast ODD phase
            if (comm.isRoot()) {
                comm.broadcast("ODD");
            } else {
                String phase = comm.receiveBroadcast();
                assert "ODD".equals(phase);
            }

            System.out.println("[Rank " + comm.getRank() + "] ODD PHASE - Array before: " + Arrays.toString(localData));
            boolean localOddSwap = executePhase("ODD");

            // Reduce ist nur für root blcokierend
            boolean globalOddSwap = comm.reduce(localOddSwap);

            // Broadcast EVEN phase, receive broadcast ist blockend.
            if (comm.isRoot()) {
                comm.broadcast("EVEN");
            } else {
                String phase = comm.receiveBroadcast();
                assert "EVEN".equals(phase);
            }

            System.out.println("[Rank " + comm.getRank() + "] EVEN PHASE - Array before: " + Arrays.toString(localData));
            boolean localEvenSwap = executePhase("EVEN");

            // Reduce statt AllReduce
            boolean globalEvenSwap = comm.reduce(localEvenSwap);

            // Nur der Coordinator wertet aus, ob weiter sortiert werden muss
            if (comm.isRoot()) {
                globalSwapped = globalOddSwap || globalEvenSwap;
                System.out.println("[Coordinator] Round " + round + " complete. Global swaps: " + globalSwapped);

                // Broadcast an Workers, ob weiter sortiert wird
                comm.broadcast(globalSwapped ? "CONTINUE" : "DONE");
            } else {
                // Workers warten auf Entscheidung vom Coordinator
                String decision = comm.receiveBroadcast();
                globalSwapped = "CONTINUE".equals(decision);
            }
        }

        // Phase 3: Gather sorted data
        if (comm.isRoot()) {
            comm.broadcast("GATHER");
            int[][] allChunks = comm.gather(this.localData);

            // Merge results
            List<Integer> finalResult = new ArrayList<>();
            for (int[] chunk : allChunks) {
                for (int val : chunk) {
                    finalResult.add(val);
                }
            }

            return finalResult;
        } else {
            String gatherCmd = comm.receiveBroadcast();
            assert "GATHER".equals(gatherCmd);
            comm.gather(this.localData);
            return null;
        }
    }

    /**
     * Execute one phase of odd-even sort
     */
    private boolean executePhase(String phase) throws IOException {
        boolean isOddPhase = "ODD".equals(phase);
        boolean isActive = (comm.getRank() % 2 == 1 && isOddPhase) || (comm.getRank() % 2 == 0 && !isOddPhase);

        if (isActive && comm.hasRightNeighbor()) {
            System.out.println("[Rank " + comm.getRank() + "] " + phase + " PHASE: I am ACTIVE, exchanging with right neighbor");
            return exchangeWithRight();
        } else if (!isActive && comm.hasLeftNeighbor()) {
            System.out.println("[Rank " + comm.getRank() + "] " + phase + " PHASE: I am PASSIVE, waiting for left neighbor");
            return receiveFromLeft();
        } else {
            System.out.println("[Rank " + comm.getRank() + "] " + phase + " PHASE: No neighbor to exchange with");
            return false;
        }
    }

    /**
     * Active process exchanges with right neighbor
     */
    private boolean exchangeWithRight() throws IOException {
        int myValue = localData[localData.length - 1];
        System.out.println("[Rank " + comm.getRank() + "] ACTIVE: Sending to right: " + myValue);

        comm.sendToRightNeighbor(myValue);
        int neighborValue = comm.receiveFromRightNeighbor();
        System.out.println("[Rank " + comm.getRank() + "] ACTIVE: Received from right: " + neighborValue);

        if (myValue > neighborValue) {
            System.out.println("[Rank " + comm.getRank() + "] ACTIVE: SWAP! My " + myValue + " > neighbor's " + neighborValue);
            localData[localData.length - 1] = neighborValue;
            Utils.insertFromRight(localData);
            System.out.println("[Rank " + comm.getRank() + "] ACTIVE: Array after swap: " + Arrays.toString(localData));
            return true;
        }
        System.out.println("[Rank " + comm.getRank() + "] ACTIVE: NO SWAP - My " + myValue + " <= neighbor's " + neighborValue);
        return false;
    }

    /**
     * Passive process receives from left neighbor
     */
    private boolean receiveFromLeft() throws IOException {
        int receivedValue = comm.receiveFromLeftNeighbor();
        int myValue = localData[0];
        System.out.println("[Rank " + comm.getRank() + "] PASSIVE: Received from left: " + receivedValue);
        System.out.println("[Rank " + comm.getRank() + "] PASSIVE: My first value: " + myValue);

        if (receivedValue > myValue) {
            System.out.println("[Rank " + comm.getRank() + "] PASSIVE: SWAP! Neighbor's " + receivedValue + " > my " + myValue);
            comm.sendToLeftNeighbor(myValue);
            System.out.println("[Rank " + comm.getRank() + "] PASSIVE: Sent back to left: " + myValue);
            localData[0] = receivedValue;
            Utils.insertFromLeft(localData);
            System.out.println("[Rank " + comm.getRank() + "] PASSIVE: Array after swap: " + Arrays.toString(localData));
            return true;
        } else {
            System.out.println("[Rank " + comm.getRank() + "] PASSIVE: NO SWAP - Neighbor's " + receivedValue + " <= my " + myValue);
            comm.sendToLeftNeighbor(receivedValue);
            System.out.println("[Rank " + comm.getRank() + "] PASSIVE: Sent back to left: " + receivedValue);
            return false;
        }
    }

    /**
     * Presort local data using merge sort
     */
    private void presort() {
        Utils.quickSort(localData);
        System.out.println("[Rank " + comm.getRank() + "] Presorted: " + Arrays.toString(localData));
    }
}