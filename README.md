# yaxmalloc : Bare-Metal C++ Memory Allocator

## Architecture Overview
This project is a custom implementation of a dynamic memory allocator (a lightweight replacement for `malloc`, `free`, and `realloc`) written from scratch in C++. Built to interface directly with the OS via POSIX-style mechanics, it manages the heap boundary and completely bypasses the standard library.

## Phase 1 Technical Specifications
* **Engine:** Implicit Free List with a First-Fit search heuristic.
* **Kernel Interface:** Utilizes `sbrk()` mechanics for dynamic heap boundary manipulation.
* **Memory Safety:** Enforces strict 8-byte (Double Word) payload alignment via 1-cycle bitwise operations (`& ~0x7`) to prevent hardware-level CPU cache penalties.
* **Fragmentation Control:** Immediate, constant-time boundary tag coalescing merges adjacent free blocks to eliminate external memory fragmentation.
* **Metadata Overhead:** 8 bytes per block (4-byte Header, 4-byte Footer) utilizing bitwise masking to store allocation status in the least significant bit (LSB).

## Benchmarks & Performance Profile (Implicit List)
The engine is actively tested against the standard C library (`glibc`) `malloc` using the `google/benchmark` framework across hundreds of millions of iterations.
* **Average Throughput (8B - 512B payloads):** ~3.8 ns for `glibc` vs ~16.4 ns for Custom (Implicit).
* **Architectural Bottleneck:** The $O(N)$ linear scan of the Implicit Free List performs approximately 4.4x slower than the kernel's optimized standard library for small allocations.

## Phase 2 Technical Specifications
* **Engine Upgrade:** Transitioned from an Implicit to an Explicit Free List using a Doubly Linked List architecture.
* **O(1) List Management:** Hijacked the unused payload space of free blocks to physically store 8-byte `PREV` and `NEXT` memory addresses. Minimum block size structurally enforced at 24 bytes (16 bytes payload + 8 bytes tags) to prevent 64-bit pointer collisions.
* **LIFO Insertion:** Newly freed blocks are instantly routed to the head of the list, reducing insertion and detachment time complexity from $O(N)$ to $O(1)$.
* **Performance Reality (Explicit List):** The explicit implementation correctly bypasses allocated blocks. However, small allocation throughput (8B - 512B) currently sits at ~19 ns. Because it relies on a First-Fit traversal of all *free* blocks, the $O(N_{free})$ search still trails `glibc`'s $O(1)$ fastbins. 
* **High-Bandwidth Parity:** At 8192-byte allocations, the custom allocator achieves parity with `glibc` (12.6 ns vs 12.2 ns), as memory bandwidth and OS paging overhead replace list traversal as the primary performance bottleneck.