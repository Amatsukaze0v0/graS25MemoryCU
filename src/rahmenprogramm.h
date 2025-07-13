#ifndef RAHMENPROGRAMM_H
#define RAHMENPROGRAMM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

struct Result {
    uint32_t cycles;
    uint32_t errors;
};

struct Request {
    uint32_t addr; // Adress to access
    uint32_t data; // Data to write
    uint8_t w; // 1 = write, 0 = read
    uint8_t user; // user-id
    uint8_t wide; // 1 = 4Bytes, 0 = 1Byte
};

typedef struct{
    uint32_t cycles;
    char* tracefile; // Path to Tracfile
    char* inputfile; // Path to input CSV file
    uint32_t latency_rom;
    uint32_t rom_size;
    uint32_t block_size;
    char* rom_content_file; // Path to ROM-Content
} MemConfig;

void print_help(const char* prog_name);

int parse_arguments(int argc, char* argv[], MemConfig *config);

int parse_number(const char* str, uint32_t* value);

uint32_t* load_rom_content(const char* filename, uint32_t rom_size);

int parse_csv_file(const char* filename, struct Request** requests, uint32_t* num_requests);

extern struct Result run_simulation (
    uint32_t cycles,
    const char* tracefile,
    uint32_t latencyRom,
    uint32_t romSize,
    uint32_t blockSize,
    uint32_t* romContent,
    uint32_t numRequests,
    struct Request* requests
);

#ifdef __cplusplus
}
#endif

#endif // RAHMENPROGRAMM_H