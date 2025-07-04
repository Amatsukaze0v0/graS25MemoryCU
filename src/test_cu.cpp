#include <systemc>
#include "memory_controller.hpp"

using namespace sc_core;

SC_MODULE(TESTBENCH) {
    // 信号
    sc_in<bool> clk;
    sc_signal<bool> r {"read_Signal"}, w, wide, mem_ready;
    sc_signal<uint32_t> addr, wdata, mem_rdata;
    sc_signal<uint8_t> user;
    sc_signal<uint32_t> rdata, mem_addr, mem_wdata;
    sc_signal<bool> ready, error, mem_r, mem_w;

    MEMORY_CONTROLLER* controller;

    SC_CTOR(TESTBENCH) {
        // 初始化 ROM 内容
        static uint32_t rom_content[8] = {0x12345678, 0xAABBCCDD, 0xCAFEBABE, 0xDEADBEEF,
                                          0x00000001, 0xFFFFFFFF, 0x0000FFFF, 0x13579BDF};

        controller = new MEMORY_CONTROLLER("controller", 32, rom_content, 0, 4);

        // 绑定信号
        controller->clk(clk);
        controller->r(r);
        controller->w(w);
        controller->wide(wide);
/*         controller->mem_ready(mem_ready);
        controller->mem_rdata(mem_rdata);  */
/*         controller->rom_ready(rom_ready);
        controller->rom_data(rom_data); */
        controller->addr(addr);
        controller->wdata(wdata);
        controller->user(user);
        controller->rdata(rdata);
/*         controller->mem_addr(mem_addr); */
/*         controller->mem_wdata(mem_wdata); */
        controller->ready(ready);
        controller->error(error);
/*         controller->mem_r(mem_r);
        controller->mem_w(mem_w); */

        SC_THREAD(run_tests);
    }

    void run_tests() {
        wait(SC_ZERO_TIME);

        // TC1: 读取 ROM 中第一个值（addr = 0）
        std::cout << "\n--- TC1: Read from ROM address 0 (4B aligned) ---" << std::endl;
        addr.write(0);
        r.write(1);
        w.write(0);
        wide.write(1);
        user.write(1);

        wait(ready.posedge_event());
        check(rdata.read(), 0x12345678);


        // TC2: 读取 ROM 中第一个字节（addr = 0, 1B）
        std::cout << "\n--- TC2: Read from ROM address 0 (1B) ---" << std::endl;
        addr.write(0);
        r.write(1);
        w.write(0);
        wide.write(0);
        user.write(1);

        wait(ready.posedge_event());
        check(rdata.read(), 0x78); // 0x12345678 的低8位

        // TC3: 写入 ROM（应被拒绝）
        std::cout << "\n--- TC3: Write to ROM (expect error) ---" << std::endl;
        w.write(1);
        r.write(0);
        addr.write(4);
        wide.write(1);
        wdata.write(0x88888888);
        user.write(1);

        wait(clk.posedge_event());
        wait(SC_ZERO_TIME);

        check(error.read(), true);


        // TC4: 读取 ROM 边界（最后一个字节）
        // TODO: 根据ROM，我们的字符是逆向存储的，如0x12345678在rom中是0x78563412, 这会导致单字节读取问题
        std::cout << "\n--- TC4: Read from ROM last byte (1B) ---" << std::endl;
        addr.write(31); // 32字节ROM，最后一个字节
        r.write(1);
        w.write(0);
        wide.write(0);
        user.write(1);
        wait(clk.posedge_event());
        wait(ready.posedge_event());
        check(rdata.read(), 0xdf); // 0x13579BDF 的低8位

        r.write(0);
        w.write(0);
        wait(clk.posedge_event());

        // TC5: 主存 4B 写入+读取
        std::cout << "\n--- TC5: Write and Read Main Memory (4B) ---" << std::endl;
        addr.write(40);
        r.write(0);
        w.write(1);
        wide.write(1);
        wdata.write(0xA5A5A5A5);
        user.write(2);
        wait(clk.posedge_event());
        wait(ready.posedge_event());

        r.write(0);
        w.write(0);
        wait(clk.posedge_event());
        printf("but this time only set the r/w to ZERO~ \n");

        // 读取
        addr.write(40);
        r.write(1);
        w.write(0);
        wide.write(1);
        user.write(2);
        wait(clk.posedge_event());
        wait(ready.posedge_event());
        r.write(0);
        wait(SC_ZERO_TIME);

        check(rdata.read(), 0xA5A5A5A5);

        
        r.write(0);
        w.write(0);
        wait(clk.posedge_event());
        printf("but this time only set the r/w to ZERO~ \n");


        // TC6: 主存 1B 写入+读取
        std::cout << "\n--- TC6: Write and Read Main Memory (1B) ---" << std::endl;
        addr.write(40);
        r.write(0);
        w.write(1);
        wide.write(0);
        wdata.write(0x5A);
        user.write(2);
        printf("at least I write all the signals. \n");
        wait(clk.posedge_event());
        printf("Then I start to wait CU finish. \n");
        wait(SC_ZERO_TIME);
        printf("This is a zero time so that the ready-signal is correctly setten. \n");

/*         // TOOD: 问题出现在这里，诸如protection这种函数调用，ready error理应有跳变到1的过程，
        // 但是测试时发现（可能由于是CU 线程快过tb）这个ready会陷入无限等待，因为ready早已为1。
        wait(ready.posedge_event());
        printf("Return value / result should be here. \n"); */

        check(error.read(), true);

        r.write(0);
        w.write(0);
        wait(clk.posedge_event());
        printf("but this time only set the r/w to ZERO~ \n");

        
        // 读取
        addr.write(40);
        r.write(1);
        w.write(0);
        wide.write(0);
        user.write(2);
        
        wait(clk.posedge_event());
        wait(SC_ZERO_TIME);

        check(error.read(), true);

        r.write(0);
        w.write(0);
        wait(clk.posedge_event());
        printf("but this time only set the r/w to ZERO~ \n");


        // TC7: 主存 4B 非对齐写入（应报错）
        std::cout << "\n--- TC7: Write Main Memory (4B, unaligned, expect error) ---" << std::endl;
        addr.write(42); // 非4字节对齐
        r.write(0);
        w.write(1);
        wide.write(1);
        wdata.write(0xDEADBEEF);
        user.write(2);
        wait(clk.posedge_event());
        wait(SC_ZERO_TIME);
        wait(ready.posedge_event());

        // 读取
        addr.write(40);
        r.write(1);
        w.write(0);
        wide.write(1);
        user.write(2);
        wait(clk.posedge_event());
        wait(ready.posedge_event());
        r.write(0);
        wait(SC_ZERO_TIME);
        // TODO: 这里应该读到什么？？？
        // 已知 40 = 0xa5a5a5a5; 42 = 0xDEADBEEF
        // 那么 40 - 46 应该为 0xa5a5DEADBEEF ?
        check(rdata.read(), 0xA5A5DEAD);

        r.write(0);
        w.write(0);
        wait(clk.posedge_event());
        printf("but this time only set the r/w to ZERO~ \n");


/*         // TC8: 主存 权限测试
        std::cout << "\n--- TC8: Main Memory permission test ---" << std::endl;
        addr.write(48);
        r.write(0);
        w.write(1);
        wide.write(1);
        wdata.write(0x11223344);
        user.write(3);
        wait(clk.posedge_event());
        wait(ready.posedge_event());

        // 另一个user尝试读取
        addr.write(48);
        r.write(1);
        w.write(0);
        wide.write(1);
        user.write(4); // 没有权限
        wait(clk.posedge_event());
        wait(ready.posedge_event());
        check(error.read(), true);
        r.write(0);
        wait(clk.posedge_event());
        wait(ready.negedge_event()); */

/*         // TC9: ROM 超出范围读取
        std::cout << "\n--- TC9: Read ROM out of range ---" << std::endl;
        addr.write(1000);
        r.write(1);
        w.write(0);
        wide.write(1);
        user.write(1);
        wait(clk.posedge_event());
        wait(ready.posedge_event());
        check(rdata.read(), 0xFFFFFFFF); // 你的ROM实现可能返回0xFFFFFFFF */

        sc_stop();
    }

    void check(uint32_t actual, uint32_t expected) {
        if (actual != expected) {
            std::cerr << "Check failed! Got 0x" << std::hex << actual
                      << ", expected 0x" << expected << std::endl;
        } else {
            std::cout << "✔ Check passed: 0x" << std::hex << actual << std::endl;
        }
    }
};

int sc_main(int argc, char* argv[]) {
    TESTBENCH tb("tb");
    sc_clock clk("clk", 1, SC_NS);
    tb.clk(clk);
    sc_start();
    return 0;
}