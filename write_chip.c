#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "spi_init.c"

#define CMD_WREN  0x06  // Write Enable
#define CMD_WRITE 0x02  // Write Data Bytes

void mram_write_byte(uint32_t addr, uint8_t data) {
    uint8_t tx_wren[1] = { CMD_WREN };
    spi_transfer(tx_wren, NULL, 1);

    uint8_t tx_buf[5];
    tx_buf[0] = CMD_WRITE;
    tx_buf[1] = (addr >> 16) & 0xFF;
    tx_buf[2] = (addr >> 8) & 0xFF;
    tx_buf[3] = addr & 0xFF;
    tx_buf[4] = data;

    spi_transfer(tx_buf, NULL, 5);
}

int main() {
    if (spi_init() < 0) return 1;

    FILE *fp = fopen("Image.csv", "r");
    if (!fp) {
        perror("Could not open input CSV file");
        spi_close();
        return 1;
    }

    char line[128];
    fgets(line, sizeof(line), fp);  // Skip header

    while (fgets(line, sizeof(line), fp)) {
        uint32_t addr;
        uint8_t data;

        if (sscanf(line, "%x,%hhx", &addr, &data) == 2) {
            mram_write_byte(addr, data);
        }
    }

    fclose(fp);
    spi_close();

    printf("MRAM write complete: Image.csv written to chip\n");
    return 0;
}
