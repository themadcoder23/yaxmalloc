#include <benchmark/benchmark.h>
#include <cstdlib>

// Forward declarations from your mm.cpp engine
extern void mem_init_sandbox();
extern int mm_init();
extern void* mm_malloc(size_t size);
extern void mm_free(void* ptr);

// --- BASELINE: System Malloc ---
static void BM_SystemMalloc(benchmark::State& state) {
    size_t alloc_size = state.range(0);
    for (auto _ : state) {
        void* ptr = std::malloc(alloc_size);
        benchmark::DoNotOptimize(ptr); // Prevent compiler from deleting the allocation
        std::free(ptr);
    }
}
// Test allocation sizes from 8 bytes to 8KB
BENCHMARK(BM_SystemMalloc)->Range(8, 1<<13); 

// --- OUR ENGINE: Custom Implicit Malloc ---
static bool initialized = false;
static void BM_CustomMalloc(benchmark::State& state) {
    if (!initialized) {
        mem_init_sandbox();
        if (mm_init() == -1) {
            state.SkipWithError("Heap initialization failed");
            return;
        }
        initialized = true;
    }
    
    size_t alloc_size = state.range(0);
    for (auto _ : state) {
        void* ptr = mm_malloc(alloc_size);
        benchmark::DoNotOptimize(ptr);
        if (ptr != NULL) {
            mm_free(ptr);
        }
    }
}
BENCHMARK(BM_CustomMalloc)->Range(8, 1<<13);

BENCHMARK_MAIN();