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

Client / Producer
│
▼
SPSC Ring Buffer (lock-free)
│
▼
Matching Loop (single-threaded core)
│
▼
Matching Engine
│
▼
OrderBook (per symbol)

------------------------------------------------------------------------

## ⚙️ Matching Rules

### Price-Time Priority

1.  Better price first
2.  Same price → earlier order first (FIFO)

------------------------------------------------------------------------

## 🔄 Order Lifecycle

New → Ack → (PartialFill)\* → Fill
↓
Cancel
↓
CancelAck

------------------------------------------------------------------------

## 🧪 Benchmark

Example:

levels throughput(cmd/s) avg_ns/cmd
1 4.1M 242 ns
4 8.4M 117 ns
16 8.5M 117 ns

------------------------------------------------------------------------

## ▶️ Build & Run

### Build

mkdir build && cd build
cmake ..
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

