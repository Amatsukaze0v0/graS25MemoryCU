#include <systemc.h>

SC_MODULE(Memory) {

    sc_in<uint32_t> mem_input, mem_addr;    // 题目描述似乎是input == wdata
    sc_in<bool> mem_r, mem_w;
    sc_out<uint32_t> mem_output;    // 同理，output == rdata
    sc_out<bool> mem_ready;

    std::vector<sc_uint<32>> storage;
    uint32_t latency;   // todo: 单位未知
    /* bool is_read_only;  // ROM 只读标志位，约定true为只读 */

    SC_HAS_PROCESS(Memory);

    /**
     * 构造一个 Memory 模块（主存/ROM）
     * 
     * 接口：sc_in<uint32_t> mem_input, mem_addr;
     *       sc_in<bool> mem_r, mem_w;
     *       sc_out<uint32_t> mem_output;
     *       sc_out<bool> mem_ready;
     * 
     * @param name SystemC模块名,（没啥用）
     * @param size 内存大小，约定单位为 4Byte word 数
     * @param readonly 如果为 true，则表示只读（ROM），空参默认值为false
     * @param latency 表示每次访问的延迟，目前单位为NS，空参默认值为0
     */
    Memory(sc_module_name name, uint32_t size, bool readonly = false, uint32_t latency = 0)
            : sc_module(name), storage(size, 0), latency(latency) {
        mem_ready.initialize(true); // 初始内存应该准备好
        SC_THREAD(update);
        sensitive << mem_r << mem_w;
    }

    void update() {
        while (true) {
            // 进入逻辑，ready置零
            wait();
            mem_ready.write(0);

            if(mem_r.read()) {  // 读取
                uint32_t res = read(mem_addr.read());
                mem_output.write(res);

                //For Debug
                std::cout << "Read Memory Address [" << read(mem_addr) << "] and get value " << res << std::endl;

                wait(sc_time(latency, SC_NS));    //TODO: 等待时间。单位未统一，暂时为纳秒
            } else if(mem_w.read()) {   // 写入
                write(mem_addr.read(), mem_input.read());

                //For debug
                std::cout << "Write Memory Address [" << read(mem_addr) << "] with value " << mem_input.read() << std::endl;

                wait(sc_time(latency, SC_NS));    //TODO: 等待时间。单位未统一，暂时为纳秒
            }

            // 处理结束，ready置1
            mem_ready.write(1);
        }
        
    }

    /** 模拟延迟的4Bytes读取
     * 
     * @param addr 地址，接受uint32，未检验对齐
     * @return 返回地址储存的 4Bytes对齐 的值
     */
    uint32_t read(uint32_t addr){
        return storage[addr];
    } 

    /** 写入一个 4Bytes 数据（主存调用）
     * 
     * @param addr 地址，接受uint32，未检验对齐
     * @param data 写入的 4Bytes宽度 的值
     * 
    */
    void write(uint32_t addr, uint32_t data){
        storage[addr] = data;
    }

    /** 返回某地址的某一字节
     * 
     * @param addr 地址，接受uint32，未检验对齐
     * 
     *  */ 
    uint8_t readByte(uint32_t addr){
        return storage[addr] && 0b11111111;
    }            
    
    /** 只修改目标字节，保留其他字节 
     * 
     * @param addr 地址，接受uint32，未检验对齐
     * @param val 待写入的 1Byte宽度 的值
     * */ 
    void writeByte(uint32_t addr, uint8_t val){
        storage[addr] = val;
    }  
};