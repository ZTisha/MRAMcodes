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
#define SPI_MEM_NUMBER_OF_BITS 24       // 24-bit address
#define SPI_MEM_MAX_SPEED_HZ 40000000   // Max SPI speed (40 MHz)
#define SPI_MEM_DELAY_US 0              // Delay in microseconds
#define SPI_MEM_WREN_CMD  0x06  // Write Enable
#define SPI_MEM_WRITE_CMD 0x02  // Write Data Bytes
#define SPI_MEM_READ_CMD  0x03  // Read Data Bytes
#define SPI_MEM_RDSR_CMD  0x05  // Read Status Register


// Global variables
uint32_t mode;
uint32_t fd;
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
    assert(speed < SPI_MEM_MAX_SPEED_HZ);
    spi_mem_speed_hz = speed;

    // Open SPI device
    fd = open(SPI_MEM_DEVICE, O_RDWR);
    if (fd < 0) {
        printf("Could not open SPI device\n");
        exit(EXIT_FAILURE);
    }

    // Set SPI mode to Mode 0
    mode |= SPI_MODE_0;
    int ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
    if (ret != 0) {
        printf("Could not write SPI mode\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Set SPI speed
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_mem_speed_hz);
    if (ret != 0) {
        printf("Could not write SPI speed\n");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

// Close SPI
void spi_mem_close() {
    close(fd);
}

// Write Enable command
void spi_mem_write_enable() {
    uint8_t tx_buffer[1] = {SPI_MEM_WREN_CMD}; // WREN command
    uint8_t rx_buffer[1] = {0xFF};             // Dummy receive buffer

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 1,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = SPI_MEM_NUMBER_OF_BITS,
    };

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
        printf("Error: Write Enable command failed\n");
    }
}

// Write a byte to MRAM
void spi_mem_write_byte(uint32_t addr, uint8_t data) {
    // Enable write operations
    spi_mem_write_enable();

    // Prepare write command and address
    uint8_t tx_buffer[5] = {
        SPI_MEM_WRITE_CMD,          // Write command
        (uint8_t)(addr >> 16),     // Upper 8 bits of address
        (uint8_t)(addr >> 8),      // Middle 8 bits of address
        (uint8_t)(addr & 0xFF),    // Lower 8 bits of address
        data                       // Data byte
    };

    uint8_t rx_buffer[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Dummy receive buffer

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 5,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = SPI_MEM_NUMBER_OF_BITS,
    };

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
        printf("Error: Write operation failed\n");
    }
}

// Read a byte from MRAM
uint8_t spi_mem_read_byte(uint32_t addr) {
    uint8_t tx_buffer[5] = {
        SPI_MEM_READ_CMD,          // Read command
        (uint8_t)(addr >> 16),    // Upper 8 bits of address
        (uint8_t)(addr >> 8),     // Middle 8 bits of address
        (uint8_t)(addr & 0xFF),   // Lower 8 bits of address
        0x00                      // Dummy byte
    };

    uint8_t rx_buffer[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Receive buffer

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 5,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = SPI_MEM_NUMBER_OF_BITS,
    };

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
        printf("Error: Read operation failed\n");
    }

    return rx_buffer[4]; // Return the received data byte
}

// Read Status Register
uint8_t spi_mem_read_status_reg() {
    uint8_t tx_buffer[2] = {SPI_MEM_RDSR_CMD, 0x00}; // RDSR command + dummy byte
    uint8_t rx_buffer[2] = {0xFF, 0xFF};            // Receive buffer

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = 2,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = SPI_MEM_NUMBER_OF_BITS,
    };

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
        printf("Error: Status Register read failed\n");
    }

    return rx_buffer[1]; // Return the status register value
}
