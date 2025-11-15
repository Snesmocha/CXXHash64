#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "../cxxhash.h"  // You'll need the official xxHash library for comparison

// Benchmark configuration
typedef struct 
{
    size_t min_size;
    size_t max_size;
    size_t iterations;
    int warmup_runs;
} benchmark_config_t;

// Test data generation
static void* generate_test_data(size_t size)
{
    void* data = malloc(size);
    if (!data) return NULL;
    
    // Fill with pseudo-random but reproducible data
    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < size / sizeof(uint32_t); i++) {
        ((uint32_t*)data)[i] = seed;
        seed = seed * 1664525 + 1013904223; // LCG
    }
    return data;
}

// Timing utilities
static double get_time_sec() 
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Warm up the CPU/cache
static void benchmark_warmup(const void* data, size_t size, int runs) 
{
    volatile uint64_t dummy = 0;
    for (int i = 0; i < runs; i++) {
        dummy += xxhash64(data, size, 0);
    }
    (void)dummy; // Prevent optimization
}

// Single benchmark run
static double benchmark_single_run(const void* data, size_t size, 
                                   uint64_t (*hash_func)(const void*, size_t, uint64_t),
                                   uint64_t seed, size_t iterations) 
{
    double start_time = get_time_sec();
    
    volatile uint64_t checksum = 0;
    for (size_t i = 0; i < iterations; i++) {
        checksum += hash_func(data, size, seed);
        // Vary seed slightly to prevent overly optimistic branch prediction
        seed += 1;
    }
    
    double end_time = get_time_sec();
    (void)checksum; // Prevent optimization
    
    return end_time - start_time;
}

// Calculate bandwidth in GB/s
static double calculate_bandwidth_gbs(size_t data_size, size_t iterations, double time_sec) 
{
    double total_bytes = (double)data_size * iterations;
    return (total_bytes / time_sec) / (1024 * 1024 * 1024);
}

// Run comprehensive benchmark for a specific data size
static void benchmark_size(size_t size, const benchmark_config_t* config, 
                          const char* impl_name,
                          uint64_t (*hash_func)(const void*, size_t, uint64_t)) 
{
    void* test_data = generate_test_data(size);
    if (!test_data) {
        printf("Failed to allocate %zu bytes\n", size);
        return;
    }
    
    // Warm up
    benchmark_warmup(test_data, size, config->warmup_runs);
    
    // Determine iterations - more for small sizes, fewer for large
    size_t iterations = config->iterations;
    if (size > 1000000) iterations = config->iterations / 10;
    if (size > 10000000) iterations = config->iterations / 100;
    
    // Run benchmark
    double total_time = 0;
    double min_time = 1e9;
    double max_time = 0;
    int valid_runs = 0;
    
    for (int run = 0; run < 5; run++) { // Multiple runs for stability
        double run_time = benchmark_single_run(test_data, size, hash_func, 0, iterations);
        
        if (run_time > 0.001) { // Ignore very fast runs (timer precision issues)
            total_time += run_time;
            if (run_time < min_time) min_time = run_time;
            if (run_time > max_time) max_time = run_time;
            valid_runs++;
        }
    }
    
    if (valid_runs > 0) {
        double avg_time = total_time / valid_runs;
        double bandwidth = calculate_bandwidth_gbs(size, iterations, avg_time);
        double min_bandwidth = calculate_bandwidth_gbs(size, iterations, max_time); // inverse relationship
        double max_bandwidth = calculate_bandwidth_gbs(size, iterations, min_time);
        
        printf("%-20s | %10zu | %8.2f GB/s (min: %6.2f, max: %6.2f) | iterations: %zu\n",
               impl_name, size, bandwidth, min_bandwidth, max_bandwidth, iterations);
    }
    
    free(test_data);
}

