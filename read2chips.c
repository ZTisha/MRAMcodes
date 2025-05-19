#include <stdio.h>
#include <stdint.h>
#include "spi_dualchip.c"  // Includes all SPI + GPIO functions

#define MRAM_SIZE 131072
#define CHIP1_CS_GPIO 17
#define CHIP2_CS_GPIO 27

// Reads MRAM content from one chip to a CSV file
void read_chip_to_csv(int cs_gpio, const char* chip_label, const char* output_filename) {
    printf("Reading data from %s (CS GPIO %d)...\n", chip_label, cs_gpio);

    FILE* out = fopen(output_filename, "w");
    if (!out) {
        perror("Failed to open output file");
        return;
    }

    spi_mem_init(10000000, cs_gpio);  // 10 MHz

    for (uint32_t addr = 0; addr < MRAM_SIZE; addr++) {
        uint8_t data = spi_mem_read_byte(addr);
        fprintf(out, "%u,0x%02X\n", addr, data);
    }

    spi_mem_close();
    fclose(out);

    printf("Data from %s written to %s\n", chip_label, output_filename);
}

int main() {
    read_chip_to_csv(CHIP1_CS_GPIO, "MRAM Chip 1", "chip1_data.csv");
    read_chip_to_csv(CHIP2_CS_GPIO, "MRAM Chip 2", "chip2_data.csv");
    return 0;
}
