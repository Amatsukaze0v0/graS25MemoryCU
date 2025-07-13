#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "rahmenprogramm.h"

#define DEFAULT_CYCLES 100000
#define DEFAULT_LATENCY_ROM 1
#define DEFAULT_ROM_SIZE 0x100000
#define DEFAULT_BLOCK_SIZE 0x1000 // Both examples from pdf data

void print_help(const char* prog_name) {
    fprintf(stderr, "Usage: %s [options] <input_file>\n\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --cycles <num>           Number of cycles (default: %d)\n", DEFAULT_CYCLES);
    fprintf(stderr, "  --tf <string>            Path to the Tracefile\n");
    fprintf(stderr, "  --latency-rom <num>      Latency of ROM (default: %d)\n", DEFAULT_LATENCY_ROM);
    fprintf(stderr, "  --rom-size <num>         Size of ROM in Bytes (default: %#x)\n", DEFAULT_ROM_SIZE);
    fprintf(stderr, "  --block-size <num>       Size of Memory-block in Bytes (default: %#x)\n", DEFAULT_BLOCK_SIZE);
    fprintf(stderr, "  --rom-content <string>   Path to the content of ROM\n");
    fprintf(stderr, "  --help                   Show this help message\n");
}

int parse_arguments(int argc, char* argv[], MemConfig *config) {

    int opt;
    int option_index=0;
    
    static struct option long_options[] = {
        {"cycles", required_argument, 0, 'c'},
        {"tf", required_argument, 0, 't'},
        {"latency-rom", required_argument, 0, 'l'},
        {"rom-size", required_argument, 0, 's'},
        {"block-size", required_argument, 0, 'b'},
        {"rom-content", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Set default values
    config->cycles=DEFAULT_CYCLES;
    config->block_size=DEFAULT_BLOCK_SIZE;
    config->inputfile=NULL;
    config->latency_rom=DEFAULT_LATENCY_ROM;
    config->rom_content_file=NULL;
    config->rom_size=DEFAULT_ROM_SIZE;
    config->tracefile=NULL;
    
    while ((opt = getopt_long(argc, argv, "c:t:l:s:b:r:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config->cycles=atoi(optarg);
                break;
            case 't':
                config->tracefile = optarg;
                break;
            case 'l':
                config->latency_rom=atoi(optarg);
                break;
            case 's':
                config->rom_size=atoi(optarg);
                break;
            case 'b':
                config->block_size=atoi(optarg);
                break;
            case 'r':
                config->rom_content_file = optarg;
                break;
            case 'h':
                print_help(argv[0]);
                exit(0);
            default:
                print_help(argv[0]);
                exit(0);
        }
    }
    
    if (optind < argc) {
        config->inputfile=argv[optind];
        if(strlen(config->inputfile)<4 || strcmp(config->inputfile + strlen(config->inputfile)-4, ".csv")!=0){
            // Check whether the name or type of inputfile is valid
            fprintf(stderr, "Input file invalid!\n");
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Input file: %s\n", config->inputfile);
    } else {
        fprintf(stderr, "Input file required!\n");
        return 1;
    }
    
    return 0;
}

int parse_number(const char* str, uint32_t* value) {
    // 1. 复制并清除结尾换行和空白
    char clean[256];
    strncpy(clean, str, sizeof(clean));
    clean[sizeof(clean) - 1] = '\0';

    // 去除末尾换行/空格
    size_t len = strlen(clean);
    while (len > 0 && (clean[len - 1] == '\n' || clean[len - 1] == '\r' || clean[len - 1] == ' ' || clean[len - 1] == '\t')) {
        clean[--len] = '\0';
    }

    // 2. 自动判断进制（0x/0X 前缀 → hex）
    char* endptr;
    unsigned long val = strtoul(clean, &endptr, 0);

    // 3. 验证结果是否合法
    if (endptr == clean || *endptr != '\0' || val > UINT32_MAX) {
        return 1;
    }

    *value = (uint32_t)val;
    return 0;
}

uint32_t* load_rom_content(const char* filename, uint32_t rom_size) {
    FILE* file=fopen(filename, "r");
    if(!file){
        fprintf(stderr, "Cannot open ROM-content file: %s\n", filename);
        return NULL;
    }
    uint32_t max_entries=rom_size / sizeof(uint32_t);
    uint32_t* content = (uint32_t*)calloc(max_entries, sizeof(uint32_t));
    if(!content){
        fclose(file);
        return NULL;
    }
    char line[256];
    uint32_t count=0;
    while(fgets(line, sizeof(line), file) && count<max_entries) {
        uint32_t value;
        if (parse_number(line, &value)==0) {
            content[count++]=value;
        }
    }
    if(count>max_entries){
        fprintf(stderr, "Error: Content in ROM has greater size than ROM size!\n");
        free(content);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return content;
}

int parse_csv_file(const char* filename, struct Request** requests, uint32_t* num_requests) {
    FILE* file=fopen(filename, "r");
    if(!file){
        fprintf(stderr, "Cannot open CSV file: %s\n", filename);
        return 1;
    }
    char line[256];
    if(!fgets(line, sizeof(line), file)){
        fprintf(stderr, "Error: CSV file empty!\n");
        fclose(file);
        return 1;
    }
    
    uint32_t line_count=0;
    while(fgets(line, sizeof(line), file)){ // Count total number of rows
        line_count++;
    }
    rewind(file);
    fgets(line, sizeof(line), file); // Skip the table head (First row)
    
    *requests = (struct Request*)malloc(line_count * sizeof(struct Request));
    if(!*requests){
        fclose(file);
        return 1;
    }
    *num_requests = 0;
    while(fgets(line, sizeof(line), file) && *num_requests < line_count){
        char* token;
        char* rest = line;
        char* fields[5] = {NULL};
        int field_count = 0;

        // 分隔5个字段（最多）
        while ((token = strtok_r(rest, ",", &rest)) && field_count < 5) {
            // 去掉首尾引号
            if (token[0] == '"') token++;
            char* end = token + strlen(token) - 1;
            if (*end == '"') *end = '\0';
            fields[field_count++] = token;
        }

        if (field_count < 4) continue; // 不合法的行

        // type (fields[0])
        (*requests)[*num_requests].w = (fields[0][0] == 'W' || fields[0][0] == 'w') ? 1 : 0;

        // address (fields[1])
        uint32_t addr;
        if (parse_number(fields[1], &addr) != 0) continue;
        (*requests)[*num_requests].addr = addr;

        // data (fields[2]) —— 仅在 write 时使用
        uint32_t data = 0;
        if ((*requests)[*num_requests].w) {
            if (fields[2] == NULL || strlen(fields[2]) == 0) continue;  // write must have data
            if (parse_number(fields[2], &data) != 0) continue;
        }
        (*requests)[*num_requests].data = data;

        // user (fields[3])
        uint32_t user;
        if (parse_number(fields[3], &user) != 0) continue;
        (*requests)[*num_requests].user = (uint8_t)user;

        // wide (fields[4])
        if (field_count >= 5) {
            (*requests)[*num_requests].wide = (fields[4][0] == 'T' || fields[4][0] == 't') ? 1 : 0;
        } else {
            (*requests)[*num_requests].wide = 0; // 默认非宽模式
        }

        (*num_requests)++;
    }
    fclose(file);
    return 0;
}

int main(int argc, char* argv[]){
    MemConfig config;
    struct Request* requests = NULL;
    uint32_t num_requests = 0;
    uint32_t* rom_content = NULL;

    if(parse_arguments(argc, argv, &config)!=0){
        return 1;
    }

    if(config.rom_content_file!=NULL){
        rom_content = load_rom_content(config.rom_content_file, config.rom_size);
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
