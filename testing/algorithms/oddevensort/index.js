// backend/algorithms/oddevensort/index.js

/**
 * Odd-Even Sort (Parallel) implementation in Node.js.
 *
 * Each worker gets a contiguous “chunk” of the original array.
 *  1) Each chunk is locally sorted first (using insertion sort).
 *  2) Then we loop “Round 0, Round 1, ...”:
 *       • Odd phase: worker‐1 compares last element of its chunk
 *         with the first element of worker‐2’s chunk; worker‐3 with worker-4, etc.
 *       • Even phase: worker-0 with worker-1; worker-2 with worker-3, etc.
 *     We keep going until no more swaps across any pair of neighbors.
 *  3) Finally we concatenate all sorted chunks in order.
 *
 * The Coordinator will call these functions in sequence, sending/receiving
 * data to/from each worker via Socket.io.
 */

const { insertionSort, mergeArrays } = require('./utils');

/**
 * Split a big array into N approximately equal‐sized chunks.
 * Returns an array of length N, each element is { chunk: [...], offset: <startIndex> }.
 */
function splitIntoChunks(fullArray, numWorkers) {
  const len = fullArray.length;
  const baseSize = Math.floor(len / numWorkers);
  const remainder = len % numWorkers;

  const chunks = [];
  let cursor = 0;

  for (let i = 0; i < numWorkers; i++) {
    const extra = i < remainder ? 1 : 0;
    const size = baseSize + extra;
    const slice = fullArray.slice(cursor, cursor + size);
    chunks.push({ chunk: slice, offset: cursor });
    cursor += size;
  }
  return chunks;
}

/**
 * Perform one “swap check” between two neighboring chunks:
 * Given workerA’s sorted array A and workerB’s sorted array B,
 * compare A[last] vs. B[0]. If A[last] > B[0], swap those two elements,
 * then re‐insert them locally to keep each chunk sorted.
 *
 * Returns true if a swap occurred, false otherwise.
 */
function trySwapNeighbor(chunkA, chunkB) {
  const aLast = chunkA[chunkA.length - 1];
  const bFirst = chunkB[0];

  if (aLast > bFirst) {
    // Swap those two values
    chunkA[chunkA.length - 1] = bFirst;
    chunkB[0] = aLast;

    // Now re‐insert into sorted order within each chunk
    insertionSort(chunkA, 0, chunkA.length);
    insertionSort(chunkB, 0, chunkB.length);
    return true;
  }
  return false;
}

/**
 * Perform the full Odd-Even Sort across all chunks.
 *
 * Input:
 *   - chunks: Array of length N, each is { chunk: [numbers], offset: <startIndex> }
 *   - io: Socket.io server instance (to emit logs or status updates)
 *   - namespace (optional): string to prepend to any log messages/event names
 *
 * Output:
 *   - Promise that resolves to a single sorted array (concatenation of all chunk arrays) once done.
 */
async function oddEvenSortParallel(chunks, io, namespace = '') {
  // 1) First, locally sort each chunk with insertion sort
  for (let i = 0; i < chunks.length; i++) {
    insertionSort(chunks[i].chunk, 0, chunks[i].chunk.length);
    // Optionally emit a log that worker i’s chunk is locally sorted
    if (io) {
      io.emit(`${namespace}log`, `Worker ${i} locally sorted its chunk: [${chunks[i].chunk.join(', ')}]`);
    }
  }

  let swappedAny = false;
  let round = 0;

  do {
    swappedAny = false;
    // “Odd” phase: compare chunk pairs (1↔2), (3↔4), ...
    for (let w = 1; w + 1 < chunks.length; w += 2) {
      const didSwap = trySwapNeighbor(chunks[w].chunk, chunks[w + 1].chunk);
      swappedAny = swappedAny || didSwap;
      if (io && didSwap) {
        io.emit(`${namespace}log`, `Round ${round} Odd Phase: Worker ${w} ↔ Worker ${w + 1} swapped.`);
      }
    }

    // “Even” phase: compare (0↔1), (2↔3), ...
    for (let w = 0; w + 1 < chunks.length; w += 2) {
      const didSwap = trySwapNeighbor(chunks[w].chunk, chunks[w + 1].chunk);
      swappedAny = swappedAny || didSwap;
      if (io && didSwap) {
        io.emit(`${namespace}log`, `Round ${round} Even Phase: Worker ${w} ↔ Worker ${w + 1} swapped.`);
      }
    }

    if (io) {
      io.emit(`${namespace}log`, `Round ${round} completed. Swapped ${swappedAny}.`);
    }
    round++;
  } while (swappedAny);

  // Finally, merge all chunks in order and return
  const sorted = mergeArrays(chunks.map(c => c.chunk));
  if (io) {
    io.emit(`${namespace}log`, `All rounds done. Final merged array length = ${sorted.length}.`);
  }
  return sorted;
}

module.exports = {
  splitIntoChunks,
  oddEvenSortParallel
};
