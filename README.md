# yaxmalloc : Bare-Metal C++ Memory Allocator

## Architecture Overview
This project is a custom implementation of a dynamic memory allocator (a lightweight replacement for `malloc`, `free`, and `realloc`) written from scratch in C++. Built to interface directly with the OS via POSIX-style mechanics, it manages the heap boundary and completely bypasses the standard library.

## Phase 1 Technical Specifications
* **Engine:** Implicit Free List with a First-Fit search heuristic.
* **Kernel Interface:** Utilizes `sbrk()` mechanics for dynamic heap boundary manipulation.
* **Memory Safety:** Enforces strict 8-byte (Double Word) payload alignment via 1-cycle bitwise operations (`& ~0x7`) to prevent hardware-level CPU cache penalties.
* **Fragmentation Control:** Immediate, constant-time boundary tag coalescing merges adjacent free blocks to eliminate external memory fragmentation.
* **Metadata Overhead:** 8 bytes per block (4-byte Header, 4-byte Footer) utilizing bitwise masking to store allocation status in the least significant bit (LSB).

## Benchmarks & Performance profile
The engine is actively tested against the standard C library (`glibc`) `malloc` using the `google/benchmark` framework across hundreds of millions of iterations.

* **Average Throughput (8B - 512B payloads):** ~16.4 ns per allocation/free cycle.
* **Architectural Bottleneck:** The current $O(N)$ linear scan of the Implicit Free List performs approximately 4.4x slower than the kernel's optimized standard library. 

## Engineering Roadmap
* **Phase 2 (Next Target):** Upgrade the core engine to an **Explicit Free List**. By embedding physical `NEXT` and `PREV` pointers directly inside the unused payload space of free blocks, the allocation time complexity will be reduced from $O(N)$ to $O(1)$, directly bridging the nanosecond latency gap with the OS.