// ==================== Utils.java ====================
import java.util.*;

/**
 * Utility methods for array operations, sorting, and validation
 */
public class Utils {
    private static final Random random = new Random();

    /**
     * Create random array with values 1-99
     */
    public static int[] createRandomArray(int length) {
        return random.ints(length, 1, 100).toArray();
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
     * Optimized quicksort implementation with Median-of-Three Pivot.
     */
    public static void quickSort(int[] arr) {
        quickSort(arr, 0, arr.length - 1);
    }

    private static void quickSort(int[] arr, int low, int high) {
        if (low < high) {
            int pivotIndex = partition(arr, low, high);
            quickSort(arr, low, pivotIndex - 1);
            quickSort(arr, pivotIndex + 1, high);
        }
    }

    private static int partition(int[] arr, int low, int high) {
        // Use median-of-three for better pivot selection
        int mid = low + (high - low) / 2;
        if (arr[mid] < arr[low]) swap(arr, low, mid);
        if (arr[high] < arr[low]) swap(arr, low, high);
        if (arr[high] < arr[mid]) swap(arr, mid, high);
        swap(arr, mid, high); // Move pivot to end

        int pivot = arr[high];
        int i = low - 1;

        for (int j = low; j < high; j++) {
            if (arr[j] <= pivot) {
                i++;
                swap(arr, i, j);
            }
        }
        swap(arr, i + 1, high);
        return i + 1;
    }

    private static void swap(int[] arr, int i, int j) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }

    /**
     * Insert element from left in a presorted list (for odd-even sort)
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
     * Insert element from right in a presorted list (for odd-even sort)
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