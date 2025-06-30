#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEV "/dev/spidev0.0"
#define SPI_MODE SPI_MODE_0
#define SPI_BITS 8
#define SPI_SPEED 10000000  // 10 MHz

int spi_fd = -1;

int spi_init() {
    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED;

    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) == -1 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("Failed to set SPI parameters");
        return -1;
    }

    return 0;
}

void spi_close() {
    if (spi_fd >= 0)
        close(spi_fd);
}

int spi_transfer(uint8_t *tx, uint8_t *rx, size_t len) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .speed_hz = SPI_SPEED,
        .bits_per_word = SPI_BITS,
        .cs_change = 0,
        .delay_usecs = 0,
    };

    return ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}
