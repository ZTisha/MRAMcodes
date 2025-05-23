#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "spi_dualchip.c"  // Uses hardware chip select (spidev0.0, spidev0.1)

#define MRAM_SIZE 131072         // 128 KB per chip
#define SPI_DEV1 "/dev/spidev0.0"
#define SPI_DEV2 "/dev/spidev0.1"

// Read one MRAM chip and write to its own CSV file
void read_chip_to_file(const char *device, const char *label, const char *filename) {
    printf("Reading from %s...\n", label);
    FILE *csv = fopen(filename, "w");
    if (!csv) {
        perror("Failed to open output file");
        return;
    }

    // CSV header
    fprintf(csv, "Address,Word\n");

    // Open SPI device and read
    spi_open(device);
    for (uint32_t addr = 0; addr < MRAM_SIZE; addr++) {
        uint8_t value = mram_read_byte(addr);
        fprintf(csv, "%05X,%02X\n", addr, value);  // Address in hex, 5 digits; data in 2-digit hex
    }
    spi_close();
    fclose(csv);

    printf("Data from %s written to %s\n", label, filename);
}

int main() {
    // Read from each chip and write to separate files
    read_chip_to_file(SPI_DEV1, "MRAM Chip 1", "test_chip1.csv");
    read_chip_to_file(SPI_DEV2, "MRAM Chip 2", "test_chip2.csv");
    return 0;
}
