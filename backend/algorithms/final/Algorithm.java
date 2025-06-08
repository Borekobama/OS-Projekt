// ==================== Algorithm.java ====================
/**
 * Simple interface for all distributed algorithms
 */
public interface Algorithm {
    /**
     * Execute the algorithm and return result (null for workers)
     */
    Object execute(Communicator comm, int[] localData) throws Exception;
}