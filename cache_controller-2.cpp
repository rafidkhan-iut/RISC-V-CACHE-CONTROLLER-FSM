/*
 * Cache Controller FSM Simulation
 * Supports both Write-Back and Write-Through policies
 * Based on Patterson & Hennessy "Computer Organization and Design"
 * Output: all addresses and values printed in decimal
 */

#include <bits/stdc++.h>
using namespace std;

// ─── Configuration ────────────────────────────────────────────────────────────
static const int CACHE_LINES   = 8;   // Number of cache lines
static const int MEMORY_CYCLES = 4;   // Cycles to read/write a block from memory
static const int BLOCK_SIZE    = 4;   // Words per cache block

// ─── Enumerations ─────────────────────────────────────────────────────────────
enum class WritePolicy { WRITE_BACK, WRITE_THROUGH };
enum class ReqType     { READ, WRITE };

// ── Write-Back FSM States ─────────────────────────────────────────────────────
enum class WBState { IDLE, TAG_CHECK, WRITE_BACK, ALLOCATE, WRITE_CACHE, COMPLETE };

// ── Write-Through FSM States ──────────────────────────────────────────────────
enum class WTState { IDLE, TAG_CHECK, ALLOCATE, WRITE_MEM, WRITE_CACHE, COMPLETE };

string wbStr(WBState s) {
    switch(s) {
        case WBState::IDLE:        return "IDLE";
        case WBState::TAG_CHECK:   return "TAG_CHECK";
        case WBState::WRITE_BACK:  return "WRITE_BACK";
        case WBState::ALLOCATE:    return "ALLOCATE";
        case WBState::WRITE_CACHE: return "WRITE_CACHE";
        case WBState::COMPLETE:    return "COMPLETE";
    }
    return "?";
}
string wtStr(WTState s) {
    switch(s) {
        case WTState::IDLE:        return "IDLE";
        case WTState::TAG_CHECK:   return "TAG_CHECK";
        case WTState::ALLOCATE:    return "ALLOCATE";
        case WTState::WRITE_MEM:   return "WRITE_MEM";
        case WTState::WRITE_CACHE: return "WRITE_CACHE";
        case WTState::COMPLETE:    return "COMPLETE";
    }
    return "?";
}

// ─── Cache Line ───────────────────────────────────────────────────────────────
struct CacheLine {
    bool valid = false;
    bool dirty = false;
    int  tag   = -1;
    int  data[BLOCK_SIZE];
    CacheLine() { fill(data, data + BLOCK_SIZE, 0); }
};

// ─── Simple Memory ────────────────────────────────────────────────────────────
struct Memory {
    map<int,int> store;
    int  read (int addr)          { return store.count(addr) ? store[addr] : 0; }
    void write(int addr, int val) { store[addr] = val; }
};

// ─── CPU Request ──────────────────────────────────────────────────────────────
struct CPURequest {
    ReqType type;
    int     address;
    int     writeData;
};

string reqStr(const CPURequest& r) {
    if (r.type == ReqType::READ)
        return "READ  addr=" + to_string(r.address);
    return "WRITE addr=" + to_string(r.address) + " data=" + to_string(r.writeData);
}

// ─── Output Signals ───────────────────────────────────────────────────────────
struct Signals {
    bool mem_read  = false;
    bool mem_write = false;
    bool cache_hit = false;
    bool cpu_stall = false;
    bool done      = false;
    int  read_data = 0;
};

// ─── Address helpers (all decimal) ───────────────────────────────────────────
int getIndex (int addr) { return (addr / BLOCK_SIZE) % CACHE_LINES; }
int getTag   (int addr) { return  addr / (BLOCK_SIZE * CACHE_LINES); }
int getOffset(int addr) { return  addr % BLOCK_SIZE; }
int blockBase(int addr) { return (addr / BLOCK_SIZE) * BLOCK_SIZE; }

// ─── Write-Back Cache Controller ─────────────────────────────────────────────
class WBCacheController {
    CacheLine cache[CACHE_LINES];
    Memory&   mem;
    int hits = 0, misses = 0, writebacks = 0, totalCycles = 0;

public:
    WBCacheController(Memory& m) : mem(m) {}

