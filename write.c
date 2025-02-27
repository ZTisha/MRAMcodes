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
#define SPI_MEM_READ_CMD 0x03       // Read command
#define SPI_MEM_WRITE_CMD 0x02      // Write command
#define SPI_MEM_RDSR_CMD 0x05       // Read Status Register command
#define SPI_MEM_WREN_CMD 0x06       // Write Enable command
#define SPI_MEM_DEVICE "/dev/spidev0.0" // SPI device
#define SPI_MEM_NUMBER_OF_BITS 24   // 24-bit address
#define SPI_MEM_MAX_SPEED_HZ 40000000 // Max SPI speed (40 MHz)
#define SPI_MEM_DELAY_US 0          // Delay in microseconds
#define SPI_MEM_MAX_ADDRESS 131071  // 17-bit address space (131,072 words)

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
void spi_mem_write_from_csv(const char *filename);

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

// Function to write data from CSV file to MRAM
void spi_mem_write_from_csv(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Could not open file %s\n", filename);
        exit(EXIT_FAILURE);
    }

   char line[256];
    // Skip the header line (first line: "Address,Word")
    fgets(line, sizeof(line), file);

    while (fgets(line, sizeof(line), file)) {
        uint32_t address;
        uint8_t data;
        // Parse address (decimal) and data (hex without 0x)
        if (sscanf(line, "%u,%2hhx", &address, &data) == 2) {
            if (address <= SPI_MEM_MAX_ADDRESS) {
                spi_mem_write_byte(address, data);
                printf("Wrote 0x%02X to address %u\n", data, address);
            } else {
                printf("Address %u out of range\n", address);
            }
        } else {
            printf("Failed to parse line: %s", line);
        }
    }

    fclose(file);
}

// Main function
int main() {
    // Initialize SPI
    spi_mem_init(SPI_MEM_MAX_SPEED_HZ);

    // Write data from CSV file to MRAM
    spi_mem_write_from_csv("image.csv");

    // Close SPI
    spi_mem_close();

    return 0;
}
