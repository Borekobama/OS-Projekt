// backend/algorithms/oddevensort/utils.js

/**
 * Simple Insertion Sort in place: sorts array[ start .. end−1 ].
 */
function insertionSort(arr, start, end) {
  for (let i = start + 1; i < end; i++) {
    let key = arr[i];
    let j = i - 1;
    while (j >= start && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

/**
 * Merge an array of sorted sub‐arrays into one big sorted array.
 * (Classic k-way merge by concatenation and sort, or simple flat and sort,
 * because after odd-even phases each chunk is already individually sorted
 * and boundary conditions ensure global sortedness when concatenated.)
 */
function mergeArrays(arrays) {
  // Simple approach: concat and quick‐sort. For large data you’d do a proper k-way merge.
  const flat = arrays.flat();
  return flat.sort((a, b) => a - b);
}

module.exports = {
  insertionSort,
  mergeArrays
};
