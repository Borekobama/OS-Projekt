#include "algorithms.h"
#include "communicator.h"

// Forward declarations for internal functions
void quick_sort_recursive(int* arr, int low, int high);
int partition(int* arr, int low, int high);
void swap(int* a, int* b);

// Sort context and helper functions
typedef struct {
    Communicator* comm;
    int* local_data;
    int length;
} SortContext;

bool execute_phase(SortContext* ctx, const char* phase);
bool exchange_with_right(SortContext* ctx);
bool receive_from_left(SortContext* ctx);
void presort(int* data, int length);

// Algorithm implementations
void* sum_algorithm(Communicator* comm, int* local_data, int length) {
    int local_sum = 0;
    for (int i = 0; i < length; i++) {
        local_sum += local_data[i];
    }

    printf("[Rank %d] Local sum: %d\n", comm->rank, local_sum);

    if (comm->is_root) {
        int result = reduce_int(comm, local_sum, sum_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_sum, sum_op);
        return NULL;
    }
}

void* min_algorithm(Communicator* comm, int* local_data, int length) {
    int local_min = local_data[0];
    for (int i = 1; i < length; i++) {
        if (local_data[i] < local_min) {
            local_min = local_data[i];
        }
    }

    printf("[Rank %d] Local minimum: %d\n", comm->rank, local_min);

    if (comm->is_root) {
        int result = reduce_int(comm, local_min, min_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_min, min_op);
        return NULL;
    }
}

void* max_algorithm(Communicator* comm, int* local_data, int length) {
    int local_max = local_data[0];
    for (int i = 1; i < length; i++) {
        if (local_data[i] > local_max) {
            local_max = local_data[i];
        }
    }

    printf("[Rank %d] Local maximum: %d\n", comm->rank, local_max);

    if (comm->is_root) {
        int result = reduce_int(comm, local_max, max_op);
        int* result_ptr = malloc(sizeof(int));
        *result_ptr = result;
        return result_ptr;
    } else {
        reduce_int(comm, local_max, max_op);
        return NULL;
    }
}

void* sort_algorithm(Communicator* comm, int* local_data, int length) {
    SortContext ctx = {comm, local_data, length};

    // Phase 1: Synchronized presort
    if (comm->is_root) {
        broadcast_string(comm, "PRESORT");
    } else {
        char* cmd = receive_broadcast(comm);
        free(cmd);
    }

    presort(local_data, length);
    printf("[Rank %d] Presorted: [", comm->rank);
    for (int i = 0; i < length; i++) {
        printf("%d%s", local_data[i], (i < length - 1) ? ", " : "");
    }
    printf("]\n");

    barrier(comm);
    printf("[Rank %d] Presort completed, waiting at barrier\n", comm->rank);

    // Phase 2: Odd-Even rounds
    bool global_swapped = true;
    int round = 0;

    while (global_swapped) {
        round++;
        printf("\n[Rank %d] ========== ROUND %d ==========\n", comm->rank, round);

        // ODD phase
        if (comm->is_root) {
            broadcast_string(comm, "ODD");
        } else {
            char* phase = receive_broadcast(comm);
            free(phase);
        }

        printf("[Rank %d] ODD PHASE - Array before: [", comm->rank);
        for (int i = 0; i < length; i++) {
            printf("%d%s", local_data[i], (i < length - 1) ? ", " : "");
        }
        printf("]\n");

        bool local_odd_swap = execute_phase(&ctx, "ODD");
        bool global_odd_swap = reduce_bool(comm, local_odd_swap);

        // EVEN phase
        if (comm->is_root) {
            broadcast_string(comm, "EVEN");
        } else {
            char* phase = receive_broadcast(comm);
            free(phase);
        }

        printf("[Rank %d] EVEN PHASE - Array before: [", comm->rank);
        for (int i = 0; i < length; i++) {
            printf("%d%s", local_data[i], (i < length - 1) ? ", " : "");
        }
        printf("]\n");

        bool local_even_swap = execute_phase(&ctx, "EVEN");
        bool global_even_swap = reduce_bool(comm, local_even_swap);

        // Check if we should continue
        if (comm->is_root) {
            global_swapped = global_odd_swap || global_even_swap;
            printf("[Coordinator] Round %d complete. Global swaps: %s\n",
                   round, global_swapped ? "true" : "false");

            broadcast_string(comm, global_swapped ? "CONTINUE" : "DONE");
        } else {
            char* decision = receive_broadcast(comm);
            global_swapped = (strcmp(decision, "CONTINUE") == 0);
            free(decision);
        }
    }

    // Phase 3: Gather sorted data
    if (comm->is_root) {
        broadcast_string(comm, "GATHER");
        int** all_chunks = gather(comm, local_data, length);

        // Calculate total size
        int* chunk_sizes = calculate_chunk_sizes(100, comm->size); // Assuming original array size 100
        int total_size = 0;
        for (int i = 0; i < comm->size; i++) {
            total_size += chunk_sizes[i];
        }

        // Merge results
        int* final_result = malloc(total_size * sizeof(int));
        int index = 0;
        for (int i = 0; i < comm->size; i++) {
            memcpy(&final_result[index], all_chunks[i], chunk_sizes[i] * sizeof(int));
            index += chunk_sizes[i];
            free(all_chunks[i]);
        }

        free(all_chunks);
        free(chunk_sizes);
        return final_result;
    } else {
        char* gather_cmd = receive_broadcast(comm);
        free(gather_cmd);
        gather(comm, local_data, length);
        return NULL;
    }
}

