# Match Engine (C++ Low-Latency Order Matching Engine)

## 🚀 Overview

This project is a **high-performance, low-latency order matching
engine** written in modern C++, designed to simulate the core matching
logic used in electronic trading systems.

It focuses on:

-   Deterministic matching (price-time priority)
-   Lock-free message passing
-   Cache-friendly data structures
-   Clear separation between matching logic and execution loop

This is NOT a toy project.
It is designed to reflect production-grade matching engine architecture.

------------------------------------------------------------------------

## ✨ Features

### Core Matching Features

-   Multi-symbol support
-   Limit Order / Market Order
-   Price-Time Priority (FIFO within price level)
-   Partial Fill / Full Fill
-   Cross price levels matching
-   Order lifecycle management

### Order Operations

-   New Order
-   Cancel Order
-   Replace Order (Cancel + New)

------------------------------------------------------------------------

## 🧠 Architecture

Client / Producer\
│\
▼\
SPSC Ring Buffer (lock-free)\
│\
▼\
Matching Loop (single-threaded core)\
│\
▼\
Matching Engine\
│\
▼\
OrderBook (per symbol)

------------------------------------------------------------------------

## ⚙️ Matching Rules

### Price-Time Priority

1.  Better price first
2.  Same price → earlier order first (FIFO)

------------------------------------------------------------------------

## 🔄 Order Lifecycle

New → Ack → (PartialFill)\* → Fill\
↓\
Cancel\
↓\
CancelAck

------------------------------------------------------------------------

## 🧪 Benchmark
```
================ Insert-Only Level Sweep Summary ================
levels    throughput(cmd/s)   avg_ns/cmd        idle_spins    resting_orders    book_levels
1         4116977.05          242.90            217           1000000           1                                                                                                                                                                                     
2         6572648.31          152.15            164           1000000           2                                                                                                                                                                                     
4         8479608.99          117.93            339           1000000           4                                                                                                                                                                                     
8         8443021.27          118.44            941           1000000           8                                                                                                                                                                                     
16        8492475.03          117.75            2             1000000           16                                                                                                                                                                                    
32        7698400.44          129.90            446           1000000           32                                                                                                                                                                                    
64        8300743.58          120.47            1             1000000           64                                                                                                                                                                                    
128       7282184.90          137.32            1154          1000000           128                                                                                                                                                                                   
===============================================================

=== Benchmark: AlternatingMatch ===
submitted_commands       : 1000000
processed_commands       : 1000000
elapsed_seconds          : 0.131986
throughput_cmd_per_s     : 7576565.51
avg_ns_per_command       : 131.99
new_orders               : 1000000
acks                     : 1000000
rejects                  : 0
partial_fills            : 0
fills                    : 1000000
cancel_acks              : 0
cancel_rejects           : 0
replace_acks             : 0
replace_rejects          : 0
trades                   : 500000
idle_spins               : 1598
final_resting_orders     : 0
final_price_levels       : 0

=== Benchmark: SweepBook ===
preload_orders           : 65536
aggressive_orders        : 100000
aggressive_qty           : 16
preload_levels_config    : 64
submitted_commands       : 165536
processed_commands       : 165536
elapsed_seconds          : 0.022995
throughput_cmd_per_s     : 7198892.86
avg_ns_per_command       : 138.91
new_orders               : 165536
acks                     : 165536
rejects                  : 0
partial_fills            : 61440
fills                    : 69632
cancel_acks              : 0
cancel_rejects           : 0
replace_acks             : 0
replace_rejects          : 0
trades                   : 65536
idle_spins               : 165
final_resting_orders     : 95904
final_price_levels       : 1
```


------------------------------------------------------------------------

## ▶️ Build & Run

### Build

mkdir build && cd build\
cmake ..\
make -j

### Run

./demo_matching_loop
./benchmark_driver

------------------------------------------------------------------------

## 💡 Design Highlights

-   Single-threaded matching core (no locks)
-   Lock-free SPSC queue
-   Cache-friendly layout
-   Replace = Cancel + New

------------------------------------------------------------------------

## 🎯 Why This Project Matters

This project demonstrates:

-   Real-world trading system design
-   Low-latency C++ engineering
-   Deep understanding of memory ordering and CPU cache

------------------------------------------------------------------------

## 👤 Author

Chendong Pang
