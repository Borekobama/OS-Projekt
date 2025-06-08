// ==================== Operations.java ====================
import java.util.Arrays;

/**
 * Basic distributed operations: SUM, MIN, MAX
 * Implemented as static inner classes
 */
public class Operations {

    /**
     * Distributed SUM operation
     */
    public static class Sum implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localSum = Arrays.stream(localData).sum();
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
     * Distributed MIN operation
     */
    public static class Min implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localMin = Arrays.stream(localData).min().orElse(Integer.MAX_VALUE);
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
     * Distributed MAX operation
     */
    public static class Max implements Algorithm {
        @Override
        public Object execute(Communicator comm, int[] localData) throws Exception {
            int localMax = Arrays.stream(localData).max().orElse(Integer.MIN_VALUE);
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