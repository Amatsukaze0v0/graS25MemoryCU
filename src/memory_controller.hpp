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
    sc_in<bool> rom_ready;
    sc_in<uint32_t> rom_data;   //这是什么用途？

    sc_signal<uint32_t> rom_addr_sig, wdata_cu_mem, rdata_cu_mem, addr_cu_mem, data_cu_rom;   
    sc_signal<bool> rom_read_en, mem_en {"Memory_enalbe_in_CU"}, rom_wide_sig, ready_cu_mem, w_cu_mem, r_cu_mem, ready_cu_rom;   

    //output
    sc_out<uint32_t> rdata, mem_addr, mem_wdata;
    sc_out<bool> ready {"ready_signal_for_CU"}, error {"error"},mem_r {"memory_read"},mem_w {"memory_write"};

    //innere Komponenten
    ROM* rom;
    MAIN_MEMORY* memory;

    //Adresse und ihrer Benutzer
    std::map<uint32_t,uint8_t> gewalt;

    //Kennzeichen dafür, ob gerade auf das ROM/HauptSpeicher gewartet wird
    //这里的控制流程略有问题，其实不可能同时执行三种情况所以一个控制信号就足够了
    bool waitRom;
    bool waitMemR;
    bool waitMemW;

    uint32_t block_size;
    // 消去thread error——定义process函数必须在创建thread前，除非加上这句声明。
    SC_HAS_PROCESS(MEMORY_CONTROLLER);

    //TODO： 添加了一个数组作为初始化rom参数，请检查。
    //edited： 这里block size未知，因此必须有blocksize作为参数传入，方能对齐gewalt——权限管理 和 memory/rom 读取或允许的写入
    MEMORY_CONTROLLER(sc_module_name name, uint32_t rom_size, uint32_t* rom_content, uint32_t latency_rom, uint32_t block_size):sc_module(name)
    {
        //initialisieren
        //这里直接创建期望主程序创建模块时已经检查了romsize
        rom = new ROM("rom", rom_size, rom_content, 8, latency_rom);
        rom->read_en(rom_read_en);
        rom->addr(rom_addr_sig);
        rom->wide(rom_wide_sig);
        rom->ready(ready_cu_rom);
        rom_ready(ready_cu_rom);
        rom_data(data_cu_rom);
        rom->data(data_cu_rom);

        // 初始化一块主存
        memory = new MAIN_MEMORY("memory");
        memory->ready(ready_cu_mem);
        mem_ready(ready_cu_mem);
        memory->mem_en(mem_en);
        memory->clk(clk);
        memory->addr(addr_cu_mem);
        mem_addr(addr_cu_mem);
        mem_wdata(wdata_cu_mem);
        memory->wdata(wdata_cu_mem);
        mem_rdata(rdata_cu_mem);
        memory->rdata(rdata_cu_mem);
        memory->w(w_cu_mem);
        mem_w((w_cu_mem));
        memory->r(r_cu_mem);
        mem_r(r_cu_mem);

        waitRom = 0;
        waitMemR = 0;
        waitMemW = 0;
        this->block_size = block_size;
        SC_THREAD(process);
        sensitive << clk.pos();
    }

    void process() {
        while (true) {
            wait(clk.posedge_event());
            if (r.read() && w.read()){
                printf("Fehler: Gleichzeitiger Lese- und Schreibzugriff auf Adresse 0x%08X ist nicht erlaubt.\n", addr.read());
            }
            if (r.read()) {
                ready.write(0);
                if (protection()) {
                    read();
                } else {
                    error.write(1);
                    ready.write(1);
                    wait(SC_ZERO_TIME);
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
                }
            }
        }
    }

    //已校验权限过后的读取行为
    void read() {
        // 如果读取的是ROM
        if (addr.read() < rom->size()) {       
            // 使用信号和标志位变量控制的版本
            // 判断地址是否合规, 题目要求wide为0时代表1B宽度
            uint32_t address;
            if (wide.read()) {
                address = addr.read();
                // 非4B对齐， 在信号层面报错
                if (address % 4 != 0) {
                    printf("Address 0x%08X is illegal without 4B alignment.\n", address);
                    error.write(1);
                    ready.write(1);
                    return;
                }
            } else {
                // 1B对齐手动挪动地址至最低有效位
                address = (address / 4) << 2;
            }
            

            // 暂且保留rom_xxxx_sig的写法, 对rom input赋值
            printf("[CU] set rom_wide_sig = %d, rom_addr_sig = 0x%08X\n", wide.read(), addr.read());
            rom_wide_sig.write(wide.read());
            rom_addr_sig.write(address);
            rom_read_en.write(1);
            printf("[CU] wait for rom_ready.posedge_event() ...\n");
            // Warten auf Rom —— 等待ROM完成工作并回传，ROM处理宽度问题
            wait(rom_ready.posedge_event());
            wait(SC_ZERO_TIME);
            printf("[CU] rom_ready arrived, rom_data = 0x%08X\n", rom_data.read());

            // 读取返回信号, 宽度判断由ROM处理
            rdata.write(rom_data.read());
            rom_read_en.write(0);
            ready.write(1);   
            printf("[CU] rdata set to 0x%08X, ready=1\n", rom_data.read());
            //到这一步我们认为ROM已经成功读取， 行为结束

        } else {
            // 检验对齐
            if (wide.read()) {
                uint32_t address = addr.read();
                // 非4B对齐， 在信号层面报错
                if (address % 4 != 0) {
                    printf("Address 0x%08X ROM reading is illegal without 4B alignment.\n", address);
                    error.write(1);
                    ready.write(1);
                    return;
                }
            } else {
                // 题目要求：主存仅支持4B访问
                printf("Address 0x%08X with 1B alignment is not allowed to read Main Memory.\n", addr.read());
                error.write(1);
                ready.write(1);
                wait(SC_ZERO_TIME);
                return;
            }

            // 赋值addr，并给予信号开始读取, mem不处理宽度问题。仅允许4B读取
            printf("[CU] memory read request: addr=0x%08X, wide=%d\n", addr.read(), wide.read());
            mem_addr.write(addr.read());
            mem_r.write(1);
            mem_en.write(1);
            // 等待mem读取完成
            printf("[CU] wait for mem_ready.posedge_event() ...\n");
            wait(mem_ready.posedge_event());
            wait(SC_ZERO_TIME);
            printf("[CU] memory read done: addr=0x%08X, mem_rdata=0x%08X\n", addr.read(), mem_rdata.read());
            // 按宽度写入返回值
            rdata.write(mem_rdata.read());
            mem_en.write(0);
            // 结束mem读取逻辑
            mem_r.write(0);
            ready.write(1);
        }
    }
    void write() {
        if (addr.read() >= rom->size()) {
            // 个人版本
            uint32_t new_data;
            // 检查对齐情况
            if (!wide.read()) {
                // 题目要求：主存仅支持4B访问
                printf("Address 0x%08X with 1B alignment is not allowed to write Main Memory.\n", addr.read());
                error.write(1);
                ready.write(1);
                return;
/*              // 1B对齐，要先读取并拓展data字段使其正确写入
                printf("[CU] memory write request (1B): addr=0x%08X, wdata=0x%02X, user=%u\n", addr.read(), wdata.read() & 0xFF, user.read());
                mem_addr.write(addr);
                mem_r.write(1);
                mem_en.write(1);

                // 等待读取返回结果
                printf("[CU] wait for mem_ready.posedge_event() ...\n");
                wait(mem_ready.posedge_event());
                wait(SC_ZERO_TIME);
                // 赋值
                uint32_t prev_data = mem_rdata.read();
                uint32_t low_8bits = wdata.read();
                uint8_t offset = addr.read() % 4;
                // 清除对应字节
                uint32_t mask = ~(0xFF << (offset * 8));
                uint32_t cleared = prev_data & mask;
                // 插入新的字节
                uint32_t inserted = low_8bits << (offset * 8);
                // 合并为待写入新的值
                new_data = cleared | inserted;
                // 写入前停止读取避免冲突
                mem_r.write(0);
                mem_en.write(0); */
            } else {
                printf("[CU] memory write request (4B): addr=0x%08X, wdata=0x%08X, user=%u\n", addr.read(), wdata.read(), user.read());
                new_data = wdata.read();
            }
            // 告诉mem准备写入
            mem_addr.write(addr.read());
            mem_wdata.write(new_data);
            mem_w.write(1);
            mem_en.write(1);
            wait(SC_ZERO_TIME);
            // 等待写入完成
            printf("[CU] wait for mem_ready.posedge_event() ...\n");
            wait(mem_ready.posedge_event());
            
            printf("[CU] memory write done: addr=0x%08X, wdata=0x%08X\n", addr.read(), new_data);
            //Benutzer mit Adresse verknüpfen
            if (user.read() != 0 && user.read() != 255) {
                for (uint32_t i = 0; i < (wide.read() ? 4 : 1); ++i) {
                    gewalt[addr.read() + i] = user.read();
                }
            } else if (user.read() == 255) {
                for (uint32_t i = 0; i < (wide.read() ? 4 : 1); ++i) {
                    gewalt.erase(addr.read() + i);
                }
            }
            waitMemW = 0;
            mem_en.write(0);
            ready.write(1);
        } else {
            // ROM 禁止写入
            printf("Die Adresse 0x%08X liegt in ROM und darf nicht verändert werden.\n", addr.read());
            error.write(1);
            ready.write(1);
        }
    }

    //认为这里是检查用户权限
    bool protection() {
        uint8_t benutzer = user.read();
        uint32_t adresse = addr.read();
        uint32_t num_bytes = wide.read() ? 4 : 1;

        for (uint32_t i = 0; i < num_bytes; ++i) {
            uint32_t byte_addr = adresse + i;

            if (byte_addr < rom ->size()) {
                if (w.read()) {
                    printf("Fehler: Schreibzugriff auf ROM-Adresse 0x%08X ist verboten.\n", adresse);
                    return false;
                }
                //Jeder darf ROM zugreifen
                break;
            }

            //User 0 und 255 dürfen alle Adressen zugreifen.
            if (benutzer == 0 || benutzer == 255) {
                continue;
            }

            //Überprüfen ob Adresse schon Benutzer hat
            auto it = gewalt.find(byte_addr);
            if (it == gewalt.end()) {
                continue;
            }

            if (it->second != benutzer) {
                printf("User %u hat keine Berechtigung auf Adresse 0x%08X.\n", benutzer, byte_addr);
                return false;
            }
        }
        return true;
    }

    void setRomAt(uint32_t address, uint8_t data) {
        if (!rom->write(addr, data)) {
            SC_REPORT_WARNING("ROM", "Write to unmapped address (byte)");
        }
    }

    uint8_t getOwner(uint32_t addr) {
        return gewalt[addr]?gewalt[addr]:0;
    }


};






#endif //MEMORY_CONTROLLER_H
