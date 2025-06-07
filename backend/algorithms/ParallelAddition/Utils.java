import java.util.List;
import java.util.Random;

public class Utils {

    public static boolean isSorted(List<Integer> list) {
        for (int i = 0; i < list.size() - 1; i++) {
            if (list.get(i) > list.get(i + 1)) return false;
        }
        return true;
    }



    public static int[] createArray(int arrayLength) {
        Random rnd = new Random();
        return rnd.ints(arrayLength, 0, 100).toArray();
    }

    public static boolean isSumCorrect(int finalSum, int[] initialArray) {
        int sum = 0;
        for (int i : initialArray) {
            sum += i;
        }
        return finalSum == sum;
    }
}