    Signals process(const CPURequest& req, ostream& log) {
        Signals sig;
        WBState state = WBState::IDLE;
        int cycle = 0;

        int index  = getIndex (req.address);
        int tag    = getTag   (req.address);
        int offset = getOffset(req.address);
        CacheLine& line = cache[index];

        log << "\n  [WB-FSM] " << reqStr(req) << "\n";
        log << "           index=" << index
            << "  tag="    << tag
            << "  offset=" << offset << "\n";

        // IDLE
        log << "  Cycle " << ++cycle << ": " << wbStr(state);
        state = WBState::TAG_CHECK;
        log << " -> " << wbStr(state) << "  (cpu_stall=0)\n";

        // TAG_CHECK
        log << "  Cycle " << ++cycle << ": " << wbStr(state);
        bool hit = line.valid && (line.tag == tag);
        sig.cache_hit = hit;

        if (hit) {
            hits++;
            if (req.type == ReqType::READ) {
                sig.read_data = line.data[offset];
                state = WBState::COMPLETE;
                log << " -> " << wbStr(state)
                    << "  (HIT, read_data=" << sig.read_data << ")\n";
            } else {
                line.data[offset] = req.writeData;
                line.dirty = true;
                state = WBState::COMPLETE;
                log << " -> " << wbStr(state)
                    << "  (HIT, wrote " << req.writeData << " to cache, dirty=1)\n";
            }
        } else {
            misses++;
            sig.cpu_stall = true;

            if (line.valid && line.dirty) {
                // WRITE_BACK
                state = WBState::WRITE_BACK;
                log << " -> " << wbStr(state) << "  (MISS, dirty eviction needed)\n";
                int evictBase = (line.tag * CACHE_LINES + index) * BLOCK_SIZE;
                log << "  Cycles " << cycle+1 << "-" << cycle+MEMORY_CYCLES
                    << ": " << wbStr(state);
                for (int i = 0; i < BLOCK_SIZE; i++)
                    mem.write(evictBase + i, line.data[i]);
                writebacks++;
                cycle += MEMORY_CYCLES;
                sig.mem_write = true;
                state = WBState::ALLOCATE;
                log << " -> " << wbStr(state)
                    << "  (wrote evicted block to memory, base addr=" << evictBase << ")\n";
            } else {
                state = WBState::ALLOCATE;
                log << " -> " << wbStr(state) << "  (MISS, no dirty eviction)\n";
            }

            // ALLOCATE
            int base = blockBase(req.address);
            log << "  Cycles " << cycle+1 << "-" << cycle+MEMORY_CYCLES
                << ": " << wbStr(state);
            for (int i = 0; i < BLOCK_SIZE; i++)
                line.data[i] = mem.read(base + i);
            line.valid = true;
            line.tag   = tag;
            line.dirty = false;
            cycle += MEMORY_CYCLES;
            sig.mem_read = true;
            state = WBState::WRITE_CACHE;
            log << " -> " << wbStr(state)
                << "  (fetched block from memory, base addr=" << base << ")\n";

            // WRITE_CACHE
            log << "  Cycle " << ++cycle << ": " << wbStr(state);
            if (req.type == ReqType::WRITE) {
                line.data[offset] = req.writeData;
                line.dirty = true;
                state = WBState::COMPLETE;
                log << " -> " << wbStr(state)
                    << "  (wrote " << req.writeData << " to cache, dirty=1)\n";
            } else {
                sig.read_data = line.data[offset];
                state = WBState::COMPLETE;
                log << " -> " << wbStr(state)
                    << "  (read_data=" << sig.read_data << ")\n";
            }
        }

        // COMPLETE
        log << "  Cycle " << ++cycle << ": " << wbStr(state)
            << "  (done=1, cpu_stall=0)\n";
        sig.done     = true;
        sig.cpu_stall = false;
        totalCycles += cycle;
        return sig;
    }

    void printStats(ostream& out) const {
        int total = hits + misses;
        out << "\n  [WB Stats]  hits=" << hits
            << "  misses=" << misses
            << "  writebacks=" << writebacks
            << "  hit_rate=" << fixed << setprecision(1)
            << (total > 0 ? 100.0*hits/total : 0.0) << "%"
            << "  total_cycles=" << totalCycles << "\n";
    }
};

