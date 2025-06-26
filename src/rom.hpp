#include<systemc>
using namespace sc_core;


#ifndef ROM_H
#define ROM_H

SC_MODULE(ROM) {

    sc_out<bool> ready;

    ROM(sc_module_name name,uint32_t size):sc_module(name) {

    }
     int size() {
        return 1;
    }

    uint32_t read(uint32_t addr) {
        return 0;
    }

};


#endif
