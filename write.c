#include "spi23x1024.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Constants
#define SPI_MEM_MAX_ADDRESS 131071  // 17-bit address space (131,072 words)
#define SPI_MEM_MAX_SPEED_HZ 10000000  // 10 MHz

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

        // Read decimal/hex address and hex data correctly
        if (sscanf(line, "%d,0x%hhx", &address, &data) == 2) {
            if (address > SPI_MEM_MAX_ADDRESS) {  // Address range check
                printf("Error: Address %d (0x%06X) out of range\n", address, address);
                continue;  // Skip this line and move to the next
            }

            spi_mem_write_byte(address, data);
            printf("Wrote 0x%02X to address %d (0x%06X)\n", data, address, address);
        } else {
            printf("Invalid format: %s", line);
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
