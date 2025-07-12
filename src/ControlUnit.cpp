#include <systemc.h>

#include "rahmenprogramm.h"
#include "memory_controller.hpp"



struct Result run_simulation(
    uint32_t cycles,
    const char* tracefile,
    uint32_t latencyRom,
    uint32_t romSize,
    uint32_t blockSize,
    uint32_t* romContent,
    uint32_t rom_content_size,
    uint32_t numRequests,
    struct Request* requests
) {
    struct Result result = {0, 0};

    // 这里写你自己的仿真逻辑
    sc_time period(10, SC_NS);  // 时钟周期 10ns

    sc_clock clk("clk", period);
    sc_signal<uint32_t> addr,wdata,mem_rdata,rdata,mem_addr,mem_wdata;
    sc_signal<bool> r,w,wide,mem_ready,ready,error,mem_r,mem_w;
    sc_signal<uint8_t> user;

    MEMORY_CONTROLLER* memory_controller = new MEMORY_CONTROLLER("memory controller",romSize,romContent,latencyRom,blockSize);
    MEMORY* memory = new MEMORY("Main Memory",66);

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

    memory->mem_rdata(mem_rdata);
    memory->mem_addr(mem_addr);
    memory->mem_wdata(mem_wdata);
    memory->mem_ready(mem_ready);
    memory->mem_r(mem_r);
    memory->mem_w(mem_w);

    uint32_t total_cycles = 0;
    uint32_t error_count = 0;

    sc_trace_file* tf = nullptr;
    //设置信号文件，这里判断了是否传入信号文件路径，传入了才进行
    if (tracefile != nullptr && strlen(tracefile) > 0) {
    // 去掉 .vcd 后缀（如果有）
    std::string tfname(tracefile);
    if (tfname.size() >= 4 && tfname.substr(tfname.size() - 4) == ".vcd") {
        tfname = tfname.substr(0, tfname.size() - 4);  // 去除 ".vcd"
    }

    tf = sc_create_vcd_trace_file(tfname.c_str());

    // 绑定所有信号
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

    std::cout << "[TRACE] Tracing enabled: " << tfname << ".vcd\n";
    }
    for (std::size_t i = 0; i < numRequests; ++i) {
    const Request& req = requests[i];

    // 设置输入信号
    addr.write(req.addr);
    wdata.write(req.data);
    w.write(req.w);
    r.write(!req.w);  // 假设不是写就是读
    wide.write(req.wide);
    user.write(req.user);

    // 打印调试信息
    std::cout << "[" << sc_time_stamp() << "] "
              << (req.w ? "WRITE" : "READ") << " request: "
              << "addr=0x" << std::hex << req.addr
              << ", data=0x" << req.data
              << ", user=" << std::dec << (int)req.user
              << ", wide=" << (int)req.wide
              << std::endl;

    // 启动仿真一个周期（或多个周期）
    sc_start(period);  // 一拍启动：传递信号到模块
    total_cycles++;

    // 可选：等待 ready 信号变为 true
    while (!ready.read()) {
        sc_start(period); // 等待模块响应
        total_cycles++;
    }

    if (error.read()) {
            std::cerr << "  --> ERROR: Module reported error on request " << i << std::endl;
            error_count++;
    }

    //这里统一重置信号，mc里面的一些地方可能多余
    w.write(false);
    r.write(false);
    }
    if (tf != nullptr) {
        sc_close_vcd_trace_file(tf);
    }

    result.cycles = total_cycles;
            result.errors = error_count;

        return result;
}

void connection(){
    
}

int sc_main(int argc, char* argv[]) {
    MemConfig config;
    struct Request* requests = NULL;
    uint32_t num_requests = 0;
    uint32_t* rom_content = NULL;
    uint32_t rom_content_size = 0;

    if(parse_arguments(argc, argv, &config)!=0){
        return 1;
    }

    if(config.rom_content_file!=NULL){
        rom_content = load_rom_content(config.rom_content_file, config.rom_size, &rom_content_size);
        if(rom_content==NULL){
            fprintf(stderr, "Error loading ROM content.\n");
            return EXIT_FAILURE;
        }
    }

    if(parse_csv_file(config.inputfile, &requests, &num_requests)!=0){
        fprintf(stderr, "Error parsing CSV file.\n");
        if(rom_content!=NULL){
            free(rom_content);
        }
        return EXIT_FAILURE;
    }

    struct Result result = run_simulation(
        config.cycles,
        config.tracefile,
        config.latency_rom,
        config.rom_size,
        config.block_size,
        rom_content,
        rom_content_size,
        num_requests,
        requests
    );

    printf("\n --- Simulation Finished --- \n");
    printf("Cycles: \n");
    printf("Errors: \n");

    if(rom_content!=NULL) free(rom_content);
    if(requests!=NULL) free(requests);
    return 0;
}