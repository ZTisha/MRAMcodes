#include "spi_init.c"

#define CMD_WREN   0x06  // Write Enable
#define CMD_WRITE  0x02  // Write Data Bytes

// Writes a byte to MRAM at a given 24-bit address
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
    if (spi_init() < 0)
        return 1;

    uint32_t address = 0x000100;  // Any 24-bit address
    uint8_t value = 0xAB;

    printf("Writing 0x%02X to MRAM at 0x%06X\n", value, address);
    mram_write_byte(address, value);

    spi_close();
    return 0;
}
