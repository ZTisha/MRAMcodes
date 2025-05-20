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
#define SPI_MEM_WREN_CMD  0x06
#define SPI_MEM_WRITE_CMD 0x02
#define SPI_MEM_READ_CMD  0x03
#define SPI_MEM_RDSR_CMD  0x05

// Global variables
uint32_t mode = 0;
int fd;
uint64_t spi_mem_speed_hz;
int current_cs_gpio = -1; // 🔄 Track active chip-select GPIO

// 🔄 GPIO helper functions
void gpio_export(int gpio) {
    char path[32];
    int f = open("/sys/class/gpio/export", O_WRONLY);
    if (f < 0) return;
    snprintf(path, sizeof(path), "%d", gpio);
    write(f, path, strlen(path));
    close(f);
    usleep(100000);
}

void gpio_set_direction(int gpio, const char *dir) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    int f = open(path, O_WRONLY);
    if (f < 0) return;
    write(f, dir, strlen(dir));
    close(f);
}

void gpio_write(int gpio, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int f = open(path, O_WRONLY);
    if (f < 0) return;
    write(f, value ? "1" : "0", 1);
    close(f);
}

// 🔄 Accept chip-select GPIO
void spi_mem_init(uint64_t speed, int cs_gpio) {
    assert(speed <= SPI_MEM_MAX_SPEED_HZ);
    spi_mem_speed_hz = speed;
    current_cs_gpio = cs_gpio;

    // Export and configure CS GPIO
    gpio_export(cs_gpio);
    gpio_set_direction(cs_gpio, "out");
    gpio_write(cs_gpio, 1); // Deselect initially

    // Open SPI device
    fd = open(SPI_MEM_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Could not open SPI device");
        exit(EXIT_FAILURE);
    }

    // Set SPI mode
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

void spi_mem_close() {
    if (fd >= 0) close(fd);
    if (current_cs_gpio >= 0) gpio_write(current_cs_gpio, 1);
}

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

    gpio_write(current_cs_gpio, 0); // 🔄 MODIFIED
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Status Register read failed");
    }
    gpio_write(current_cs_gpio, 1); // 🔄 MODIFIED

    return rx_buffer[1];
}


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

    // 🔄 MODIFIED: Wrap SPI with GPIO CS control
    gpio_write(current_cs_gpio, 0);
    usleep(5); //added delay before SPI transfer
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Write Enable command failed");
        gpio_write(current_cs_gpio, 1);
        return;
    }
    usleep(5);  //delay before deasserting CS
    gpio_write(current_cs_gpio, 1);

    usleep(10);   //delay before Reading status (WEL Latch)
    

    uint8_t status = spi_mem_read_status_reg();
    if (!(status & 0x02)) {
        printf("Warning: Write Enable Latch (WEL) not set\n");
    }
}

void spi_mem_write_byte(uint32_t addr, uint8_t data) {
    if (addr > SPI_MEM_MAX_ADDRESS) {
        printf("Error: Address 0x%06X out of range\n", addr);
        return;
    }

    spi_mem_write_enable();

    uint8_t tx_buffer[5] = {
        SPI_MEM_WRITE_CMD,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
        data
    };

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = 0,
        .len = 5,
        .speed_hz = spi_mem_speed_hz,
        .delay_usecs = SPI_MEM_DELAY_US,
        .bits_per_word = 8,
    };

    gpio_write(current_cs_gpio, 0); // 🔄 MODIFIED
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        perror("Error: Write operation failed");
    }
    gpio_write(current_cs_gpio, 1); // 🔄 MODIFIED
}

uint8_t spi_mem_read_byte(uint32_t addr) {
    if (addr > SPI_MEM_MAX_ADDRESS) {
        printf("Error: Address 0x%06X out of range\n", addr);
        return 0xFF;
    }

    uint8_t tx_buffer[4] = {
        SPI_MEM_READ_CMD,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF)
    };
    uint8_t rx_buffer[1] = {0};

    struct spi_ioc_transfer xfer[] = {
        {
            .tx_buf = (unsigned long)tx_buffer,
            .len = 4,
            .speed_hz = spi_mem_speed_hz,
            .delay_usecs = SPI_MEM_DELAY_US,
            .bits_per_word = 8,
        },
        {
            .rx_buf = (unsigned long)rx_buffer,
            .len = 1,
            .speed_hz = spi_mem_speed_hz,
            .delay_usecs = SPI_MEM_DELAY_US,
            .bits_per_word = 8,
        }
    };

    gpio_write(current_cs_gpio, 0); // 🔄 MODIFIED
    if (ioctl(fd, SPI_IOC_MESSAGE(2), &xfer) < 0) {
        perror("Error: Read operation failed");
    }
    gpio_write(current_cs_gpio, 1); // 🔄 MODIFIED

    return rx_buffer[0];
}
