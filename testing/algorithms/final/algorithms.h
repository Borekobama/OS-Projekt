#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "common.h"

// Algorithm functions
void* sum_algorithm(Communicator* comm, int* local_data, int length);
void* min_algorithm(Communicator* comm, int* local_data, int length);
void* max_algorithm(Communicator* comm, int* local_data, int length);
void* sort_algorithm(Communicator* comm, int* local_data, int length);

// Utility functions
int* create_random_array(int length);
int* calculate_chunk_sizes(int array_length, int num_processes);
void quick_sort(int* arr, int length);
void insert_from_left(int* arr, int length);
void insert_from_right(int* arr, int length);

// Validation functions
bool validate_sum(int calculated_sum, int* original_array, int length);
bool validate_min(int calculated_min, int* original_array, int length);
bool validate_max(int calculated_max, int* original_array, int length);
bool is_sorted(int* array, int length);

#endif // ALGORITHMS_H