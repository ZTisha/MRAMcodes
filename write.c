#include "spi23x1024.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Constants
#define SPI_MEM_MAX_ADDRESS 131071  // 17-bit address space (131,072 words)
#define SPI_SPEED_HZ 10000000  // 10 MHz

// Function to write data from CSV file to MRAM
void spi_mem_write_from_csv(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Could not open file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        uint32_t address;
        uint8_t data;
        if (sscanf(line, "%d,0x%hhx", &address, &data) == 2) {
            if (address <= SPI_MEM_MAX_ADDRESS) {
                spi_mem_write_byte(address, data);
                printf("Wrote 0x%02X to address %d\n", data, address);
            } else {
                printf("Address %d out of range\n", address);
            }
        }
    }

    fclose(file);
}

// Main function
int main() {
    // Initialize SPI
    spi_mem_init(SPI_MEM_MAX_SPEED_HZ);

    // Write data from CSV file to MRAM
    spi_mem_write_from_csv("image.csv");

    // Close SPI
    spi_mem_close();

    return 0;
}