bool execute_phase(SortContext* ctx, const char* phase) {
    bool is_odd_phase = (strcmp(phase, "ODD") == 0);
    bool is_active = (ctx->comm->rank % 2 == 1 && is_odd_phase) ||
                     (ctx->comm->rank % 2 == 0 && !is_odd_phase);

    if (is_active && ctx->comm->has_right_neighbor) {
        printf("[Rank %d] %s PHASE: I am ACTIVE, exchanging with right neighbor\n",
               ctx->comm->rank, phase);
        return exchange_with_right(ctx);
    } else if (!is_active && ctx->comm->has_left_neighbor) {
        printf("[Rank %d] %s PHASE: I am PASSIVE, waiting for left neighbor\n",
               ctx->comm->rank, phase);
        return receive_from_left(ctx);
    } else {
        printf("[Rank %d] %s PHASE: No neighbor to exchange with\n",
               ctx->comm->rank, phase);
        return false;
    }
}

bool exchange_with_right(SortContext* ctx) {
    int my_value = ctx->local_data[ctx->length - 1];
    printf("[Rank %d] ACTIVE: Sending to right: %d\n", ctx->comm->rank, my_value);

    send_to_right_neighbor(ctx->comm, my_value);
    int neighbor_value = receive_from_right_neighbor(ctx->comm);
    printf("[Rank %d] ACTIVE: Received from right: %d\n", ctx->comm->rank, neighbor_value);

    if (my_value > neighbor_value) {
        printf("[Rank %d] ACTIVE: SWAP! My %d > neighbor's %d\n",
               ctx->comm->rank, my_value, neighbor_value);
        ctx->local_data[ctx->length - 1] = neighbor_value;
        insert_from_right(ctx->local_data, ctx->length);

        printf("[Rank %d] ACTIVE: Array after swap: [", ctx->comm->rank);
        for (int i = 0; i < ctx->length; i++) {
            printf("%d%s", ctx->local_data[i], (i < ctx->length - 1) ? ", " : "");
        }
        printf("]\n");
        return true;
    }

    printf("[Rank %d] ACTIVE: NO SWAP - My %d <= neighbor's %d\n",
           ctx->comm->rank, my_value, neighbor_value);
    return false;
}

