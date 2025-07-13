#include <map>
#include <systemc>

#include "main_memory.hpp"
#include "rom.hpp"
using namespace sc_core;

#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H
/*
 *仅便于个人开发，最后记得删除，以下所有要保留的注释一律用德语写
 *
 *1.内存映射，控制器实现只负责转发地址，rom实现映射
 *写入
 *2.权限控制单元 维护一个map，一个地址对应一个user
 *谁写入谁有权限
 *0 255 超级用户可以随便访问
 *255访问后要清楚所有者
 *注意要具有所有字段的访问权限
 *
 *外部应该有两块：我们的动态大小内存和主存(可以直接拿作业的答案)
 *
 *启动时ready信号0，完成要设为1
 *rom只负责读操作,里面的内容不可更改
 *rom大小要是2的次方（期望主程序检查）
 *写入操作这里会拒接但是额外方法依然要求rom内部通过写入方法
 *rom有延迟所以要等待rom完成读取
 *如果等待的时候有新的同一地址的读请求怎么办
 *地址超出范围通过mem_r mem_w转发给主存并等待mem_ready信号使用mem_rdata
 *
 *访问宽度为单字节或四字节
 *主存只支持四字节访问，单字节访问应该只取低8bit
 *写入单字节先读取，修改低八位再写回
 *
 *
 */
