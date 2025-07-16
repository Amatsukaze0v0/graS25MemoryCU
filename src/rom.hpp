#include <systemc>
#include <map>
using namespace sc_core;

#ifndef ROM_H
#define ROM_H

SC_MODULE(ROM)
{

    sc_in<bool> clk, wide, read_en{"ROM_enable"};
    sc_in<uint32_t> addr;
    sc_out<bool> ready, error;
    sc_out<uint32_t> data;
    std::map<uint32_t, uint8_t> memory;
    uint32_t latency;

    SC_HAS_PROCESS(ROM);

    // TODO: 此处memory没有设定latency，默认值为 0 Ns;
    /**
     * 创建一个ROM，存在对于size的二次检查
     *
     * @param name 名字
     * @param size 设定的ROM大小
     * @param rom_content 数组指针，指向初始化数组
     *
     */
    ROM(sc_module_name name, uint32_t size, uint32_t *rom_content, uint32_t latency_clk)
        : sc_module(name), ready("rom_ready"), data("rom_data_out")
    {
        if (latency_clk > 0)
        {
            latency = latency_clk;
        }
        else
        {
            latency = 3;
        }
        uint32_t i = 0; // byte index in memory
        // Korpieren die Inhalte auf memory
        while (i < size)
        {
            // ROM 不再负责校验数组，数组空缺的部分由RP负责填充0，因此也不用处理缺失部分
            uint32_t word = rom_content[0];
            for (int k = 0; k < 4; ++k)
            {
                memory[i++] = (word >> (k * 8)) & 0xFF;
            }
            rom_content++;
            printf("ROM writing value: 0x%08x on Address 0x%08x. \n", word, i - 4);
        }

        SC_THREAD(read);
        sensitive << clk.pos();
    }

    /**
     * 返回ROM存储区大小
     *
     * @return int, ROM大小
     */
    int size()
    {
        return memory.size();
    }

    void read()
    {
        while (true)
        {
            wait();
            if (read_en.read())
            {
                ready.write(false);
                error.write(false);

                // latency Simulation
                for(int i =0;i < latency;i++){
                    wait();
                }

                uint32_t addresse = addr.read();
                if (!wide.read())
                {
                    if (memory.count(addresse))
                    {
                        data.write(static_cast<uint32_t>(memory[addresse]));
                        printf("ROM found 1B value: 0x%08x at Address 0x%08x.\n", static_cast<uint32_t>(memory[addresse]), addresse);
                        ready.write(true);
                    }
                    else
                    {
                        SC_REPORT_WARNING("ROM", "Read from unmapped address (byte)");
                        data.write(0xFF);
                        ready.write(true);
                    }
                }
                else
                {
                    if (addresse % 4 != 0)
                    {
                        data.write(0x00);
                        error.write(true);
                        ready.write(true);
                        return;
                    }
                    uint32_t result = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        uint32_t curr_addr = addresse + i;
                        if (memory.count(curr_addr))
                        {
                            result |= static_cast<uint32_t>(memory[curr_addr]) << (8 * i);
                        }
                        else
                        {
                            SC_REPORT_WARNING("ROM", "Read from unmapped address (word)");
                            result |= 0xFF << (8 * i);
                        }
                    }
                    data.write(result);
                    printf("ROM found 4B value: 0x%08x at Address 0x%08x.\n", result, addresse);
                    ready.write(true);
                }
            }
        }
    }

    bool write(uint32_t address, uint8_t data)
    {
        if (memory.count(address))
        {
            memory[address] = data;
            return true;
        }
        return false;
    }
};

#endif
