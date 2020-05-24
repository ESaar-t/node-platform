/*
 * Basic JEDEC SPI flash wrapper.
 *
 * Copyright Thinnect Inc. 2019
 * @license MIT
 * @author Veiko Rütter, Raido Pahtma
 */
#include "spi_flash.h"
#include "retargetspi.h"
#include <stdio.h>

#define SPI_FLASH_CS               0
#define SPI_FLASH_PARTITIONS_COUNT 3

static struct{
	uint32_t start;
	uint32_t end;
	uint32_t size;
}spi_flash_partitions[SPI_FLASH_PARTITIONS_COUNT] = {
	{ 0,        0x4000 },
	{ 0x4000,   0x100000},
	{ 0x100000, 0x800000},
};

static int spi_flash_sleeping = 1;

void spi_flash_init(void)
{
	int i;
	spi_flash_resume();
	for (i = 0; i < SPI_FLASH_PARTITIONS_COUNT; i++)
	{
		spi_flash_partitions[i].size = spi_flash_partitions[i].end - spi_flash_partitions[i].start;
	}
}

void spi_flash_suspend(void)
{
	// Put FLASH into deep sleep
	spi_flash_wait_busy();
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, "\xB9", 1, NULL, 0);
	spi_flash_sleeping = 1;
}

void spi_flash_resume(void)
{
	if (!spi_flash_sleeping) return;
	// Wake FLASH chip from deep sleep
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, "\xAB", 1, NULL, 0);
	spi_flash_wait_busy();
	spi_flash_sleeping = 0;
}

void spi_flash_cmd(uint8_t cmd)
{
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, &cmd, 1, NULL, 0);
}

uint8_t spi_flash_status(void)
{
	uint8_t c;
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, "\x05", 1, &c, 1);
	return c;
}

void spi_flash_wait_busy(void)
{
	while(spi_flash_status() & 0x01);
}

void spi_flash_normalize(void)
{
}

void spi_flash_wait_wel(void)
{
	while(!(spi_flash_status() & 0x02));
}

void spi_flash_mass_erase(void)
{
	spi_flash_resume();
	spi_flash_wait_busy();
	spi_flash_cmd(0x06);
	spi_flash_wait_wel();
	spi_flash_cmd(0xC7);
	spi_flash_wait_busy();
	spi_flash_cmd(0x04);
	spi_flash_wait_busy();
}

int32_t spi_flash_read(int partition, uint32_t addr, uint32_t size, uint8_t * dst)
{
	uint8_t buffer[5];
	uint32_t len;

	if (partition >= SPI_FLASH_PARTITIONS_COUNT)
		return -1;

	if (addr > spi_flash_partitions[partition].size)
		return -1;

	spi_flash_resume();

	len = spi_flash_partitions[partition].size - addr;
	if (size > len)
		size = len;

	addr += spi_flash_partitions[partition].start;

	spi_flash_wait_busy();
	buffer[0] = 0x0B;
	buffer[1] = ((addr >> 16) & 0xFF);
	buffer[2] = ((addr >> 8) & 0xFF);
	buffer[3] = ((addr >> 0) & 0xFF);
	buffer[4] = 0xFF;
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, buffer, 5, dst, size);
	return size;
}

int32_t spi_flash_write(int partition, uint32_t addr, uint32_t size, uint8_t * src)
{
	static uint8_t buffer[260];
	uint32_t len;
	uint32_t i;

	if (partition >= SPI_FLASH_PARTITIONS_COUNT)
		return -1;

	if (addr > spi_flash_partitions[partition].size)
		return -1;

	spi_flash_resume();

	len = spi_flash_partitions[partition].size - addr;
	if (size > len)
		size = len;

	addr += spi_flash_partitions[partition].start;

	spi_flash_wait_busy();
	spi_flash_cmd(0x06);
	spi_flash_wait_wel();
	buffer[0] = 0x02;
	buffer[1] = ((addr >> 16) & 0xFF);
	buffer[2] = ((addr >> 8) & 0xFF);
	buffer[3] = ((addr >> 0) & 0xFF);
	if(size > 256)size = 256;
	for(i = 0; i < size; i++){
		buffer[4 + i] = src[i];
	}
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, buffer, 4 + size, NULL, 0);
	spi_flash_wait_busy();
	spi_flash_cmd(0x04);
	spi_flash_wait_busy();
	return size;
}

int32_t spi_flash_erase(int partition, uint32_t addr, uint32_t size) // sector 4KB (0x20), half block 32KB (0x52), block 64KB (0xD8)
{
	uint8_t buffer[4];
	uint32_t len;

	if (partition >= SPI_FLASH_PARTITIONS_COUNT)
		return -1;

	if (addr > spi_flash_partitions[partition].size)
		return -1;

	len = spi_flash_partitions[partition].size - addr;
	if (size > len)
		size = len;

	if (size < 4096)
		return -1;

	addr += spi_flash_partitions[partition].start;

	spi_flash_resume();

	spi_flash_wait_busy();
	spi_flash_cmd(0x06);
	spi_flash_wait_wel();
	buffer[0] = 0x20;
	buffer[1] = ((addr >> 16) & 0xFF);
	buffer[2] = ((addr >> 8) & 0xFF);
	buffer[3] = ((addr >> 0) & 0xFF);
	RETARGET_SpiTransferHalf(SPI_FLASH_CS, buffer, 4, NULL, 0);
	spi_flash_wait_busy();
	spi_flash_cmd(0x04);
	spi_flash_wait_busy();
	return size;
}

int32_t spi_flash_size(int partition)
{
	if (partition >= SPI_FLASH_PARTITIONS_COUNT)
		return -1;
	return(spi_flash_partitions[partition].size);
}

int32_t spi_flash_erase_size(int partition)
{
	if (partition >= SPI_FLASH_PARTITIONS_COUNT)
		return -1;
	return(4096);
}

void spi_flash_lock()
{
	RETARGET_SpiTransactionLock(SPI_FLASH_CS);
}

void spi_flash_unlock()
{
	RETARGET_SpiTransactionUnlock(SPI_FLASH_CS);
}