SC_MODULE(MEMORY_CONTROLLER) {

    //input
    sc_in<bool> clk,r,w,wide,mem_ready;
    sc_in<uint32_t> addr,wdata,mem_rdata;
    sc_in<uint8_t> user;

     //output
    sc_out<uint32_t> rdata, mem_addr, mem_wdata;
    sc_out<bool> ready {"ready_signal_for_CU"}, error {"error"},mem_r {"memory_read"},mem_w {"memory_write"};

    //innere Komponenten
    ROM* rom;
    sc_signal<uint32_t> rom_addr_sig,data_cu_rom;   
    sc_signal<bool> rom_read_en,rom_wide_sig,ready_cu_rom;   

    //Adresse und ihrer Benutzer
    std::map<uint32_t,uint8_t> gewalt;

    uint32_t block_size;
    uint32_t rom_size;
    // 消去thread error——定义process函数必须在创建thread前，除非加上这句声明。
    SC_HAS_PROCESS(MEMORY_CONTROLLER);

    //TODO： 添加了一个数组作为初始化rom参数，请检查。
    //edited： 这里block size未知，因此必须有blocksize作为参数传入，方能对齐gewalt——权限管理 和 memory/rom 读取或允许的写入
    MEMORY_CONTROLLER(sc_module_name name, uint32_t rom_size, uint32_t* rom_content, uint32_t latency_rom, uint32_t block_size):sc_module(name)
    {
        //initialisieren
        //这里直接创建期望主程序创建模块时已经检查了romsize
        // 检查 rom_content 是否为 NULL
        if (rom_content == NULL) {
            rom_content = new uint32_t[rom_size / sizeof(uint32_t)]();
        }
        printf("ROM size is: %d\n", rom_size);
        this->rom_size = rom_size;
        rom = new ROM("rom", rom_size, rom_content,latency_rom);
        rom->read_en(rom_read_en);
        rom->clk(clk);
        rom->addr(rom_addr_sig);
        rom->wide(rom_wide_sig);
        rom->ready(ready_cu_rom);
        rom->data(data_cu_rom);

        this->block_size = block_size;
        SC_THREAD(process);
        sensitive << clk.pos();
    }

    void process() {
        while (true) {
            wait(clk.posedge_event());
            printf("CU job started~ \n");
            if (r.read() && w.read()){
                SC_REPORT_ERROR("Memory Controller","Fehler: Gleichzeitiger Lese- und Schreibzugriff ist nicht erlaubt.\n");
                continue;
            }
            if (r.read()) {
                ready.write(0);
                if (protection()) {
                    read();
                } else {
                    error.write(1);
                    ready.write(1);
                    wait(SC_ZERO_TIME);
                    continue;
                }
            }
            if (w.read()) {
                ready.write(0);
                if (protection()) {
                    write();  
                }else {
                    error.write(1);
                    ready.write(1);
                    wait(SC_ZERO_TIME);
                    continue;
                }
            }

            error.write(0);

        }
    }

    //已校验权限过后的读取行为
    void read() {
        // 如果读取的是ROM
        if (addr.read() < rom->size()) {       
            // 使用信号和标志位变量控制的版本
            // 判断地址是否合规, 题目要求wide为0时代表1B宽度
            uint32_t address = addr.read();

            //Überprüfung, ob die 4-Byte-ausgerichtete Adresse außerhalb des ROM-Bereichs liegt
            if (wide.read() && rom -> size() < 4 || address > rom->size() - 4) {
                char buf[128];
                printf("Error without interruption: Address 0x%08X by ROM access is out of Range under 4B alignment.\n", address);
                //snprintf(buf, sizeof(buf), "Address 0x%08X by ROM access is out of Range under 4B alignment.\n", address);
                //SC_REPORT_ERROR("Memory Controller : read", buf);
                error.write(1);
                ready.write(1);
                return;
            } 

            // 暂且保留rom_xxxx_sig的写法, 对rom input赋值
            printf("[CU] set rom_wide_sig = %d, rom_addr_sig = 0x%08X\n", wide.read(), address);
            rom_wide_sig.write(wide.read());
            rom_addr_sig.write(address);
            rom_read_en.write(1);
            printf("[CU] wait for rom_ready.posedge_event() ...\n");
            // Warten auf Rom —— 等待ROM完成工作并回传，ROM处理宽度问题
            wait(ready_cu_rom.posedge_event());
            wait(SC_ZERO_TIME);
            printf("[CU] rom_ready arrived, rom_data = 0x%08X\n", data_cu_rom.read());

            // 读取返回信号, 宽度判断由ROM处理
            rdata.write(data_cu_rom.read());
            rom_read_en.write(0);
            ready.write(1);   
            printf("[CU] rdata set to 0x%08X, ready=1\n", data_cu_rom.read());
            //到这一步我们认为ROM已经成功读取， 行为结束

        } else {
            // 检验对齐
            if (wide.read()) {
                uint32_t address = addr.read();
                // 赋值addr，并给予信号开始4B读取
                printf("[CU] memory 4B read request: addr=0x%08X, wide=%d\n", addr.read(), wide.read());
                mem_addr.write(addr.read());
                mem_r.write(1);
                // 等待mem读取完成
                printf("[CU] wait for mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                wait(SC_ZERO_TIME);
                printf("[CU] memory 4B read done: addr=0x%08X, mem_rdata=0x%08X\n", addr.read(), mem_rdata.read());
                // 按宽度写入返回值
                rdata.write(mem_rdata.read());
                // 结束mem读取逻辑
                ready.write(1);
            } else {
                uint32_t offset = addr.read() % 4;
                uint32_t address = addr.read() - offset;
                // 赋值addr为四字节对齐，并给予信号开始1B读取
                printf("[CU] memory 1B read request: addr=0x%08X, wide=%d\n", addr.read(), wide.read());
                mem_addr.write(address);
                mem_r.write(1);
                // 等待mem读取完成
                printf("[CU] wait for mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                wait(SC_ZERO_TIME);
                // 按宽度写入返回值, 已知是小端序
                uint32_t raw_data = mem_rdata.read();
                uint32_t real_data = (raw_data >> (offset * 8)) & 0xFF;
                real_data = real_data >> (4 - offset);
                rdata.write(real_data);
                printf("[CU] memory 1B read done: addr=0x%08X, mem_rdata=0x%08X\n", addr.read(), real_data);
                // 结束mem读取逻辑, 重置信号
                ready.write(1);
            }
        }
    }
    void write() {
        if (addr.read() >= rom->size()) {
            // 个人版本
            uint32_t new_data;
            // 检查对齐情况
            if (!wide.read()) {
                // 1B对齐，要先读取并拓展data字段使其正确写入
                printf("[CU] memory write request (1B): addr=0x%08X, wdata=0x%02X, user=%u\n", addr.read(), wdata.read() & 0xFF, user.read());
                mem_addr.write(addr);
                mem_r.write(1);
                // 等待读取返回结果
                printf("[CU] wait for mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());           
                wait(SC_ZERO_TIME);
                // 写入前停止读取避免冲突
                mem_r.write(0);
                wait(SC_ZERO_TIME);
                // 赋值
                uint32_t prev_data = mem_rdata.read();
                printf("[CU] got raw_data at address 0x%08x with value 0x%08x.\n", addr.read(), prev_data);
                uint32_t low_8bits = wdata.read();
                uint8_t offset = addr.read() % 4;
                // 清除对应字节
                uint32_t mask = ~(0xFF << (offset * 8));
                uint32_t cleared = prev_data & mask;
                // 插入新的字节
                uint32_t inserted = low_8bits << (offset * 8);
                // 合并为待写入新的值
                new_data = cleared | inserted;

                printf("[CU] new Data value is 0x%08x.\n", new_data);
            } else {
                printf("[CU] memory write request (4B): addr=0x%08X, wdata=0x%08X, user=%u\n", addr.read(), wdata.read(), user.read());
                new_data = wdata.read();
            }
            // 告诉mem准备写入
            mem_addr.write(addr);
            mem_wdata.write(new_data);
            mem_w.write(1);
            wait(SC_ZERO_TIME);
            // 等待写入完成
            printf("[CU] wait for mem_ready.posedge_event() ...\n");
            do {
                wait(clk.posedge_event());
            } while (!mem_ready.read());  
            wait(SC_ZERO_TIME);
            mem_w.write(0);
            
            printf("[CU] memory write done: addr=0x%08X, wdata=0x%08X\n", addr.read(), new_data);
            //Benutzer mit Adresse verknüpfen
            if (user.read() != 0 && user.read() != 255) {
                uint32_t block_addr = (addr.read() / block_size) * block_size;
                gewalt[block_addr] = user.read();
            } else if (user.read() == 255) {
                uint32_t block_addr = (addr.read() / block_size) * block_size;
                gewalt.erase(block_addr);
            }

            ready.write(1);
        } else {
            // ROM 禁止写入
            printf("Die Adresse 0x%08X liegt in ROM und darf nicht verändert werden.\n", addr.read());
            error.write(1);
            ready.write(1);
            wait(SC_ZERO_TIME);
        }
    }

    //认为这里是检查用户权限
    bool protection() {
        uint8_t benutzer = user.read();
        uint32_t adresse = addr.read();
        uint32_t num_bytes = wide.read() ? 4 : 1;

        ready.write(0);

/*         for (uint32_t i = 0; i < num_bytes; ++i) {
            uint32_t byte_addr = adresse + i; */

/*             // ROM 区域允许读，不允许写 ---- 不应该在此判断
            if (byte_addr < rom->size()) {
                if (w.read()) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Fehler: Schreibzugriff auf ROM-Adresse 0x%08X ist verboten.\n",adresse);
                    SC_REPORT_ERROR("Memory Controller: protection",buf);
                    return false;
                }
                // Jeder darf ROM lesen
                continue;
            } */

            // 超级用户永远拥有权限
            if (benutzer == 0 || benutzer == 255) {
                return true;
            }

            // 计算所属 block 的起始地址
            uint32_t block_addr = ( adresse - rom_size) / block_size;

            auto it = gewalt.find(block_addr);
            if (it == gewalt.end()) {
                // 这个 block 还没有 owner，任何人都可以访问
                return true;
            }

            if (it->second != benutzer) {
                char buf[128];
                printf("User %u hat keine Berechtigung auf Block 0x%08X (Adresse 0x%08X).\n",benutzer, block_addr, adresse);
                //snprintf(buf, sizeof(buf), "User %u hat keine Berechtigung auf Block 0x%08X (Adresse 0x%08X).\n",benutzer, block_addr, byte_addr);
                //SC_REPORT_ERROR("Memory Controller: protection",buf);
                return false;
            }
        //}
        return true;
    }

    void setRomAt(uint32_t address, uint8_t data) {
        if (!rom->write(addr, data)) {
            SC_REPORT_WARNING("ROM", "Write to unmapped address (byte)");
        }
    }

    uint8_t getOwner(uint32_t addr) {
        uint32_t block_addr = (addr / block_size) * block_size;
        auto it = gewalt.find(block_addr);
        return it != gewalt.end() ? it->second : 0;
    }


};






#endif //MEMORY_CONTROLLER_H