// ─── Write-Through Cache Controller ──────────────────────────────────────────
class WTCacheController {
    CacheLine cache[CACHE_LINES];
    Memory&   mem;
    int hits = 0, misses = 0, totalCycles = 0;

public:
    WTCacheController(Memory& m) : mem(m) {}

    Signals process(const CPURequest& req, ostream& log) {
        Signals sig;
        WTState state = WTState::IDLE;
        int cycle = 0;

        int index  = getIndex (req.address);
        int tag    = getTag   (req.address);
        int offset = getOffset(req.address);
        CacheLine& line = cache[index];

        log << "\n  [WT-FSM] " << reqStr(req) << "\n";
        log << "           index=" << index
            << "  tag="    << tag
            << "  offset=" << offset << "\n";

        // IDLE
        log << "  Cycle " << ++cycle << ": " << wtStr(state);
        state = WTState::TAG_CHECK;
        log << " -> " << wtStr(state) << "\n";

        // TAG_CHECK
        log << "  Cycle " << ++cycle << ": " << wtStr(state);
        bool hit = line.valid && (line.tag == tag);
        sig.cache_hit = hit;

        if (hit) {
            hits++;
            if (req.type == ReqType::READ) {
                sig.read_data = line.data[offset];
                state = WTState::COMPLETE;
                log << " -> " << wtStr(state)
                    << "  (HIT, read_data=" << sig.read_data << ")\n";
            } else {
                line.data[offset] = req.writeData;
                mem.write(req.address, req.writeData);
                state = WTState::WRITE_MEM;
                log << " -> " << wtStr(state)
                    << "  (HIT write: updating cache & memory)\n";
                log << "  Cycles " << cycle+1 << "-" << cycle+MEMORY_CYCLES
                    << ": " << wtStr(state);
                cycle += MEMORY_CYCLES;
                sig.mem_write = true;
                state = WTState::COMPLETE;
                log << " -> " << wtStr(state) << "\n";
            }
        } else {
            misses++;
            sig.cpu_stall = true;

            if (req.type == ReqType::READ) {
                // ALLOCATE
                state = WTState::ALLOCATE;
                log << " -> " << wtStr(state) << "  (READ MISS)\n";
                int base = blockBase(req.address);
                log << "  Cycles " << cycle+1 << "-" << cycle+MEMORY_CYCLES
                    << ": " << wtStr(state);
                for (int i = 0; i < BLOCK_SIZE; i++)
                    line.data[i] = mem.read(base + i);
                line.valid = true;
                line.tag   = tag;
                line.dirty = false;
                cycle += MEMORY_CYCLES;
                sig.mem_read = true;
                state = WTState::WRITE_CACHE;
                log << " -> " << wtStr(state)
                    << "  (fetched block from memory, base addr=" << base << ")\n";

                // WRITE_CACHE
                log << "  Cycle " << ++cycle << ": " << wtStr(state);
                sig.read_data = line.data[offset];
                state = WTState::COMPLETE;
                log << " -> " << wtStr(state)
                    << "  (read_data=" << sig.read_data << ")\n";
            } else {
                // WRITE_MEM (no-allocate)
                state = WTState::WRITE_MEM;
                log << " -> " << wtStr(state) << "  (WRITE MISS, no-allocate)\n";
                log << "  Cycles " << cycle+1 << "-" << cycle+MEMORY_CYCLES
                    << ": " << wtStr(state);
                mem.write(req.address, req.writeData);
                cycle += MEMORY_CYCLES;
                sig.mem_write = true;
                state = WTState::COMPLETE;
                log << " -> " << wtStr(state)
                    << "  (wrote " << req.writeData << " directly to memory)\n";
            }
        }

        // COMPLETE
        log << "  Cycle " << ++cycle << ": " << wtStr(state) << "  (done=1)\n";
        sig.done      = true;
        sig.cpu_stall = false;
        totalCycles  += cycle;
        return sig;
    }

    void printStats(ostream& out) const {
        int total = hits + misses;
        out << "\n  [WT Stats]  hits=" << hits
            << "  misses=" << misses
            << "  hit_rate=" << fixed << setprecision(1)
            << (total > 0 ? 100.0*hits/total : 0.0) << "%"
            << "  total_cycles=" << totalCycles << "\n";
    }
};

