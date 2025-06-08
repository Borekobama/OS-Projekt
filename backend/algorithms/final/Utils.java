// ==================== Utils.java ====================
import java.util.*;

/**
 * Utility methods for array operations, sorting, and validation
 */
public class Utils {
    private static final Random random = new Random();

    /**
     * Create random array with values 1-98
     */
    public static int[] createRandomArray(int length) {
        return random.ints(length, 1, 99).toArray();
    }

    /**
     * Calculate chunk sizes for fair distribution
     */
    public static int[] calculateChunkSizes(int arrayLength, int numProcesses) {
        int[] sizes = new int[numProcesses];
        int baseSize = arrayLength / numProcesses;
        int remainder = arrayLength % numProcesses;

        Arrays.fill(sizes, baseSize);
        for (int i = 0; i < remainder; i++) {
            sizes[i]++;
        }
        return sizes;
    }

    /**
     * Merge sort implementation
     */
    public static void mergeSort(int[] arr) {
        if (arr.length <= 1) return;
        int mid = arr.length / 2;
        int[] left = Arrays.copyOfRange(arr, 0, mid);
        int[] right = Arrays.copyOfRange(arr, mid, arr.length);
        mergeSort(left);
        mergeSort(right);
        merge(left, right, arr);
    }

    private static void merge(int[] left, int[] right, int[] result) {
        int i = 0, j = 0, k = 0;
        while (i < left.length && j < right.length) {
            result[k++] = (left[i] <= right[j]) ? left[i++] : right[j++];
        }
        while (i < left.length) result[k++] = left[i++];
        while (j < right.length) result[k++] = right[j++];
    }

    /**
     * Insert element from left (for odd-even sort)
     */
    public static void insertFromLeft(int[] arr) {
        int i = 0;
        int temp = arr[i];
        while (i + 1 < arr.length && temp > arr[i + 1]) {
            arr[i] = arr[i + 1];
            i++;
        }
        arr[i] = temp;
    }

    /**
     * Insert element from right (for odd-even sort)
     */
    public static void insertFromRight(int[] arr) {
        int i = arr.length - 1;
        int temp = arr[i];
        while (i - 1 >= 0 && temp < arr[i - 1]) {
            arr[i] = arr[i - 1];
            i--;
        }
        arr[i] = temp;
    }

    /**
     * Validation methods
     */
    public static boolean validateSum(int calculatedSum, int[] originalArray) {
        return calculatedSum == Arrays.stream(originalArray).sum();
    }

    public static boolean validateMin(int calculatedMin, int[] originalArray) {
        return calculatedMin == Arrays.stream(originalArray).min().orElse(Integer.MAX_VALUE);
    }

    public static boolean validateMax(int calculatedMax, int[] originalArray) {
        return calculatedMax == Arrays.stream(originalArray).max().orElse(Integer.MIN_VALUE);
    }

    public static boolean isSorted(List<Integer> list) {
        for (int i = 0; i < list.size() - 1; i++) {
            if (list.get(i) > list.get(i + 1)) return false;
        }
        return true;
    }
}