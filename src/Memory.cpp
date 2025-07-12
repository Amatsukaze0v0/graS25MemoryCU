#include <systemc.h>

SC_MODULE(MEMORY) {

    sc_in<uint32_t> mem_wdata, mem_addr;
    sc_in<bool> mem_r, mem_w;
    sc_out<uint32_t> mem_rdata;    // 同理，output == rdata
    sc_out<bool> mem_ready;

    std::vector<sc_uint<32>> storage;
    uint32_t latency;
    /* bool is_read_only;  // ROM 只读标志位，约定true为只读 */

    SC_HAS_PROCESS(MEMORY);

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
     * @param latency 表示每次访问的延迟，目前单位为NS，空参默认值为0
     */
    /*说明：
    只读标志位没有必要进行了删除
    size参数进行了删除，不能提前确定要多大，在下面的写入读取添加了判断方法防止越界
     */
    MEMORY(sc_module_name name, uint32_t latency = 0)
            : sc_module(name),latency(latency) {
        mem_ready.initialize(true); // 初始内存应该准备好
        SC_METHOD(update);
        sensitive << mem_r << mem_w;
    }

    void update() {
        while (true) {
            // 进入逻辑，ready置零
            wait();
            mem_ready.write(0);

            if(mem_r.read()) {  // 读取
                uint32_t res = read(mem_addr.read());
                mem_rdata.write(res);

                //For Debug
                std::cout << "Read Memory Address [" << read(mem_addr) << "] and get value " << res << std::endl;

                wait(sc_time(latency, SC_NS));
            } else if(mem_w.read()) {   // 写入
                write(mem_addr.read(), mem_wdata.read());

                //For debug
                std::cout << "Write Memory Address [" << read(mem_addr) << "] with value " << mem_wdata.read() << std::endl;

                wait(sc_time(latency, SC_NS));
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
        //TODO:如果读到了没有写过的地址怎么办？目前的权限控制对没有写过(没有绑定主人)的地址是放行了的没有检查是不是空地址
        return storage[addr];
    } 

    /** 写入一个 4Bytes 数据（主存调用）
     * 
     * @param addr 地址，接受uint32，未检验对齐
     * @param data 写入的 4Bytes宽度 的值
     * 
    */
    void write(uint32_t addr, uint32_t data){
        if (addr >= storage.size()) {
            //如果不够了每次扩容128
            uint32_t new_size = ((addr + 1 + 127) / 128) * 128;
            storage.resize(new_size, 0);
        }
        storage[addr] = data;
    }

    //下面这两个方法是拿来干什么的？

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