// ─── Run one scenario ────────────────────────────────────────────────────────
void runScenario(const string& name,
                 const vector<CPURequest>& requests,
                 WritePolicy policy,
                 ostream& out)
{
    out << "\n================================================================\n";
    out << "  Scenario : " << name << "\n";
    out << "  Policy   : "
        << (policy == WritePolicy::WRITE_BACK ? "Write-Back" : "Write-Through") << "\n";
    out << "================================================================\n";

    Memory mem;
    for (int i = 0; i < 64; i++) mem.write(i, i * 10); // addr i -> value i*10

    if (policy == WritePolicy::WRITE_BACK) {
        WBCacheController ctrl(mem);
        for (const auto& r : requests) {
            Signals s = ctrl.process(r, out);
            out << "  => " << (s.cache_hit ? "HIT " : "MISS")
                << "  result="    << s.read_data
                << "  mem_read="  << s.mem_read
                << "  mem_write=" << s.mem_write << "\n";
        }
        ctrl.printStats(out);
    } else {
        WTCacheController ctrl(mem);
        for (const auto& r : requests) {
            Signals s = ctrl.process(r, out);
            out << "  => " << (s.cache_hit ? "HIT " : "MISS")
                << "  result="    << s.read_data
                << "  mem_read="  << s.mem_read
                << "  mem_write=" << s.mem_write << "\n";
        }
        ctrl.printStats(out);
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    ofstream fout("simulation_output.txt");

    auto both = [&](const string& name, const vector<CPURequest>& reqs) {
        runScenario(name, reqs, WritePolicy::WRITE_BACK,    cout);
        runScenario(name, reqs, WritePolicy::WRITE_THROUGH, cout);
        runScenario(name, reqs, WritePolicy::WRITE_BACK,    fout);
        runScenario(name, reqs, WritePolicy::WRITE_THROUGH, fout);
    };

    // S1: Cold-start read misses — all addresses map to different indices
    both("S1: Cold-Start Read Misses", {
        {ReqType::READ,  0, 0},
        {ReqType::READ,  4, 0},
        {ReqType::READ,  8, 0},
    });

    // S2: Temporal locality — first miss, then same address and block offset hit
    both("S2: Temporal Locality (Read Hit after Miss)", {
        {ReqType::READ, 0, 0},
        {ReqType::READ, 0, 0},   // hit
        {ReqType::READ, 1, 0},   // same block, offset 1 -> hit
    });

    // S3: Write then read — WB allocates on write miss; WT uses no-allocate
    both("S3: Write then Read Same Address", {
        {ReqType::WRITE, 2, 999},
        {ReqType::READ,  2,   0},
    });

    // S4: Dirty eviction — addr=32 aliases index=0 (same as addr=0) but tag=1
    //   index(0)  = (0/4)%8  = 0, tag(0)  = 0/32 = 0
    //   index(32) = (32/4)%8 = 0, tag(32) = 32/32 = 1  <- conflict -> eviction
    both("S4: Dirty Block Eviction", {
        {ReqType::WRITE,  0, 100},   // index=0, tag=0, dirty
        {ReqType::WRITE,  4, 200},   // index=1, tag=0, dirty
        {ReqType::READ,  32,   0},   // index=0, tag=1 -> evicts dirty line at index 0
    });

    // S5: Write-miss allocation — WB read hits; WT read misses
    both("S5: Write Miss Allocation Behaviour", {
        {ReqType::WRITE, 16, 777},
        {ReqType::READ,  16,   0},
    });

    // S6: Mixed workload — all addresses 0-3 share block base=0, index=0, tag=0
    both("S6: Mixed Read/Write Workload", {
        {ReqType::READ,   0,   0},
        {ReqType::WRITE,  1, 555},
        {ReqType::READ,   2,   0},
        {ReqType::WRITE,  3, 333},
        {ReqType::READ,   1,   0},
        {ReqType::READ,   3,   0},
    });

    cout << "\nSimulation complete. Full log saved to simulation_output.txt\n";
    return 0;
}
