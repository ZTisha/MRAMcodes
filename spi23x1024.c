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

HEY - IF YOU GET "Could not write SPI mode, ret = -1" THEN MAKE SURE YOU RUN
IT AS sudo <executable-path>
For example:
	sudo ./a.out
*/
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

#include "sram_print.c"

// constants that shouldn't be changed
#define SPI_MEM_READ_CMD 0x03 // the command to read the MRAM at initil address
#define SPI_MEM_WRITE_CMD 0x02 // the command to write to the SRAM chip is 0000_0010
#define SPI_MEM_RDSR_CMD 0x05 // the command to read the status register
//#define SPI_MEM_WREN_CMD 0x06h // the command for Write Enable
//#define SPI_MEM_WRDI_CMD 0x04h // the command for Write Disable
//#define SPI_MEM_WRSR_CMD 0x01h // the command to write values to the status register
//#define SPI_MEM_FREAD_CMD 0x0Bh // the command for fast read data bytes
#define SPI_MEM_DEVICE "/dev/spidev0.0" // we're going to open SPI on bus 0 device 0
//idk what to put here

#define SPI_MEM_NUMBER_OF_BITS 24
#define SPI_MEM_MAX_SPEED_HZ 40000000 // see datasheet
#define SPI_MEM_DELAY_US 0 // delay in microseconds
#define SPI_MEM_MAX_ADDRESS 2097152   //131072

typedef struct spi_ioc_transfer spi_ioc_transfer;

// module properties
uint32_t mode;
uint32_t fd;
uint64_t spi_mem_speed_hz;
spi_ioc_transfer read_transfer;
spi_ioc_transfer write_transfer;

/*
    Useful for debugging
    Prints out the transfer and receive buffers of an ioctl transfer
*/
void print_tx_and_rx(uint8_t * tx, uint8_t * rx, uint16_t size) {
	int i;
        for (i = 0; i < size; i++) {
                printf("Index: %d \tTX: %d \tRX: %d\n", i, tx[i], rx[i]);
        }
}

/*
    Handles ioctl error messages in the read and write functions.
    I don't know why I'm not using this for errors that occur in the
    init function. Maybe I'll use this function to handle them later.
*/
void handle_message_response(int ret) {
    if (ret <= 0) {
		char buffer[256];
		strerror_r(errno, buffer, 256);
		printf("Error! SPI failed\t %s\n", buffer);
    }
}

/*
    Initialize the spimem module
    Pass in a speed in HZ, which cannot be above SPI_MEM_MAX_SPEED_HZ
*/
void spi_mem_init(uint64_t speed) {

    assert(speed < SPI_MEM_MAX_SPEED_HZ);

    spi_mem_speed_hz = speed;

	// open device
    fd = open(SPI_MEM_DEVICE, O_RDWR);
	if (fd < 0) {
		printf("Could not open\n");
		exit(EXIT_FAILURE);
	}


	// assign the mode - Mode 0 means (1) data is shifted in on the Rising
	// edge and shifted out on the falling edge, in accordance with
	// the SRAM device description, and (2) clock polarity is low in the idle state
	mode |= SPI_MODE_0;

	int ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode);
	if (ret != 0) {
		printf("Could not write SPI mode, ret = %d\n", ret);
		close(fd);
		exit(EXIT_FAILURE);
	}

	
	// for SPI_MODE_n, the value of "mode" becomes n + 4 for whatever
	// reason after the line below. A forum post says this didn't
	// appear to be an issue for them
	ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode);
	if (ret != 0) {
		printf("Could not read mode\n");
		close(fd);
		exit(EXIT_FAILURE);
	}

	// spi_speed_hz needs to be declared so a pointer can be passed to the calls to ioctl(). ioctl() won't accept constants because constants don't have pointers.
	//uint32_t spi_speed_hz = SPI_SPEED_HZ;
	// assign the speed of ioctl
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_mem_speed_hz);
	if (ret != 0) {
		printf("Could not write the SPI max speed...\r\n");
		close(fd);
		exit(EXIT_FAILURE);
	}
	
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_mem_speed_hz);
	if (ret != 0) {
		printf("Could not read the SPI max speed...\r\n");
		close(fd);
		exit(EXIT_FAILURE);
	}

	
}


