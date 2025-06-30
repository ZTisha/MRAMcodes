#include <stdio.h>
#include <stdint.h>
#include "spi_init.c"

#define CMD_READ  0x03
#define MEM_SIZE  131072   // 1 Mbit = 131,072 bytes

uint8_t mram_read_byte(uint32_t addr) {
    uint8_t tx[5] = {0};
    uint8_t rx[5] = {0};

    tx[0] = CMD_READ;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;

    // Transfer command + address and read 1 byte
    if (spi_transfer(tx, rx, 5) < 1) {
        perror("SPI read failed");
        return 0xFF;
    }

    return rx[4];
}

int main() {
    if (spi_init() < 0) return 1;

    FILE *fp = fopen("test.csv", "w");
    if (!fp) {
        perror("Could not open output file");
        spi_close();
        return 1;
    }

    fprintf(fp, "Address,Word\n");

    for (uint32_t addr = 0; addr < MEM_SIZE; addr++) {
        uint8_t data = mram_read_byte(addr);
        fprintf(fp, "%06X,%02X\n", addr, data);
    }

    fclose(fp);
    spi_close();

    printf("MRAM dump complete: test.csv\n");
    return 0;
}
