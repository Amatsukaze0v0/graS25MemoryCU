#include <map>
#include <systemc>

#include "main_memory.hpp"
#include "rom.hpp"
using namespace sc_core;

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

SC_MODULE(MEMORY_CONTROLLER)
{

    // input
    sc_in<bool> clk, r, w, wide, mem_ready;
    sc_in<uint32_t> addr, wdata, mem_rdata;
    sc_in<uint8_t> user;

    // output
    sc_out<uint32_t> rdata, mem_addr, mem_wdata;
    sc_out<bool> ready{"ready_signal_for_CU"}, error{"error"}, mem_r{"memory_read"}, mem_w{"memory_write"};

    // innere Komponenten
    ROM *rom;
    sc_signal<uint32_t> rom_addr_sig, data_cu_rom;
    sc_signal<bool> rom_read_en, rom_wide_sig, ready_cu_rom, rom_error;

    // Adresse und ihrer Benutzer
    std::map<uint32_t, uint8_t> gewalt;

    uint32_t block_size;
    uint32_t rom_size;

    SC_HAS_PROCESS(MEMORY_CONTROLLER);

    MEMORY_CONTROLLER(sc_module_name name, uint32_t rom_size, uint32_t *rom_content, uint32_t latency_rom, uint32_t block_size) : sc_module(name), block_size(block_size), rom_size(rom_size)
    {
        // initialisieren
        // die ROM-Größe soll bereits im Hauptprogramm überprüft werden
        if (rom_content == NULL)
        {
            rom_content = new uint32_t[rom_size / sizeof(uint32_t)]();
        }
        printf("ROM size is: %d Bytes.\n", rom_size);
        rom = new ROM("rom", rom_size, rom_content, latency_rom);
        rom->read_en(rom_read_en);
        rom->clk(clk);
        rom->addr(rom_addr_sig);
        rom->wide(rom_wide_sig);
        rom->ready(ready_cu_rom);
        rom->data(data_cu_rom);
        rom->error(rom_error);

        SC_THREAD(process);
        sensitive << clk.pos();
    }

    void process()
    {
        while (true)
        {
            wait();
            printf("MC job started~ \n");
            // ControlUnit stellt sicher, dass Lese- und Schreiboperationen nicht gleichzeitig auftreten.
            // Zur Robustheit des Programms behalten wir jedoch diese Prüfung bei.
            if (r.read() && w.read())
            {
                SC_REPORT_ERROR("Memory Controller", "Fehler: Gleichzeitiger Lese- und Schreibzugriff ist nicht erlaubt.\n");
                continue;
            }
            if (r.read())
            {
                ready.write(0);
                if (protection())
                {
                    read();
                }
                else
                {
                    error.write(1);
                    ready.write(1);
                    continue;
                }
            }
            if (w.read())
            {
                ready.write(0);
                if (protection())
                {
                    write();
                }
                else
                {
                    error.write(1);
                    ready.write(1);
                    continue;
                }
            }
        }
    }

    void read()
    {
        // read in Rom
        if (addr.read() < rom->size())
        {
            uint32_t address = addr.read();
            // Überprüfung, ob die 4-Byte-ausgerichtete Adresse außerhalb des ROM-Bereichs liegt
            if (wide.read() && rom->size() < 4 || address > rom->size() - 4)
            {
                printf("[MC] Fehler ohne Unterbrechung: Adresse 0x%08X beim ROM-Zugriff liegt außerhalb des gültigen Bereichs bei 4-Byte-Alignment.\n", address);
                error.write(1);
                ready.write(1);
                return;
            }

            printf("[MC] set rom_wide_sig = %d, rom_addr_sig = 0x%08X\n", wide.read(), address);
            rom_wide_sig.write(wide.read());
            rom_addr_sig.write(address);
            rom_read_en.write(1);
            printf("[MC] Warten auf rom_ready.posedge_event() ...\n");
            // Warten auf Rom
            wait(ready_cu_rom.posedge_event());

            if (!rom_error.read())
            {
                printf("[MC] rom_ready eingetroffen, rom_data = 0x%08X\n", data_cu_rom.read());

                rdata.write(data_cu_rom.read());
                rom_read_en.write(0);
                ready.write(1);
                error.write(0);
                printf("[MC] rdata set to 0x%08X, ready=1\n", data_cu_rom.read());
            }
            else
            {
                printf("ERROR : Bei einem 4-Byte-weiten Lesezugriff ist die Adresse 0x%08x nicht 4-Byte aligned.\n", address);
                rdata.write(data_cu_rom.read());
                rom_read_en.write(0);
                error.write(1);
                ready.write(1);
            }
        }
        else
        {
            if (wide.read())
            {
                uint32_t address = addr.read();
                printf("[MC] memory 4B read request: addr=0x%08X, wide=%d\n", address, wide.read());
                mem_addr.write(addr.read());
                mem_r.write(1);
                printf("[MC] Warten auf mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                wait(SC_ZERO_TIME);
                printf("[MC] memory 4B read beendet: addr=0x%08X, mem_rdata=0x%08X\n", address, mem_rdata.read());
                rdata.write(mem_rdata.read());
                ready.write(1);
                error.write(0);
            }
            else
            {
                uint32_t offset = addr.read() % 4;
                uint32_t address = addr.read() - offset;
                printf("[MC] memory 1B read Anfrage: addr=0x%08X, wide=%d\n", addr.read(), wide.read());
                mem_addr.write(address);
                mem_r.write(1);
                printf("[MC] Warten auf mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                wait(SC_ZERO_TIME);
                uint32_t raw_data = mem_rdata.read();
                uint32_t real_data = (raw_data >> (offset * 8)) & 0xFF;
                real_data = real_data >> (4 - offset);
                rdata.write(real_data);
                printf("[MC] memory 1B read beendet: addr=0x%08X, mem_rdata=0x%08X\n", addr.read(), real_data);
                ready.write(1);
                error.write(0);
            }
        }
    }
    void write()
    {
        if (addr.read() >= rom->size())
        {
            uint32_t new_data;
            if (!wide.read())
            {
                // Bei 1-Byte-Alignment der Adresse muss das Datenfeld zuerst gelesen und erweitert werden.
                printf("[MC] memory write Anfrage (1B): addr=0x%08X, wdata=0x%02X, user=%u\n", addr.read(), wdata.read() & 0xFF, user.read());
                mem_addr.write(addr);
                mem_r.write(1);
                printf("[MC] Warten auf mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                // Steuerung wurde noch nicht an die Control Unit zurückgegeben – Lesesignal muss zurückgesetzt werden, um Konflikte zu vermeiden.
                mem_r.write(0);
                uint32_t prev_data = mem_rdata.read();
                printf("[MC] Rohdaten an Adresse 0x%08x mit Wert 0x%08x erhalten.\n", addr.read(), prev_data);
                uint32_t low_8bits = wdata.read();
                uint8_t offset = addr.read() % 4;
                uint32_t mask = ~(0xFF << (offset * 8));
                uint32_t cleared = prev_data & mask;
                uint32_t inserted = low_8bits << (offset * 8);
                new_data = cleared | inserted;

                printf("[MC] Neuer Datenwert: 0x%08x\n", new_data);
            }
            else
            {
                printf("[MC] memory write Anfrage (4B): addr=0x%08X, wdata=0x%08X, user=%u\n", addr.read(), wdata.read(), user.read());
                new_data = wdata.read();
            }
            mem_addr.write(addr);
            mem_wdata.write(new_data);
            mem_w.write(1);
            printf("[MC] Warten auf mem_ready.posedge_event() ...\n");
            do
            {
                wait();
            } while (!mem_ready.read());
            mem_w.write(0);
            printf("[MC] memory write beendet: addr=0x%08X, wdata=0x%08X\n", addr.read(), new_data);
            ready.write(1);
            error.write(0);
        }
        else
        {
            printf("Die Adresse 0x%08X liegt in ROM und darf nicht verändert werden.\n", addr.read());
            error.write(1);
            ready.write(1);
            wait(SC_ZERO_TIME);
        }
    }

    bool protection()
    {
        uint8_t benutzer = user.read();
        uint32_t adresse = addr.read();

        if (adresse < rom->size())
        {
            if (w.read())
            {
                printf("Fehler: Schreibzugriff auf ROM-Adresse 0x%08X ist verboten.\n", adresse);
                return false;
            }
            // Jeder darf ROM lesen
        }
        else
        {
            // Berechnung der Startadresse des zugehörigen Blocks
            uint32_t block_addr = (adresse - rom_size) / block_size;

            // Der Superuser hat immer alle Berechtigungen.
            if (benutzer == 0)
            {
                return true;
            }
            else if (benutzer == 255)
            {
                gewalt.erase(block_addr);
                return true;
            }

            auto it = gewalt.find(block_addr);
            if (it == gewalt.end())
            {
                // Dieser Block hat noch keinen Besitzer – jeder darf darauf zugreifen.
                if (r.read())
                {
                    printf("ACHTUNG : Block 0x%08X wird noch nicht geschrieben.\n", block_addr);
                    return true;
                }
                gewalt[block_addr] = benutzer;
                printf("Block 0x%08X wurde User %u zugeteilt.\n", block_addr, benutzer);
                return true;
            }

            if (it->second != benutzer)
            {
                printf("User %u hat keine Berechtigung auf Block 0x%08X (Adresse 0x%08X).\n", benutzer, block_addr, adresse);
                return false;
            }
        }
        return true;
    }

    void setRomAt(uint32_t address, uint8_t data)
    {
        if (!rom->write(addr, data))
        {
            SC_REPORT_WARNING("ROM", "Schreibzugriff auf nicht zugewiesene Adresse (Byte)");
        }
    }

    uint8_t getOwner(uint32_t addr)
    {
        uint32_t block_addr = (addr - rom_size) * block_size;
        auto it = gewalt.find(block_addr);
        return it != gewalt.end() ? it->second : 255;
    }
};

#endif // MEMORY_CONTROLLER_H