// Test vector verification
static int verify_correctness() 
{
    struct {
        const char* input;
        uint64_t seed;
        uint64_t expected64;
    } test_vectors[] = {
        {"", 0, 0xef46db3751d8e999ULL},
        {"", 1, 0xd24ec4f1a98c6e5bULL},
        {"abc", 0, 0x44bc2cf5ad770999ULL},
        {"123456789012345", 0, 0xfd5e502a9d6e5116ULL},
    };
    
    printf("=== Correctness Verification ===\n");
    int all_passed = 1;
    
    for (size_t i = 0; i < sizeof(test_vectors)/sizeof(test_vectors[0]); i++) {
        uint64_t result = xxhash64(test_vectors[i].input, 
                               strlen(test_vectors[i].input), 
                               test_vectors[i].seed);
        
        int passed = (result == test_vectors[i].expected64);
        all_passed &= passed;
        
        printf("Test %zu: %s (got 0x%016llx, expected 0x%016llx)\n",
               i, passed ? "PASS" : "FAIL", 
               (unsigned long long)result,
               (unsigned long long)test_vectors[i].expected64);
    }
    
    printf("Overall: %s\n\n", all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_passed;
}

// Main benchmark function
void run_comprehensive_benchmark() {
    benchmark_config_t config = {
        .min_size = 1,
        .max_size = 64 * 1024 * 1024, // 64 MB
        .iterations = 1000,
        .warmup_runs = 1000
    };
    
    printf("=== xxHash64 Performance Benchmark ===\n");
    printf("System: Intel i7-9700K equivalent test\n");
    printf("Compiler: ");
    
#ifdef __clang__
    printf("clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif __GNUC__
    printf("gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif _MSC_VER
    printf("MSVC %d", _MSC_VER);
#else
    printf("Unknown");
#endif
    
    printf(" with -O3 optimization\n\n");
    
    // Verify correctness first
    if (!verify_correctness()) {
        printf("WARNING: Correctness tests failed! Performance results may be invalid.\n\n");
    }
    
    printf("=== Performance Results ===\n");
    printf("%-20s | %10s | %-45s | %s\n", 
           "Implementation", "Size", "Bandwidth", "Details");
    printf("--------------------|------------|-------------------------------------------------|-----------\n");
    
    // Test various sizes (similar to official benchmark)
    size_t test_sizes[] = {
        1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 
        192, 256, 384, 512, 768, 1024,                    // Small sizes
        1536, 2048, 3072, 4096, 6144, 8192,              // Medium sizes  
        12288, 16384, 24576, 32768, 49152, 65536,        // Large blocks
        98304, 131072, 196608, 262144, 393216, 524288,   // Big blocks
        786432, 1048576, 1572864, 2097152, 3145728,      // 1-3 MB
        4194304, 6291456, 8388608, 12582912, 16777216,   // 4-16 MB
        33554432, 67108864                               // 32-64 MB
    };
    
    for (size_t i = 0; i < sizeof(test_sizes)/sizeof(test_sizes[0]); i++) {
        if (test_sizes[i] >= config.min_size && test_sizes[i] <= config.max_size) {
            benchmark_size(test_sizes[i], &config, "xxHash64-reference", xxhash64);
            // Uncomment to test your implementation:
            // benchmark_size(test_sizes[i], &config, "xxHash64-custom", xxhash64);
        }
    }
    
    // Memory bandwidth reference test
    printf("\n=== Memory Bandwidth Reference ===\n");
    size_t large_size = 64 * 1024 * 1024; // 64MB
    void* large_data = generate_test_data(large_size);
    if (large_data) {
        double start = get_time_sec();
        volatile uint64_t sum = 0;
        for (size_t i = 0; i < large_size / sizeof(uint64_t); i++) {
            sum += ((uint64_t*)large_data)[i];
        }
        double end = get_time_sec();
        double mem_bandwidth = ((double)large_size / (end - start)) / (1024*1024*1024);
        printf("RAM Sequential Read: %.2f GB/s\n", mem_bandwidth);
        (void)sum;
        free(large_data);
    }
}

// Small data velocity test (important for hash tables)
void run_small_data_test() {
    printf("\n=== Small Data Velocity Test ===\n");
    printf("This test measures performance on typical hash table key sizes\n");
    
    size_t small_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128};
    const int iterations = 1000000;
    
    for (size_t i = 0; i < sizeof(small_sizes)/sizeof(small_sizes[0]); i++) {
        void* data = generate_test_data(small_sizes[i]);
        double start = get_time_sec();
        
        volatile uint64_t sum = 0;
        for (int j = 0; j < iterations; j++) {
            sum += xxhash64(data, small_sizes[i], j); // Varying seed
        }
        
        double end = get_time_sec();
        double hashes_per_sec = iterations / (end - start);
        
        printf("Size %3zu bytes: %8.2f hashes/sec (%.2f ns/hash)\n",
               small_sizes[i], hashes_per_sec, 1e9 / hashes_per_sec);
        
        (void)sum;
        free(data);
    }
}

int main() {
    printf("xxHash64 Benchmark Suite\n");
    printf("========================\n\n");
    
    run_comprehensive_benchmark();
    run_small_data_test();
    
    printf("\n=== Benchmark Complete ===\n");
    
    return 0;
}
