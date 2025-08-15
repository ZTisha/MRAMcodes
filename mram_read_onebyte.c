#include "spi_init.c"

#define CMD_READ  0x03  // Read Data Bytes (SPI mode)

// Reads a byte from MRAM at a given 24-bit address
uint8_t mram_read_byte(uint32_t addr) {
    uint8_t tx_buf[4];
    uint8_t rx_buf[5] = {0};

    tx_buf[0] = CMD_READ;
    tx_buf[1] = (addr >> 16) & 0xFF;
    tx_buf[2] = (addr >> 8) & 0xFF;
    tx_buf[3] = addr & 0xFF;

    // Send command + 3-byte address, read 1 byte
    struct spi_ioc_transfer tr[2] = {
        {
            .tx_buf = (unsigned long)tx_buf,
            .rx_buf = 0,
            .len = 4,
            .speed_hz = SPI_SPEED,
            .bits_per_word = SPI_BITS,
            .cs_change = 0
        },
        {
            .tx_buf = 0,
            .rx_buf = (unsigned long)&rx_buf[4],
            .len = 1,
            .speed_hz = SPI_SPEED,
            .bits_per_word = SPI_BITS,
            .cs_change = 0
        }
    };

    if (ioctl(spi_fd, SPI_IOC_MESSAGE(2), tr) < 1) {
        perror("SPI read failed");
        return 0xFF;
    }

    return rx_buf[4];
}

int main() {
    if (spi_init() < 0)
        return 1;

    uint32_t address = 0x000100;
    uint8_t value = mram_read_byte(address);

    printf("Read from 0x%06X: 0x%02X\n", address, value);

    spi_close();
    return 0;
}
