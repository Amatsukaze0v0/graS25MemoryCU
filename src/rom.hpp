#include<systemc>
#include"Memory.cpp"
using namespace sc_core;


#ifndef ROM_H
#define ROM_H

SC_MODULE(ROM) {

    sc_in<bool> wide;
    sc_in<uint32_t> addr;
    sc_out<bool> ready;
    sc_out<uint32_t> data;
    std::map<uint32_t, uint8_t> memory;
    uint32_t latency;
    //事件触发规则参考https://www.learnsystemc.com/basic/event
    sc_event start_read;

    // TODO: 此处memory没有设定latency，默认值为 0 Ns; 另外，初始化在哪？
    /**
     * 创建一个ROM，存在对于size的二次检查
     * 
     * @param name 名字
     * @param size 设定的ROM大小
     * @param rom_content 数组指针，指向初始化数组
     * 
     */
    ROM(sc_module_name name,uint32_t size, uint32_t* rom_content, uint32_t latency):sc_module(name),latency(latency){
        uint32_t i = 0;//Speicherzeiger byteweise zugreifbar
        uint32_t j = 0;//Inhaltszeiger bewegt sich in 4-Byte-Schritten
        while (rom_content) {
            // 如果数组过大, 报错
            if (i >= size) {
                SC_REPORT_ERROR("ROM", "ROM content size is bigger than storage size! Failed to initialize ROM.");
                //这里怎么退出有待研究
                exit(1);
            }

            uint32_t word = rom_content[j];

            for (int k = 0; k < 4; ++k) {
                memory[i++] = (word >> (k * 8)) & 0xFF;
            }
            ++j;
        }
        ready.write(false);
        SC_THREAD(read);
    }

    /**
     * 返回ROM存储区大小
     * 
     * @return int, ROM大小
     */
    int size() {
        return memory.size();
    }

    void read() {
        while(true) {
            wait(start_read);
            ready.write(false);
            wait(latency, SC_NS);
            uint32_t addresse = addr.read();
            if (!wide) {
                if (memory.count(addresse)) {
                    data.write(static_cast<uint32_t>(memory[addresse]));
                    ready.write(true);
                } else {
                    SC_REPORT_WARNING("ROM", "Read from unmapped address (byte)");
                    data.write(0xFF);
                    ready.write(true);
                }
            } else {
                uint32_t result = 0;
                for (int i = 0; i < 4; ++i) {
                    uint32_t curr_addr = addresse + i;
                    if (memory.count(curr_addr)) {
                        result |= static_cast<uint32_t>(memory[curr_addr]) << (8 * i);
                    } else {
                        SC_REPORT_WARNING("ROM", "Read from unmapped address (word)");
                        result |= 0xFF << (8 * i);
                    }
                }
                data.write(result);
                ready.write(true);
            }
        }
    }

    bool write(uint32_t address,uint8_t data) {
        if (memory.count(address)) {
            memory[address] = data;
            return true;
        }
        return false;
    }

};


#endif
