#include<systemc>
#include"Memory.cpp"
using namespace sc_core;


#ifndef ROM_H
#define ROM_H

SC_MODULE(ROM) {

    sc_out<bool> ready;
    Memory memoryUnit;

    // TODO: 此处memory没有设定latency，默认值为 0 Ns; 另外，初始化在哪？
    /**
     * 创建一个ROM，存在对于size的二次检查
     * 
     * @param name 名字
     * @param size 设定的ROM大小
     * @param rom_content 数组指针，指向初始化数组
     * 
     */
    ROM(sc_module_name name,uint32_t size, uint32_t* rom_content):sc_module(name), memoryUnit("ROMspeicher", size, true){
        // 如果数组过大, 报错
        if (memoryUnit.storage.size() < size) {
            SC_REPORT_ERROR("ROM", "ROM content size is bigger than storage size! Failed to initialize ROM.");
        }
        // 拷贝 rom_content 到 storage
        for (uint32_t i = 0; i < size; ++i) {
            memoryUnit.storage[i] = rom_content[i];  // uint32_t -> sc_uint<32> 自动转换
        }

        // TODO：存储模块ready即为ROM ready？是否存在其他要求？
        ready.bind(memoryUnit.mem_ready);
    }

    /**
     * 返回ROM存储区大小
     * 
     * @return int, ROM大小
     */
    int size() {
        return memoryUnit.storage.size();
    }

    /**
     * 读取一个ROM整段4字节数据，默认已经进行过权限检查
     * @param addr 地址
     * @return 返回地址储存的 4Bytes对齐 的值
     */
    uint32_t read(uint32_t addr) {
        return memoryUnit.read(addr);
    }

};


#endif
