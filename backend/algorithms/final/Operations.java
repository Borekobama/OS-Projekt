// ==================== Operations.java ====================
import java.util.Arrays;

/**
 * Basic distributed operations: SUM, MIN, MAX
 * Implemented as static inner classes
 */
public class Operations {

    /**
     * Distributed SUM operation. Work with for loops for performance reasons.
     */
    public static class Sum implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localSum = 0;
            for(int i : localData){
                localSum += i;
            }

            System.out.println("[Rank " + comm.getRank() + "] Local sum: " + localSum);

            if (comm.isRoot()) {
                return comm.reduce(localSum, Integer::sum);
            } else {
                comm.reduce(localSum, Integer::sum);
                return null;
            }
        }
    }

    /**
     * Distributed MIN operation. Work with for loops for performance reasons.
     */
    public static class Min implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localMin = localData[0];
            for (int i : localData) {
                if (i < localMin) {
                    localMin = i;
                }
            }

            System.out.println("[Rank " + comm.getRank() + "] Local minimum: " + localMin);

            if (comm.isRoot()) {
                return comm.reduce(localMin, Integer::min);
            } else {
                comm.reduce(localMin, Integer::min);
                return null;
            }
        }
    }

    /**
     * Distributed MAX operation. Work with for loops for performance reasons.
     */
    public static class Max implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localMax = localData[0];
            for (int i : localData) {
                if (i > localMax) {
                    localMax = i;
                }
            }

            System.out.println("[Rank " + comm.getRank() + "] Local maximum: " + localMax);

            if (comm.isRoot()) {
                return comm.reduce(localMax, Integer::max);
            } else {
                comm.reduce(localMax, Integer::max);
                return null;
            }
        }
    }
}