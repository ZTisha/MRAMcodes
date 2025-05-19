#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "spi_dualchip.c"

#define MRAM_SIZE 131072         // 128KB
#define CHIP1_CS_GPIO 17
#define CHIP2_CS_GPIO 27

uint8_t mram_data[MRAM_SIZE];    // Buffer for full MRAM content

// Load image.csv into memory buffer
int load_image_csv(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening CSV");
        return 1;
    }

    char line[64];
    while (fgets(line, sizeof(line), file)) {
        uint32_t address;
        uint8_t value;

        if (sscanf(line, "%u,0x%hhx", &address, &value) == 2) {
            if (address < MRAM_SIZE) {
                mram_data[address] = value;
            } else {
                printf("Warning: Address 0x%X out of range. Skipped.\n", address);
            }
        } else {
            printf("Malformed line: %s", line);
        }
    }

    fclose(file);
    return 0;
}

// Write full buffer to one MRAM chip
void write_to_chip(int cs_gpio, const char* label) {
    printf("Writing to %s (CS GPIO %d)...\n", label, cs_gpio);
    spi_mem_init(10000000, cs_gpio);  // 10 MHz
    for (uint32_t addr = 0; addr < MRAM_SIZE; addr++) {
        spi_mem_write_byte(addr, mram_data[addr]);
    }
    spi_mem_close();
    printf("Done writing to %s\n", label);
}

int main() {
    if (load_image_csv("image.csv") != 0) {
        printf("Write operation failed.\n");
        return 1;
    }

    write_to_chip(CHIP1_CS_GPIO, "MRAM Chip 1");
    write_to_chip(CHIP2_CS_GPIO, "MRAM Chip 2");

    return 0;
}
