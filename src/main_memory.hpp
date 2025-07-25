#ifndef MAIN_MEMORY_HPP
#define MAIN_MEMORY_HPP

#include <systemc>
#include <map>
using namespace sc_core;

//Dieses Modul basiert größtenteils auf dem Code aus der Übungsaufgabe.

SC_MODULE(MAIN_MEMORY)
{
  sc_in<bool> clk;

  sc_in<uint32_t> addr;
  sc_in<uint32_t> wdata;
  sc_in<bool> r;
  sc_in<bool> w;

  sc_out<uint32_t> rdata;
  sc_out<bool> ready{"ready_in_Mem"};

  std::map<uint32_t, uint32_t> memory;
  uint32_t latency;

  SC_HAS_PROCESS(MAIN_MEMORY);

  MAIN_MEMORY(sc_module_name name, uint32_t latency_clk) : sc_module(name)
  {
    if (latency_clk > 0)
    {
      latency = latency_clk;
    }
    else
    {
      latency = 3;
    }
    SC_THREAD(behaviour);
    sensitive << clk.pos();
  }

  void behaviour()
  {
    while (true)
    {
      wait();

      if (r.read())
      {
        doRead(w.read());
      }
      if (w.read())
      {
        doWrite();
      }
    }
  }

  void doRead(bool dontSetReady)
  {
    ready.write(false);

    uint32_t result = get(addr.read());

    for (int i = 0; i < latency; i++)
    {
      wait();
    }

    rdata.write(result);
    if (!dontSetReady)
    {
      ready.write(true);
    }
  }

  void doWrite()
  {
    ready.write(false);
    set(addr.read(), wdata.read());

    for (int i = 0; i < latency; i++)
    {
      wait();
    }

    ready.write(true);
  }

  uint32_t get(uint32_t address)
  {
    uint32_t result = 0;

    for (int i = 0; i < 4; i++)
    {
      uint8_t value = 0;
      if (memory.find(address + i) != memory.end())
      {
        value = memory[address + i];
      }
      result |= value << (i * 8);
    }
    printf("[MEM] Wert aus dem Speicher gelesen: 0x%08x an Adresse 0x%08x.\n", result, address);
    return result;
  }

  void set(uint32_t address, uint32_t value)
  {
    for (int i = 0; i < 4; i++)
    {
      memory[address + i] = (value >> (i * 8)) & 0xFF;
      if (address + i == UINT32_MAX)
      {
        break;
      }
    }
    printf("[MEM] Wert in den Speicher geschrieben: 0x%08x an Adresse 0x%08x.\n", value, address);
  }
};

#endif // MAIN_MEMORY_HPP