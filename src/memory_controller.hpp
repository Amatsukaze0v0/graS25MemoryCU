#include <map>
#include <systemc>

#include "main_memory.hpp"
#include"rom.hpp"
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
    sc_out<uint32_t> rdata,mem_addr,mem_wdata;
    sc_out<bool> ready,error,mem_r,mem_w;

    //innere Komponenten
    ROM* rom;

    //Adresse und ihrer Benutzer
    std::map<uint32_t,uint8_t> gewalt;

    //Kennzeichen dafür, ob gerade auf das ROM/HauptSpeicher gewartet wird
    //这里的控制流程略有问题，其实不可能同时执行三种情况所以一个控制信号就足够了
    bool waitRom;
    bool waitMemR;
    bool waitMemW;

    // 消去thread error——定义process函数必须在创建thread前，除非加上这句声明。
    SC_HAS_PROCESS(MEMORY_CONTROLLER);

    //TODO： 添加了一个数组作为初始化rom参数，请检查。
    MEMORY_CONTROLLER(sc_module_name name, uint32_t rom_size, uint32_t* rom_content):sc_module(name)
    {
        //initialisieren
        //这里直接创建期望主程序创建模块时已经检查了romsize
        rom = new ROM("rom",rom_size, rom_content);
        error.write(0);
        ready.write(0);
        waitRom = 0;
        waitMemR = 0;
        waitMemW = 0;
        SC_THREAD(process);
        sensitive << clk.pos();
    }

    void process() {
        while (true) {
            wait();
            if (r.read() && w.read()){
                printf("Fehler: Gleichzeitiger Lese- und Schreibzugriff auf Adresse 0x%08X ist nicht erlaubt.\n", addr.read());
            }
            if (r.read()) {
                if (protection()) {
                    read();
                }else {
                    error.write(1);
                    ready.write(1);
                }
            }
            if (w.read()) {
                if (protection()) {
                    write();
                }else {
                    error.write(1);
                    ready.write(1);
                }
            }
        }
    }

    void read() {
        if (addr.read()<rom->size()) {
            //期待rom内部实现函数size返回大小,实现读信号与读完信号,判断字节宽度并返回数据
            if (!waitRom) {
                //Adresse liegt im ROM
                //给rom传入读取地址和字节宽度
                waitRom = 1;
            }else if (waitRom){
                if (rom->ready.read()) {
                    waitRom = 0;
                    //rdata.write();写入rom读取的数据
                    ready.write(1);
                }
                //Warten auf Rom
            }
        }else {
            //Adresse liegt im Hauptspeicher
            if (!waitMemR){
                mem_addr.write(addr.read());
                mem_r.write(1);
                waitMemR = 1;
            }
            if (mem_ready.read()) return;
            if (wide.read()) {
                rdata.write(mem_addr.read());
            }else {
                //Nur die niedrigsten 8 Bits nehmen
                rdata.write(mem_addr.read()& 0xFF);
            }
            waitMemR = 0;
            ready.write(1);
        }
    }
    void write() {
        if (addr.read() >= rom->size()) {
            if (!waitMemW) {
                mem_addr.write(addr.read());
                mem_r.write(1);
                waitMemW = 1;
            }
            if (mem_ready.read()) return;
            uint32_t data = mem_rdata.read();
            if (wide.read()) {
                data = wdata.read();
            }else {
                uint8_t new_byte = wdata.read() & 0xFF;
                data &= 0xFFFFFF00;
                data |= new_byte;
            }
            //这里暂时没有考虑mem的等待写完问题
            // TODO： （刘）mem有个mem_ready信号，为0代表正在写入，为1代表完成。（但是实际上读取也会置零，不清楚这个是否需要处理）
            mem_w.write(1);
            mem_wdata.write(data);
            mem_addr.write(addr.read());
            //Benutzer mit Adresse verknüpfen
            if (user.read() != 0 && user.read() != 255) {
                for (uint32_t i = 0; i < (wide.read() ? 4 : 1); ++i) {
                    gewalt[addr.read() + i] = user.read();
                }
            }else if (user.read() == 255) {
                for (uint32_t i = 0; i < (wide.read() ? 4 : 1); ++i) {
                    gewalt.erase(addr.read() + i);
                }
            }
            waitMemW = 0;
            ready.write(1);
        }else {
            printf("Die Adresse 0x%08X liegt in ROM und darf nicht verändert werden.\n", addr.read());
            error.write(1);
            ready.write(1);
        }
    }

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

};






#endif //MEMORY_CONTROLLER_H
