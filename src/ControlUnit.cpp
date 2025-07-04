#include <systemc.h>

SC_MODULE(ControlUnit){

    sc_clock clk;
    sc_in<uint32_t> addr, wdata, mem_rdata;
    sc_in<bool> r, w, wide, mem_ready;
    sc_in<uint8_t> user;

    sc_out<uint32_t> rdata, mem_addr, mem_wdata;
    sc_out<bool> ready, error, mem_r, mem_w;

    std::vector<uint8_t> mem_owner;   // 设想：用于存储Memory的权限； 约定：与memory size(理论上自定义) // block size等大, 255 表示block未分配

    uint32_t block_size;    // 传入的设定，块大小

    // TODO：实现








    /*  // Memory的对齐检查逻辑

    if (wide && (addr % 4 != 0)) {
        // 非对齐访问
        rdata.write(0);      // 无意义的数据
        error.write(true);   // 设置错误标志
        ready.write(true);   // 告诉外部：这次访问“完成”了（尽管是失败）
        return;
    }

    // 用户数组初始化
    mem_owner(size, 255)；

    // 权限检查逻辑
    if (r.read()) {
        uint32_t address = addr.read();
        if(checkAccess(address, user.read)) {
            // 可读
        } else {
            ready.write(1);
            error.write(1);
        }
    } 
    // 其他.....
    */

    /** 查询占有某地址所在块的用户
     * 
     * @param address 查询地址
     */
    uint8_t getOwner(uint32_t address) {
        return mem_owner[address / block_size]; // 索引：block编号（address / block_size）
    }
    
    /** 检查用户是否能访问地址
     * 
     * @param address 待检查的地址
     * @param user 当前准备访问块的用户
     * @return True可访问，False不可访问
     */
    bool checkAccess(uint32_t address, uint8_t user) {
        return user == 0 || user == 255 || user == mem_owner[address / block_size];
    }

    /** 在写入后更新 block 所有权
     * 
     * @param address 查询地址
     * @param user 当前写入的用户
     */
    void assignOwner(uint32_t address, uint8_t user) {
        mem_owner[address / block_size] = user;
    }

    /** 仅用户255调用，用于释放块归属使其他用户访问
     * 
     * @param address 查询地址
     */
    void releaseOwner(uint32_t address) {
        mem_owner[address / block_size] = 255;
    }             
};