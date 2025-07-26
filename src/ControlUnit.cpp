#include <systemc.h>

#include "rahmenprogramm.h"
#include "memory_controller.hpp"

struct Result run_simulation(
    uint32_t cycles,
    const char *tracefile,
    uint32_t latencyRom,
    uint32_t romSize,
    uint32_t blockSize,
    uint32_t *romContent,
    uint32_t numRequests,
    struct Request *requests)
{
    struct Result result = {0, 0};

    sc_time period(10, SC_NS);

    sc_clock clk("clk", period);
    sc_signal<uint32_t> addr, wdata, mem_rdata, rdata, mem_addr, mem_wdata;
    sc_signal<bool> r, w, wide, mem_ready, ready, error, mem_r, mem_w;
    sc_signal<uint8_t> user;

    MEMORY_CONTROLLER *memory_controller = new MEMORY_CONTROLLER("memory_controller", romSize, romContent, latencyRom, blockSize);
    MAIN_MEMORY *memory = new MAIN_MEMORY("Main_Memory", 3);

    memory_controller->clk(clk);
    memory_controller->addr(addr);
    memory_controller->wdata(wdata);
    memory_controller->mem_rdata(mem_rdata);
    memory_controller->rdata(rdata);
    memory_controller->mem_addr(mem_addr);
    memory_controller->mem_wdata(mem_wdata);
    memory_controller->r(r);
    memory_controller->w(w);
    memory_controller->wide(wide);
    memory_controller->mem_ready(mem_ready);
    memory_controller->ready(ready);
    memory_controller->error(error);
    memory_controller->mem_r(mem_r);
    memory_controller->mem_w(mem_w);
    memory_controller->user(user);

    memory->clk(clk);
    memory->rdata(mem_rdata);
    memory->addr(mem_addr);
    memory->wdata(mem_wdata);
    memory->ready(mem_ready);
    memory->r(mem_r);
    memory->w(mem_w);

    uint32_t total_cycles = 0;
    uint32_t error_count = 0;

    sc_trace_file *tf = nullptr;
    if (tracefile != nullptr && strlen(tracefile) > 0)
    {
        std::string tfname(tracefile);
        if (tfname.size() >= 4 && tfname.substr(tfname.size() - 4) == ".vcd")
        {
            tfname = tfname.substr(0, tfname.size() - 4); // 去除 ".vcd"
        }

        tf = sc_create_vcd_trace_file(tfname.c_str());

        // Alle Signale anbinden
        sc_trace(tf, clk, "clk");

        sc_trace(tf, addr, "addr");
        sc_trace(tf, wdata, "wdata");
        sc_trace(tf, rdata, "rdata");
        sc_trace(tf, mem_rdata, "mem_rdata");
        sc_trace(tf, mem_addr, "mem_addr");
        sc_trace(tf, mem_wdata, "mem_wdata");

        sc_trace(tf, r, "r");
        sc_trace(tf, w, "w");
        sc_trace(tf, wide, "wide");
        sc_trace(tf, mem_ready, "mem_ready");
        sc_trace(tf, ready, "ready");
        sc_trace(tf, error, "error");
        sc_trace(tf, mem_r, "mem_r");
        sc_trace(tf, mem_w, "mem_w");

        sc_trace(tf, user, "user");

        sc_trace(tf, memory->ready, "Memory_ready_signal");
        sc_trace(tf, memory_controller->ready_cu_rom, "rom_ready");

        std::cout << "[TRACE] Tracing enabled: " << tfname << ".vcd\n";
    }
    for (std::size_t i = 0; i < numRequests; ++i)
    {
        const Request &req = requests[i];

        // Eingangssignale setzen
        addr.write(req.addr);
        wdata.write(req.data);
        w.write(req.w);
        r.write(!req.w); // Annahme: wenn nicht Schreiben, dann Lesen
        wide.write(req.wide);
        user.write(req.user);

        // Debug Informationen
        std::cout << "[" << sc_time_stamp() << "] "
                  << (req.w ? "WRITE" : "READ") << " request: "
                  << "addr=0x" << std::hex << req.addr
                  << ", data=0x" << req.data
                  << ", user=" << std::dec << (int)req.user
                  << ", wide=" << (int)req.wide
                  << std::endl;

        // Simulation für einen Taktzyklus starten
        sc_start(period); // Ein Taktzyklus: Signale an das Modul übergeben
        total_cycles++;

        if (total_cycles == cycles)
        {
            std::cerr << "Fehler: Unzureichende Taktzyklen, Befehl nicht vollständig ausgeführt." << std::endl;
            goto cycle_deficit;
        }

        while (!ready.read())
        {
            sc_start(period); // Auf Modulantwort warten
            total_cycles++;
            if (total_cycles == cycles)
            {
                std::cerr << "Fehler: Unzureichende Taktzyklen, Befehl nicht vollständig ausgeführt." << std::endl;
                goto cycle_deficit;
            }
        }

        if (error.read())
        {
            std::cerr << " --> FEHLER: Modul hat einen Fehler bei der Anfrage gemeldet " << i << std::endl;
            error_count++;
        }

        // reset
        addr.write(0);
        wdata.write(0);
        mem_rdata.write(0);
        rdata.write(0);
        mem_addr.write(0);
        mem_wdata.write(0);

        r.write(false);
        w.write(false);
        wide.write(false);
        mem_ready.write(false);
        ready.write(false);
        error.write(false);
        mem_r.write(false);
        mem_w.write(false);

        user.write(0);
    }

    // Die verbleibenden Taktzyklen ausführen
    for (int i = total_cycles; i < cycles; i++)
    {
        sc_start(period);
    }

cycle_deficit:

    if (tf != nullptr)
    {
        sc_close_vcd_trace_file(tf);
    }

    result.cycles = total_cycles;
    result.errors = error_count;

    return result;
}

int sc_main(int argc, char *argv[])
{
    std::cout << "ERROR" << std::endl;
    return 1;
}