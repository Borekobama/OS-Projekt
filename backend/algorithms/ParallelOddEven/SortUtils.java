public class SortUtils {
    public static void mergeSort(int[] arr) {
        if (arr.length <= 1) return;
        int mid = arr.length / 2;
        int[] left = java.util.Arrays.copyOfRange(arr, 0, mid);
        int[] right = java.util.Arrays.copyOfRange(arr, mid, arr.length);
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

    // Fügt das erste Element von links beginnend in die vorsortierte Liste.
    // Effizienteste Lösung.
    public static void insertFromLeft(int[] arr) {
        int i = 0;
        int temp = arr[i];
        while (i + 1 < arr.length && temp > arr[i + 1]) {
            arr[i] = arr[i + 1];
            i++;
        }
        arr[i] = temp;
    }

    // Fügt das letzte Element von rechts beginnend in die vorsortierte Liste.
    public static void insertFromRight(int[] arr) {
        int i = arr.length - 1;
        int temp = arr[i];
        while (i - 1 >= 0 && temp < arr[i - 1]) {
            arr[i] = arr[i - 1];
            i--;
        }
        arr[i] = temp;
    }

}