bool receive_from_left(SortContext* ctx) {
    int received_value = receive_from_left_neighbor(ctx->comm);
    int my_value = ctx->local_data[0];

    printf("[Rank %d] PASSIVE: Received from left: %d\n", ctx->comm->rank, received_value);
    printf("[Rank %d] PASSIVE: My first value: %d\n", ctx->comm->rank, my_value);

    if (received_value > my_value) {
        printf("[Rank %d] PASSIVE: SWAP! Neighbor's %d > my %d\n",
               ctx->comm->rank, received_value, my_value);
        send_to_left_neighbor(ctx->comm, my_value);
        printf("[Rank %d] PASSIVE: Sent back to left: %d\n", ctx->comm->rank, my_value);

        ctx->local_data[0] = received_value;
        insert_from_left(ctx->local_data, ctx->length);

        printf("[Rank %d] PASSIVE: Array after swap: [", ctx->comm->rank);
        for (int i = 0; i < ctx->length; i++) {
            printf("%d%s", ctx->local_data[i], (i < ctx->length - 1) ? ", " : "");
        }
        printf("]\n");
        return true;
    } else {
        printf("[Rank %d] PASSIVE: NO SWAP - Neighbor's %d <= my %d\n",
               ctx->comm->rank, received_value, my_value);
        send_to_left_neighbor(ctx->comm, received_value);
        printf("[Rank %d] PASSIVE: Sent back to left: %d\n", ctx->comm->rank, received_value);
        return false;
    }
}

void presort(int* data, int length) {
    quick_sort(data, length);
}

// Utility functions
int* create_random_array(int length) {
    int* array = malloc(length * sizeof(int));
    srand(time(NULL));
    for (int i = 0; i < length; i++) {
        array[i] = (rand() % 99) + 1;
    }
    return array;
}

int* calculate_chunk_sizes(int array_length, int num_processes) {
    int* sizes = malloc(num_processes * sizeof(int));
    int base_size = array_length / num_processes;
    int remainder = array_length % num_processes;

    for (int i = 0; i < num_processes; i++) {
        sizes[i] = base_size;
        if (i < remainder) {
            sizes[i]++;
        }
    }

    return sizes;
}

void quick_sort(int* arr, int length) {
    quick_sort_recursive(arr, 0, length - 1);
}

void quick_sort_recursive(int* arr, int low, int high) {
    if (low < high) {
        int pivot_index = partition(arr, low, high);
        quick_sort_recursive(arr, low, pivot_index - 1);
        quick_sort_recursive(arr, pivot_index + 1, high);
    }
}

int partition(int* arr, int low, int high) {
    // Median-of-three pivot selection
    int mid = low + (high - low) / 2;
    if (arr[mid] < arr[low]) swap(&arr[low], &arr[mid]);
    if (arr[high] < arr[low]) swap(&arr[low], &arr[high]);
    if (arr[high] < arr[mid]) swap(&arr[mid], &arr[high]);
    swap(&arr[mid], &arr[high]);

    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void insert_from_left(int* arr, int length) {
    int i = 0;
    int temp = arr[i];
    while (i + 1 < length && temp > arr[i + 1]) {
        arr[i] = arr[i + 1];
        i++;
    }
    arr[i] = temp;
}

void insert_from_right(int* arr, int length) {
    int i = length - 1;
    int temp = arr[i];
    while (i - 1 >= 0 && temp < arr[i - 1]) {
        arr[i] = arr[i - 1];
        i--;
    }
    arr[i] = temp;
}

// Validation functions
bool validate_sum(int calculated_sum, int* original_array, int length) {
    int expected_sum = 0;
    for (int i = 0; i < length; i++) {
        expected_sum += original_array[i];
    }
    return calculated_sum == expected_sum;
}

bool validate_min(int calculated_min, int* original_array, int length) {
    int expected_min = original_array[0];
    for (int i = 1; i < length; i++) {
        if (original_array[i] < expected_min) {
            expected_min = original_array[i];
        }
    }
    return calculated_min == expected_min;
}

bool validate_max(int calculated_max, int* original_array, int length) {
    int expected_max = original_array[0];
    for (int i = 1; i < length; i++) {
        if (original_array[i] > expected_max) {
            expected_max = original_array[i];
        }
    }
    return calculated_max == expected_max;
}

bool is_sorted(int* array, int length) {
    for (int i = 0; i < length - 1; i++) {
        if (array[i] > array[i + 1]) {
            return false;
        }
    }
    return true;
}