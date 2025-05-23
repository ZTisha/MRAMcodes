// Write contents of image.csv to two MR10Q010 MRAM chips
// Uses hardware chip selects with spi_dualchip.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "spi2chips.c" // Provides SPI functions for MRAM access

#define MRAM_SIZE 131072               // 128KB MRAM size
#define CSV_FILE "image.csv"           // Input CSV file name

// Load address-data pairs from CSV file into memory buffer
int load_csv(uint8_t *buffer, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening CSV file");
        return 1;
    }

    char line[64];
    while (fgets(line, sizeof(line), file)) {
        uint32_t addr;
        uint8_t value;

        // Parse each line in format: address,0xDATA
        if (sscanf(line, "%u,0x%hhx", &addr, &value) == 2) {
            if (addr < MRAM_SIZE) {
                buffer[addr] = value; // Store parsed value into buffer
            } else {
                fprintf(stderr, "Skipping out-of-bounds address 0x%X\n", addr);
            }
        } else {
            fprintf(stderr, "Malformed line: %s", line);
        }
    }

    fclose(file);
    return 0;
}

// Write the buffer contents to a single MRAM chip
void write_chip(const char *device, const uint8_t *buffer, const char *label) {
    printf("Writing to %s (%s)\n", label, device);
    spi_open(device); // Open SPI device with hardware CS

    for (uint32_t addr = 0; addr < MRAM_SIZE; addr++) {
        mram_write_byte(addr, buffer[addr]); // Write each byte sequentially
    }

    spi_close(); // Close SPI connection
    printf("Done writing to %s\n", label);
}

int main() {
    uint8_t buffer[MRAM_SIZE] = {0}; // Allocate and zero-initialize MRAM buffer

    // Load data from CSV into buffer
    if (load_csv(buffer, CSV_FILE) != 0) {
        fprintf(stderr, "Failed to load input CSV\n");
        return 1;
    }

    // Write data to both chips using separate SPI devices
    write_chip(SPI_DEV1, buffer, "MRAM Chip 1");
    write_chip(SPI_DEV2, buffer, "MRAM Chip 2");

    return 0;
}