/*
    Closes the spi connection. Use this once you're done with SPI.
*/
void spi_mem_close() {
    close(fd);
}

/*
	Reads the status register of the chip
*/
uint8_t spi_mem_read_status_reg() {
	// initialize transmission and receive buffers
	uint8_t tx_buffer[3]; //???
	uint8_t rx_buffer[3];
	int i;
	for (i = 0; i < 3; i++) {               //!!!
		tx_buffer[i] = 0x00;
		rx_buffer[i] = 0xFF;
	}

	// populate transmission buffer with the (1) SRAM command, (2) address (split among 2 bytes)
    tx_buffer[0] = SPI_MEM_RDSR_CMD;

	// configure transmission
    read_transfer.tx_buf = (unsigned long) tx_buffer;
    read_transfer.rx_buf = (unsigned long) rx_buffer;
    read_transfer.bits_per_word = SPI_MEM_NUMBER_OF_BITS;
    read_transfer.speed_hz = spi_mem_speed_hz;
    read_transfer.delay_usecs = SPI_MEM_DELAY_US;
    read_transfer.len = 3;

	// send transmission
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &read_transfer);
	
	// print if there's an error needed
    handle_message_response(ret);
        
	//print_tx_and_rx(&tx_buffer, &rx_buffer, 4);

	// the byte at the address is expected in rx_buffer[3]
	return rx_buffer[1];
}


/*
    Write a single byte to a 24-bit address
*/
void spi_mem_write_byte(uint32_t addr, uint8_t data) {        //!!!


	uint8_t tx_buffer[5] = {
		SPI_MEM_WRITE_CMD,		// write command
        (uint8_t) (addr >> 16), // upper 8 bits of the address          !!!
		(uint8_t) (addr >> 8),	// middle 8 bits of the address         
		(uint8_t) (addr & 0xFF),	// lower 8 bits of the address
	       	data			// data byte
	};		

	uint8_t rx_buffer[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};      //!!!
	


	write_transfer.tx_buf = (unsigned long) tx_buffer;
        write_transfer.rx_buf = (unsigned long) rx_buffer;
        write_transfer.bits_per_word = SPI_MEM_NUMBER_OF_BITS;
        write_transfer.speed_hz = spi_mem_speed_hz;
        write_transfer.delay_usecs = SPI_MEM_DELAY_US;
        write_transfer.len = 5;                             //!!!

	uint32_t ret = ioctl(fd, SPI_IOC_MESSAGE(1), &write_transfer);
    handle_message_response(ret);

	//print_tx_and_rx(&tx_buffer, &rx_buffer, 32);
}


/*
    Read a single byte at a 24-bit address
*/
uint8_t spi_mem_read_byte(uint32_t addr) {
	
	// initialize transmission and receive buffers
	uint8_t tx_buffer[5];
	uint8_t rx_buffer[5];
	int i;
	for (i = 0; i < 5; i++) {
		tx_buffer[i] = 0x00;
		rx_buffer[i] = 0xFF;
	}

	// populate transmission buffer with the (1) SRAM command, (2) address (split among 2 bytes)
    tx_buffer[0] = SPI_MEM_READ_CMD;
    tx_buffer[1] = (uint8_t) (addr >> 16);  // upper third of address               !!!
    tx_buffer[2] = (uint8_t) (addr >> 8); // middle third of address
    tx_buffer[3] = (uint8_t) (addr & 0xFF); // lower third of address


	// configure transmission
    read_transfer.tx_buf = (unsigned long) tx_buffer;
    read_transfer.rx_buf = (unsigned long) rx_buffer;
    read_transfer.bits_per_word = SPI_MEM_NUMBER_OF_BITS;
    read_transfer.speed_hz = spi_mem_speed_hz;
    read_transfer.delay_usecs = SPI_MEM_DELAY_US;
    read_transfer.len = 5;                                              //!!!

	// send transmission
	int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &read_transfer);
	
	// print if there's an error needed
    handle_message_response(ret);
        
	//print_tx_and_rx(&tx_buffer, &rx_buffer, 4);

	// the byte at the address is expected in rx_buffer[4]
	return rx_buffer[4];
 


}
