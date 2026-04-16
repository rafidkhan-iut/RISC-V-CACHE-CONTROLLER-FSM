# Cache Controller FSM Simulation

A C++ simulation of a Finite State Machine (FSM) based simple cache controller, supporting both **Write-Back** and **Write-Through** write policies. Based on the cache controller model from Patterson & Hennessy *Computer Organization and Design*.

---

## Features

- **Write-Back FSM** — States: IDLE → TAG_CHECK → WRITE_BACK (dirty eviction only) → ALLOCATE → WRITE_CACHE → COMPLETE
- **Write-Through FSM** — States: IDLE → TAG_CHECK → ALLOCATE / WRITE_MEM → WRITE_CACHE → COMPLETE
- Direct-mapped cache
- All output (addresses, indices, tags, data values) printed in **decimal**
- Simulated memory pre-populated with `address × 10`
- Per-request cycle-accurate state-transition log
- Hit/miss/writeback statistics per scenario
- 6 test scenarios covering every FSM path

---

## Requirements

- C++17 compatible compiler (`g++`, `clang++`)
- No external dependencies

---

## Build

```bash
g++ -std=c++17 -O2 -o cache_sim src/cache_controller.cpp
```

---

## Run

```bash
./cache_sim
```

Output is printed to **stdout** and also saved to `simulation_output.txt`.

---

## Configuration

All parameters are defined as constants at the top of `cache_controller.cpp`:

| Constant        | Default | Description                           |
|-----------------|---------|---------------------------------------|
| `CACHE_LINES`   | 8       | Number of direct-mapped cache lines   |
| `BLOCK_SIZE`    | 4       | Words (integers) per cache block      |
| `MEMORY_CYCLES` | 4       | Cycles to read or write one block     |

---

## Address Decomposition

Each address is split into three fields (all computed and displayed in decimal):

| Field        | Formula                                |
|--------------|----------------------------------------|
| Block Offset | `address % BLOCK_SIZE`                 |
| Cache Index  | `(address / BLOCK_SIZE) % CACHE_LINES` |
| Tag          | `address / (BLOCK_SIZE x CACHE_LINES)` |

---

## Test Scenarios

| #  | Name                      | What it tests                                                   |
|----|---------------------------|-----------------------------------------------------------------|
| S1 | Cold-Start Read Misses    | All misses; full ALLOCATE path for each request                 |
| S2 | Temporal Locality         | Miss then hits on same address and block offset                 |
| S3 | Write then Read           | WB: allocate-on-write -> read hit; WT: no-allocate -> read miss |
| S4 | Dirty Block Eviction      | Conflict miss triggers WRITE_BACK state in WB                   |
| S5 | Write Miss Allocation     | Confirms WB read hits after write; WT read misses               |
| S6 | Mixed Read/Write Workload | Realistic sequence; highlights WT memory traffic cost           |

---

## Adding Custom Scenarios

In `main()`, call `both()` with a name and a `vector<CPURequest>`:

```cpp
both("My Scenario", {
    {ReqType::READ,   0,   0},   // READ  address 0
    {ReqType::WRITE,  1, 123},   // WRITE address 1, data=123
    {ReqType::READ,   1,   0},   // READ  address 1
});
```

---

## Project Structure

```
cache_fsm/
├── src/
│   └── cache_controller.cpp   # Complete simulation (single file, no dependencies)
├── simulation_output.txt      # Pre-generated output from all 6 scenarios
└── README.md
```
