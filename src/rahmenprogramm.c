#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "rahmenprogramm.h"

#define DEFAULT_CYCLES 100000
#define DEFAULT_LATENCY_ROM 1
#define DEFAULT_ROM_SIZE 0x100000
#define DEFAULT_BLOCK_SIZE 0x1000 // Both examples from pdf data

void print_help(const char *prog_name)
{
    fprintf(stderr, "Verwendung: %s [Optionen] <Eingabedatei>\n\n", prog_name);
    fprintf(stderr, "Optionen:\n");
    fprintf(stderr, "  --cycles <Zahl>          Anzahl der Zyklen (Standard: %d)\n", DEFAULT_CYCLES);
    fprintf(stderr, "  --tf <Zeichenkette>      Pfad zur Trace-Datei\n");
    fprintf(stderr, "  --latency-rom <Zahl>     Latenz der ROM (Standard: %d)\n", DEFAULT_LATENCY_ROM);
    fprintf(stderr, "  --rom-size <Zahl>        Größe der ROM in Bytes (Standard: %#x)\n", DEFAULT_ROM_SIZE);
    fprintf(stderr, "  --block-size <Zahl>      Größe eines Speicherblocks in Bytes (Standard: %#x)\n", DEFAULT_BLOCK_SIZE);
    fprintf(stderr, "  --rom-content <Pfad>     Pfad zum ROM-Inhalt\n");
    fprintf(stderr, "  --help                   Diese Hilfemeldung anzeigen\n");
}

