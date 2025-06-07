// ==================== ReduceOperation.java ====================

/**
 * A functional interface representing a reduction operation on two integers.
 * Can be used to define custom or predefined reduction strategies (e.g., SUM, MAX, MIN).
 */
@FunctionalInterface
public interface ReduceOperation {

    /**
     * Applies the reduction operation to the given operands.
     *
     * @param a the first operand
     * @param b the second operand
     * @return the result of reducing a and b
     */
    int apply(int a, int b);

    /**
     * Predefined reduction operation for summation.
     */
    ReduceOperation SUM = Integer::sum;

    /**
     * Predefined reduction operation for finding the maximum.
     */
    ReduceOperation MAX = Integer::max;

    /**
     * Predefined reduction operation for finding the minimum.
     */
    ReduceOperation MIN = Integer::min;
}
