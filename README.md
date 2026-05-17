## Architecture Overview
This project is a custom implementation of a dynamic memory allocator (a lightweight replacement for `malloc`, `free`, and `realloc`) written from scratch in C++. Built to interface directly with the Linux kernel via POSIX system calls, it manages the heap boundary, completely bypassing the standard library.

### Technical Specifications(Update Date : 17/05/2026)
* **Engine:** Implicit Free List with First-Fit search heuristic.
* **Kernel Interface:** Utilizes raw `sbrk()` system calls for heap boundary manipulation.
* **Memory Safety:** Enforces strict 8-byte (Double Word) payload alignment to prevent hardware-level cache penalties.
* **Fragmentation Control:** Immediate, constant-time boundary tag coalescing merges adjacent free blocks to eliminate external memory fragmentation.
* **Metadata Overhead:** 8 bytes per block (4-byte Header, 4-byte Footer) utilizing bitwise masking to store allocation status in the LSB.