int parse_arguments(int argc, char *argv[], MemConfig *config)
{

    int opt;
    int option_index = 0;

    static struct option long_options[] = {
        {"cycles", required_argument, 0, 'c'},
        {"tf", required_argument, 0, 't'},
        {"latency-rom", required_argument, 0, 'l'},
        {"rom-size", required_argument, 0, 's'},
        {"block-size", required_argument, 0, 'b'},
        {"rom-content", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    // Set default values
    config->cycles = DEFAULT_CYCLES;
    config->block_size = DEFAULT_BLOCK_SIZE;
    config->inputfile = NULL;
    config->latency_rom = DEFAULT_LATENCY_ROM;
    config->rom_content_file = NULL;
    config->rom_size = DEFAULT_ROM_SIZE;
    config->tracefile = NULL;

    while ((opt = getopt_long(argc, argv, "c:t:l:s:b:r:h", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'c':
            config->cycles = atoi(optarg);
            break;
        case 't':
            config->tracefile = optarg;
            break;
        case 'l':
            config->latency_rom = atoi(optarg);
            break;
        case 's':
            config->rom_size = atoi(optarg);
            break;
        case 'b':
            config->block_size = atoi(optarg);
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

    if (optind < argc)
    {
        config->inputfile = argv[optind];
        // Überprüfen, ob der Dateiname gültig ist und die Endung ".csv" hat
        if (strlen(config->inputfile) < 4 || strcmp(config->inputfile + strlen(config->inputfile) - 4, ".csv") != 0)
        {
            // Check whether the name or type of inputfile is valid
            fprintf(stderr, "Eingabedatei ungültig!\n");
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Eingabedatei: %s\n", config->inputfile);
    }
    else
    {
        fprintf(stderr, "Eingabedatei erforderlich!\n");
        return 1;
    }

    return 0;
}

int parse_number(const char *str, uint32_t *value)
{
    // Kopieren und Entfernen von Zeilenumbrüchen und Leerzeichen am Ende
    char clean[256];
    strncpy(clean, str, sizeof(clean));
    clean[sizeof(clean) - 1] = '\0';

    // Entfernen von Zeilenumbruch/Leerzeichen am Ende
    size_t len = strlen(clean);
    while (len > 0 && (clean[len - 1] == '\n' || clean[len - 1] == '\r' || clean[len - 1] == ' ' || clean[len - 1] == '\t'))
    {
        clean[--len] = '\0';
    }

    // Automatische Erkennung des Zahlensystems (Präfix 0x/0X → hexadezimal)
    char *endptr;
    unsigned long val = strtoul(clean, &endptr, 0);

    if (endptr == clean || *endptr != '\0' || val > UINT32_MAX)
    {
        return 1;
    }

    *value = (uint32_t)val;
    return 0;
}

uint32_t *load_rom_content(const char *filename, uint32_t rom_size)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Kann ROM-Inhaltsdatei nicht öffnen: %s\n", filename);
        return NULL;
    }
    if (rom_size % 2 != 0)
    {
        fprintf(stderr, "Fehler: ROM-Größe muss eine Zweierpotenz sein!\n");
        fclose(file);
        return NULL;
    }
    uint32_t max_entries = rom_size / 4;
    uint32_t *content = (uint32_t *)calloc(max_entries, sizeof(uint32_t));
    if (!content)
    {
        fclose(file);
        return NULL;
    }
    char line[256];
    uint32_t count = 0;
    while (fgets(line, sizeof(line), file))
    {
        uint32_t value;
        if (parse_number(line, &value) == 0)
        {
            content[count++] = value;
        }
    }
    if (count > max_entries)
    {
        fprintf(stderr, "Der Inhalt der ROM überschreitet die ROM-Größe!\n");
        free(content);
        fclose(file);
        return NULL;
    }
    else if (count < max_entries)
    {
        while (count != max_entries)
        {
            content[count++] = 0;
        }
    }
    fclose(file);
    return content;
}

bool is_line_empty(const char *line);
int parse_csv_file(const char *filename, struct Request **requests, uint32_t *num_requests)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Kann CSV-Datei nicht öffnen: %s\n", filename);
        return 1;
    }
    char line[256];
    if (!fgets(line, sizeof(line), file))
    {
        fprintf(stderr, "Fehler: CSV-Datei ist leer!\n");
        fclose(file);
        return 1;
    }

    uint32_t line_count = 0;
    while (fgets(line, sizeof(line), file))
    { // Count total number of rows
        line_count++;
    }
    rewind(file);
    fgets(line, sizeof(line), file); // Skip the table head (First row)

    *requests = (struct Request *)malloc(line_count * sizeof(struct Request));
    if (!*requests)
    {
        fclose(file);
        return 1;
    }
    *num_requests = 0;
    uint32_t current_line = 2;
    while (fgets(line, sizeof(line), file) && *num_requests < line_count)
    {
        if (is_line_empty(line))
        {
            fprintf(stderr, "Fehler in Zeile %u: Empty Line\n", current_line);
            goto parse_error;
        }

        char *token;
        char *rest = line;
        char *fields[5] = {NULL};
        int field_count = 0;

        while ((token = strtok_r(rest, ",", &rest)) && field_count < 5)
        {
            if (token[0] == '"')
                token++;
            char *end = token + strlen(token) - 1;
            if (*end == '"')
                *end = '\0';
            fields[field_count++] = token;
        }

        if (field_count != 5)
        {
            fprintf(stderr, "Fehler in Zeile %u: 5 Parameter erwartet, aber %d erhalten\n", current_line, field_count);
            goto parse_error;
        }

        struct Request r;

        // type (fields[0])

        // Type
        if (fields[0][0] == 'W' || fields[0][0] == 'w')
        {
            r.w = 1;
        }
        else if (fields[0][0] == 'R' || fields[0][0] == 'r')
        {
            r.w = 0;
        }
        else
        {
            fprintf(stderr, "Fehler in Zeile %u: Unbekannter Typ '%s'\n", current_line, fields[0]);
            goto parse_error;
        }

        // address (fields[1])
        if (parse_number(fields[1], &r.addr) != 0)
        {
            fprintf(stderr, "Fehler in Zeile %u: Ungültige Adresse '%s'\n", current_line, fields[1]);
            goto parse_error;
        }

        // data (fields[2])
        if (r.w)
        {
            if (fields[2] == NULL || strlen(fields[2]) == 0)
            {
                fprintf(stderr, "Fehler in Zeile %u: Schreibanforderung muss Daten enthalten\n", current_line);
                goto parse_error;
            }
            if (parse_number(fields[2], &r.data) != 0)
            {
                fprintf(stderr, "Fehler in Zeile %u: Ungültige Daten '%s'\n", current_line, fields[2]);
                goto parse_error;
            }
            if (fields[4][0] == 'F' || fields[4][0] == 'f')
            {
                if (r.data > 0xFF)
                {
                    fprintf(stderr, "Fehler in Zeile %u: Narrow-Write-Daten zu groß: 0x%x\n", current_line, r.data);
                    goto parse_error;
                }
            }
        }
        else
        {
            if (fields[2] != NULL && strlen(fields[2]) > 0)
            {
                fprintf(stderr, "Fehler in Zeile %u: Leseanforderung darf keine Daten enthalten\n", current_line);
                goto parse_error;
            }
            r.data = 0;
        }

        // user (fields[3])
        uint32_t user;
        if (parse_number(fields[3], &user) != 0 || user > 255)
        {
            fprintf(stderr, "Fehler in Zeile %u: Ungültiger Benutzer '%s'\n", current_line, fields[3]);
            goto parse_error;
        }
        r.user = (uint8_t)user;

        // wide (fields[4])
        if (fields[4][0] == 'T' || fields[4][0] == 't')
        {
            r.wide = 1;
        }
        else if (fields[4][0] == 'F' || fields[4][0] == 'f')
        {
            r.wide = 0;
        }
        else
        {
            fprintf(stderr, "Fehler in Zeile %u: Ungültiges Wide-Flag '%s'\n", current_line, fields[4]);
            goto parse_error;
        }

        (*requests)[(*num_requests)++] = r;
        current_line++;
        continue;

    parse_error:
        free(*requests);
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}

bool is_line_empty(const char *line)
{
    while (*line)
    {
        if (!isspace((unsigned char)*line))
            return false;
        line++;
    }
    return true;
}

int main(int argc, char *argv[])
{
    MemConfig config;
    struct Request *requests = NULL;
    uint32_t num_requests = 0;
    uint32_t *rom_content = NULL;

    if (parse_arguments(argc, argv, &config) != 0)
    {
        return 1;
    }

    if (config.rom_content_file != NULL)
    {
        rom_content = load_rom_content(config.rom_content_file, config.rom_size);
        if (rom_content == NULL)
        {
            fprintf(stderr, "Fehler beim Laden des ROM-Inhalts.\n");
            return EXIT_FAILURE;
        }
    }

    if (parse_csv_file(config.inputfile, &requests, &num_requests) != 0)
    {
        fprintf(stderr, "Fehler beim Parsen der CSV-Datei.\n");
        if (rom_content != NULL)
        {
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
        requests);

    printf("\n --- Simulation beendet --- \n");
    printf("Zyklen: %u\n", result.cycles);
    printf("Fehler: %u\n", result.errors);

    if (rom_content != NULL)
        free(rom_content);
    if (requests != NULL)
        free(requests);
    return 0;
}
