// MRAM SPI interface using hardware chip-selects (/dev/spidev0.0 and /dev/spidev0.1)
// Target chip: MR10Q010 (128KB capacity)

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <assert.h>

// Constants
#define MRAM_SIZE 131072               // Total MRAM size in bytes
#define SPI_SPEED_HZ 10000000          // SPI clock speed (10 MHz)

#define SPI_DEV1 "/dev/spidev0.0"     // SPI device for MRAM chip 1
#define SPI_DEV2 "/dev/spidev0.1"     // SPI device for MRAM chip 2

// MRAM command opcodes
#define WREN  0x06                     // Write Enable command
#define WRITE 0x02                     // Write data command
#define READ  0x03                     // Read data command
#define RDSR  0x05                     // Read Status Register command

int fd = -1; // File descriptor for opened SPI device

// Open and initialize SPI device
void spi_open(const char *device) {
    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("SPI open failed");
        exit(EXIT_FAILURE);
    }

    __u32 mode = SPI_MODE_0; // SPI Mode 0 (CPOL=0, CPHA=0)
    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0) {
        perror("SPI mode set failed");
        exit(EXIT_FAILURE);
    }
    __u32 speed = SPI_SPEED_HZ;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("SPI speed set failed");
        exit(EXIT_FAILURE);
    }
}

// Close SPI device
void spi_close() {
    if (fd >= 0) close(fd);
}

// Send Write Enable (WREN) command to MRAM
void mram_write_enable() {
    uint8_t cmd = WREN;
    struct spi_ioc_transfer t = {
        .tx_buf = (unsigned long)&cmd,
        .rx_buf = 0,
        .len = 1,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = 8
    };
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &t) < 0) {
        perror("WREN failed");
    }
// usleep(10); // short wait to ensure WEL is latched
}

// Write a single byte to a given MRAM address
void mram_write_byte(uint32_t addr, uint8_t data) {
    if (addr >= MRAM_SIZE) return; // Ignore out-of-bounds access
    mram_write_enable(); // Enable writing first

    uint8_t tx[5] = {
        WRITE,                      // Write command
        (uint8_t)(addr >> 16),     // Address high byte
        (uint8_t)(addr >> 8),      // Address mid byte
        (uint8_t)(addr),           // Address low byte
        data                       // Data byte
    };

    struct spi_ioc_transfer t = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = 0,
        .len = sizeof(tx),
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = 8
    };
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &t) < 0) {
        perror("Write failed");
    }
}

// Read a single byte from a given MRAM address
uint8_t mram_read_byte(uint32_t addr) {
    if (addr >= MRAM_SIZE) return 0xFF; // Return dummy value if invalid

    uint8_t tx[4] = {
        READ,                      // Read command
        (uint8_t)(addr >> 16),     // Address high byte
        (uint8_t)(addr >> 8),      // Address mid byte
        (uint8_t)(addr)            // Address low byte
    };
    uint8_t rx[1] = {0}; // Buffer to store received data

    struct spi_ioc_transfer t[2] = {
        {
            .tx_buf = (unsigned long)tx,
            .rx_buf = 0,
            .len = 4,
            .speed_hz = SPI_SPEED_HZ,
            .bits_per_word = 8
        },
        {
            .tx_buf = 0,
            .rx_buf = (unsigned long)rx,
            .len = 1,
            .speed_hz = SPI_SPEED_HZ,
            .bits_per_word = 8
        }
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(2), t) < 0) {
        perror("Read failed");
    }

    return rx[0];
}
