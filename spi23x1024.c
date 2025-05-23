/*
General Purpose SPI API for Serial Memory chips that utilize 24 address bits
Modified from a dedicated SPI API for the Microchip 23x640 series SRAM ICs

supports reading and writing in "Byte Operation"
does not support reading status register
does not support "Page Operation" or "Sequential Operation"
does not support writing to status register
Author: Amaar Ebrahim
Email: aae0008@auburn.edu

Modified by: Gaines Odom
Email: gao0006@auburn.edu

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <assert.h>

// Constants
#define SPI_MEM_DEVICE "/dev/spidev0.0" // SPI device
#define SPI_MEM_MAX_SPEED_HZ 40000000   // Max SPI speed (40 MHz)
#define SPI_MEM_DELAY_US 0              // Delay in microseconds
#define SPI_MEM_MAX_ADDRESS 0x1FFFF     // 17-bit valid address range (128 KB)

// MR10Q010 SPI Commands
#define SPI_MEM_WREN_CMD  0x06  // Write Enable
#define SPI_MEM_WRITE_CMD 0x02  // Write Data Bytes
#define SPI_MEM_READ_CMD  0x03  // Read Data Bytes
#define SPI_MEM_RDSR_CMD  0x05  // Read Status Register

// Global variables
uint32_t mode = 0;
int fd;
uint64_t spi_mem_speed_hz;

// Function prototypes
void spi_mem_init(uint64_t speed);
void spi_mem_close();
void spi_mem_write_enable();
void spi_mem_write_byte(uint32_t addr, uint8_t data);
uint8_t spi_mem_read_byte(uint32_t addr);
uint8_t spi_mem_read_status_reg();

// Initialize SPI
void spi_mem_init(uint64_t speed) {
    assert(speed <= SPI_MEM_MAX_SPEED_HZ);
    spi_mem_speed_hz = speed;

    // Open SPI device
    fd = open(SPI_MEM_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Could not open SPI device");
        exit(EXIT_FAILURE);
    }

    // Set SPI mode to Mode 0 (CPOL=0, CPHA=0)
    mode = SPI_MODE_0;
    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0) {
        perror("Could not write SPI mode");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Set SPI speed
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_mem_speed_hz) < 0) {
        perror("Could not write SPI speed");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

// Close SPI
void spi_mem_close() {
    close(fd);
}

// Write Enable command (must be sent before any write)
void spi_mem_write_enable() {
    uint8_t tx_buffer[1] = {SPI_MEM_WREN_CMD};
    
    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = 0,
        .len = 1,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Write Enable command failed");
        return;
    }

    // Optional: Verify WEL is set before writing
    uint8_t status = spi_mem_read_status_reg();
    if (!(status & 0x02)) {  // WEL bit (bit 1) should be set
        printf("Warning: Write Enable Latch (WEL) not set\n");
    }
}

// Write a byte to MRAM
void spi_mem_write_byte(uint32_t addr, uint8_t data) {
    // Check valid address range
    if (addr > SPI_MEM_MAX_ADDRESS) {
        printf("Error: Address 0x%06X out of range\n", addr);
        return;
    }

    // Enable write operations
    spi_mem_write_enable();

    // Prepare write command and address
    uint8_t tx_buffer[5] = {
        SPI_MEM_WRITE_CMD,      // Write command (0x02)
        (uint8_t)(addr >> 16),  // Upper 8 bits of address
        (uint8_t)(addr >> 8),   // Middle 8 bits of address
        (uint8_t)(addr & 0xFF), // Lower 8 bits of address
        data                    // Data byte
    };

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = 0,
        .len = 5,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Write operation failed");
    }
}

// Read a byte from MRAM
uint8_t spi_mem_read_byte(uint32_t addr) {
    // Check valid address range
    if (addr > SPI_MEM_MAX_ADDRESS) {
        printf("Error: Address 0x%06X out of range\n", addr);
        return 0xFF;  // Return invalid data
    }

    uint8_t tx_buffer[4] = {
        SPI_MEM_READ_CMD,       // Read command (0x03)
        (uint8_t)(addr >> 16),  // Upper 8 bits of address
        (uint8_t)(addr >> 8),   // Middle 8 bits of address
        (uint8_t)(addr & 0xFF)  // Lower 8 bits of address
    };

    uint8_t rx_buffer[4] = {0}; // Read buffer

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 4,  // Sending 4 bytes (command + address), receiving 1 byte
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Read operation failed");
    }

    return rx_buffer[3]; // Return the received data byte
}

// Read Status Register
uint8_t spi_mem_read_status_reg() {
    uint8_t tx_buffer[2] = {SPI_MEM_RDSR_CMD, 0x00};
    uint8_t rx_buffer[2] = {0};

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 2,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = 8,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Status Register read failed");
    }

    return rx_buffer[1]; // Correctly return the status register value
}
