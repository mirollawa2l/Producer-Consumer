[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/CVb3AUs2)


# Producer-Consumer Shared Memory IPC Project

## Overview
This project implements a **Producer-Consumer system using shared memory and semaphores** in C++ on Linux. It demonstrates inter-process communication (IPC) with multiple producers and a consumer, managing a bounded buffer with proper synchronization.  

Key features:  
- Multiple producers generating commodity prices with normal distribution.  
- Shared buffer implemented in **shared memory**.  
- **Mutex semaphore** to prevent simultaneous writes.  
- **Counting semaphores** to manage full/empty slots in the buffer.  
- Consumer reads from the buffer and calculates current and average prices.  
- Handles multiple producers joining and leaving gracefully.  

---

## Requirements
- Linux environment (Ubuntu recommended)  
- g++ compiler with C++17 support  
- POSIX realtime libraries (`-lrt`)  

---

## File Structure
├── producer.cpp # Producer process implementation
├── consumer.cpp # Consumer process implementation
├── Makefile # To compile producer and consumer
└── README.md # Project description and usage


---

## Compilation
You can compile both programs using the provided `Makefile`:

bash
make all
This will produce two executables:
producer
consumer

To clean the binaries:
make clean

Usage
Producer:
Each producer simulates a commodity and generates price values with a normal distribution.
./producer <name> <mean> <stddev> <time> <buffer_size>
name – Name of the commodity (e.g., GOLD, SILVER)
mean – Mean price
stddev – Standard deviation of price
time – Sleep time in milliseconds between production
buffer_size – Size of the shared buffer
Example:
./producer GOLD 4 0.2 700 4
./producer SILVER 3 0.3 900 4
Multiple producers can run simultaneously. The shared memory and semaphores are created by the first producer and reused by subsequent producers.

Consumer:
The consumer reads items from the shared buffer, updates history and average prices, and displays a table.
./consumer <buffer_size>
Example:
./consumer 4

How it Works
Producer
Generates a price for its commodity using a normal distribution.
Waits on empty semaphore to ensure buffer space is available.
Locks the mutex semaphore to enter the critical section.
Writes the item to the shared buffer and updates buffer indices.
Releases the mutex semaphore and signals the full semaphore.
Sleeps for the specified time before producing the next item.


Consumer
Waits on full semaphore to ensure data is available.
Locks the mutex semaphore to enter the critical section.
Reads an item from the buffer and updates buffer indices.
Releases the mutex semaphore and signals the empty semaphore.
Updates commodity history and average price, displaying the table in real-time.
Shared Memory and Semaphores

Shared memory: Stores memory_block structure and item buffer.

Semaphores:

0 – Mutex semaphore for critical section
1 – Empty slots counting semaphore
2 – Full slots counting semaphore

Cleanup
Producers decrease active_producers on exit.
Last producer to exit removes shared memory and semaphores.
Signals SIGINT, SIGTERM, and SIGTSTP are handled for graceful cleanup.

Example Commodities

GOLD
SILVER
COPPER
ALUMINIUM
COTTON
LEAD
MENTHAOIL
NATURALGAS
NICKEL
ZINC
CRUDEOIL







