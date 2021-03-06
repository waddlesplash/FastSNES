#include <stdlib.h>
#include <string.h>
#include <allegro.h>

#include "snem.h"
#include "util.h"

int pal;
uint16_t srammask;
void initmem();
uint8_t* sram;
void allocmem()
{
	// FILE *f=fopen("finalf~1.srm","rb");
	ram = (uint8_t*)malloc(128 * 1024);
	memset(ram, 0x55, 128 * 1024);
	sram = (uint8_t*)malloc(8192);
	memset(sram, 0, 8192);
	// fread(sram,8192,1,f);
	// fclose(f);
}

void loadrom(char* fn)
{
	int c = 0;
	char name[22];
	char sramname[512];
	FILE* f = fopen(fn, "rb");
	int len;
	uint16_t temp, temp2;
	spccycles = -10000;
	if (!f) {
		printf("File %s not found\n", fn);
		exit(-1);
	}
	fseek(f, -1, SEEK_END);
	len = ftell(f) + 1;
	fseek(f, len & 512, SEEK_SET);
	snemdebug("%i %lu\n", len, ftell(f));
	rom = (uint8_t*)malloc(4096 * 1024);
	// fread(rom,512,1,f);
	/* for (c=0;c<0x40000;c+=0x8000) {
		fread(&rom[c+0x40000],32768,1,f);
		fread(&rom[c],32768,1,f);
	} */
	while (!feof(f) && c < 0x400000) {
		// snemdebug("Read %06X\n",c);
		fread(&rom[c], 65536, 1, f);
		c += 0x10000;
	}
	fclose(f);
	temp = rom[0x7FDC] | (rom[0x7FDD] << 8);
	temp2 = rom[0x7FDE] | (rom[0x7FDF] << 8);
	if ((temp | temp2) == 0xFFFF)
		lorom = 1;
	else
		lorom = 0;
	// lorom=0;
	initmem();
	if (((readmem(0xFFFD) << 8) | readmem(0xFFFC)) == 0xFFFF) {
		lorom ^= 1;
		initmem();
	}

	len = c; //-0x10000;
	for (c = 0; c < 21; c++)
		name[c] = readmem(0xFFC0 + c);
	name[21] = 0;
	snemdebug("ROM name : %s\n", name);
	snemdebug("ROM size : %i megabits (%i kbytes)\n", 1 << (readmem(0xFFD7) - 7),
		   len >> 10);
	srammask = (1 << (readmem(0xFFD8) + 10)) - 1;
	if (!readmem(0xFFD8))
		srammask = 0;
	snemdebug("SRAM size : %i kilobits (%i kbytes) %04X\n",
		   1 << (readmem(0xFFD8) + 3), (1 << (readmem(0xFFD8) + 3)) >> 3,
		   srammask);
	if (readmem(0xFFD9) > 1)
		pal = 1;
	else
		pal = 0;
	snemdebug("Country code : %i (%s)\n", readmem(0xFFD9),
		   (readmem(0xFFD9) > 1) ? "PAL" : "NTSC");
	snemdebug("NMI vector : %02X%02X\n", readmem(0xFFEB), readmem(0xFFEA));
	snemdebug("IRQ vector : %02X%02X\n", readmem(0xFFEF), readmem(0xFFEE));
	snemdebug("Reset vector : %02X%02X\n", readmem(0xFFFD), readmem(0xFFFC));
	snemdebug("Memory map : %s\n", (lorom) ? "LoROM" : "HiROM");
	if (srammask) {
		if ((srammask + 1) > 8192) {
			// Realloc sram.
			free(sram);
			sram = (uint8_t*)malloc(srammask + 1);
		}
		sramname[0] = 0;
		replace_extension(sramname, fn, "srm", 511);
		snemdebug("Load SRAM from %s %s\n", sramname, fn);
		f = fopen(sramname, "rb");
		if (f) {
			fread(sram, srammask + 1, 1, f);
			fclose(f);
		} else
			memset(sram, 0, srammask + 1);
	}
	memset(ram, 0x55, 128 * 1024);
}

void initmem()
{
	int c, d;
	for (c = 0; c < 256; c++) {
		for (d = 0; d < 8; d++) {
			memread[(c << 3) | d] = memwrite[(c << 3) | d] = 0;
		}
	}
	if (lorom) {
		for (c = 0; c < 96; c++) {
			for (d = 0; d < 4; d++) {
				memread[(c << 3) | (d + 4)] = 1;
				memlookup[(c << 3) | (d + 4)] =
					&rom[((d * 0x2000) + (c * 0x8000)) & 0x3FFFFF];
				memread[(c << 3) | (d + 4) | 0x400] = 1;
				memlookup[(c << 3) | (d + 4) | 0x400] =
					&rom[((d * 0x2000) + (c * 0x8000)) & 0x3FFFFF];
			}
		}
		for (c = 0; c < 64; c++) {
			memread[(c << 3) | 0] = memwrite[(c << 3) | 0] = 1;
			memlookup[(c << 3) | 0] = ram;
		}
		for (c = 0; c < 64; c++) {
			memread[(c << 3) | 0x400] = memwrite[(c << 3) | 0x400] = 1;
			memlookup[(c << 3) | 0x400] = ram;
		}
		for (c = 0; c < 8; c++) {
			memread[(0x7E << 3) | c] = memwrite[(0x7E << 3) | c] = 1;
			memlookup[(0x7E << 3) | c] = &ram[c * 0x2000];
			memread[(0x7F << 3) | c] = memwrite[(0x7F << 3) | c] = 1;
			memlookup[(0x7F << 3) | c] = &ram[(c * 0x2000) + 0x10000];
		}
		// snemdebug("%08X\n",memlookup[(0x7F<<3)|4]);
		/*                for (c=0;c<96;c++)
						{
								for (d=0;d<4;d++)
								{
										memread[((c+64)<<3)|(d+4)]=1;
										memlookup[((c+64)<<3)|(d+4)]=&rom[((d*0x2000)+(c*0x8000))&0x3FFFFF];
								}
						}
						for (c=0;c<0x60<<3;c++)
						{
								memread[c+0x400]=memread[c];
								memwrite[c+0x400]=memwrite[c];
								memlookup[c+0x400]=memlookup[c];
						}
						/*for (c=0;c<16;c++)
						{
								memread[(0x70<<3)+c]=memwrite[(0x70<<3)+c]=0;
								memlookup[(0x70<<3)+c]=sram;
						} */
		// snemdebug("%08X\n",memlookup[(0x7F<<3)|4]);
	} else {
		for (c = 0; c < 2048; c++) {
			memread[c] = 1;
			memwrite[c] = 0;
			memlookup[c] = &rom[(c * 0x2000) & 0x3FFFFF];
		}
		for (c = 0; c < 64; c++) {
			for (d = 1; d < 4; d++) {
				memread[(c << 3) + d] = memwrite[(c << 3) + d] = 0;
				memread[(c << 3) + d + 1024] = memwrite[(c << 3) + d + 1024] =
					0;
			}
		}
		for (c = 0; c < 64; c++) {
			memread[(c << 3) | 0] = memwrite[(c << 3) | 0] = 1;
			memlookup[(c << 3) | 0] = ram;
			memread[(c << 3) | 1024] = memwrite[(c << 3) | 1024] = 1;
			memlookup[(c << 3) | 1024] = ram;
		}
		for (c = 0; c < 8; c++) {
			memread[(0x7E << 3) | c] = memwrite[(0x7E << 3) | c] = 1;
			memlookup[(0x7E << 3) | c] = &ram[c * 0x2000];
			memread[(0x7F << 3) | c] = memwrite[(0x7F << 3) | c] = 1;
			memlookup[(0x7F << 3) | c] = &ram[(c * 0x2000) + 0x10000];
		}
		/*                for (c=0;c<0x40<<3;c++)
						{
								memread[c+0x400]=memread[c];
								memwrite[c+0x400]=memwrite[c];
								memlookup[c+0x400]=memlookup[c];
						} */
		for (c = 0; c < 16; c++) {
			memread[(0x70 << 3) + c] = memwrite[(0x70 << 3) + c] = 1;
			memlookup[(0x70 << 3) + c] = sram;
		}
	}
	/* Set up access speed table */
	for (c = 0; c < 64; c++) {
		accessspeed[(c << 3) | 0] = 8;
		accessspeed[(c << 3) | 1] = 6;
		accessspeed[(c << 3) | 2] = 6;
		accessspeed[(c << 3) | 3] = 6;
		accessspeed[(c << 3) | 4] = accessspeed[(c << 3) | 5] = 8;
		accessspeed[(c << 3) | 6] = accessspeed[(c << 3) | 7] = 8;
	}
	for (c = 64; c < 128; c++) {
		for (d = 0; d < 8; d++) {
			accessspeed[(c << 3) | d] = 8;
		}
	}
	for (c = 128; c < 192; c++) {
		accessspeed[(c << 3) | 0] = 8;
		accessspeed[(c << 3) | 1] = 6;
		accessspeed[(c << 3) | 2] = 6;
		accessspeed[(c << 3) | 3] = 6;
		accessspeed[(c << 3) | 4] = accessspeed[(c << 3) | 5] = 8;
		accessspeed[(c << 3) | 6] = accessspeed[(c << 3) | 7] = 8;
	}
	for (c = 192; c < 256; c++) {
		for (d = 0; d < 8; d++) {
			accessspeed[(c << 3) | d] = 8;
		}
	}
}

uint8_t readmeml(uint32_t addr)
{
	addr &= ~0xFF000000;
	if (((addr >> 16) & 0x7F) < 0x40) {
		switch (addr & 0xF000) {
		case 0x2000:
			return readppu(addr);
		case 0x4000:
			if ((addr & 0xE00) == 0x200)
				return readio(addr);
			if ((addr & 0xFFFE) == 0x4016)
				return readjoyold(addr);
			return 0;
			//printf("Bad Read %06X\n", addr);
			//dumpregs();
			//exit(-1);
		case 0x6000:
		case 0x7000:
			// snemdebug("Read SRAM %04X %02X
			// %06X\n",addr,sram[addr&0x1FFF],pbr|pc);
			if (!lorom)
				return sram[addr & srammask];
		default:
			return 0xFF;
			//printf("Bad read %06X\n", addr);
			//dumpregs();
			//exit(-1);
		}
	}
	if ((addr >> 16) >= 0xD0 && (addr >> 16) <= 0xFE)
		return 0;
	if ((addr >> 16) == 0x70) {
		// return 0;
		// snemdebug("Read SRAM %04X
		// %02X\n",addr,sram[addr&0x1FFF]);
		if (srammask) {
			// snemdebug("Read SRAM %04X %04X %02X
			// %04X\n",addr,addr&srammask,sram[addr&srammask],srammask);
			return sram[addr & srammask];
		}
		// snemdebug("Read SRAM zero\n");
		return 0;
	}
	if ((addr >> 16) == 0x60)
		return 0;
	return 0;
	//printf("Bad read %06X\n", addr);
	//dumpregs();
	//exit(-1);
}

void writememl(uint32_t addr, uint8_t val)
{
	addr &= ~0xFF000000;
	if (((addr >> 16) & 0x7F) < 0x40) {
		switch (addr & 0xF000) {
		case 0x2000:
			if ((addr & 0xF00) == 0x100)
				writeppu(addr, val);
			return;
		case 0x3000:
			return;
		case 0x4000:
			if ((addr & 0xE00) == 0x200)
				writeio(addr, val);
			if ((addr & 0xFFFE) == 0x4016)
				writejoyold(addr, val);
			return;
		case 0x5000:
			return;
		case 0x6000:
		case 0x7000:
			// snemdebug("Write SRAM %04X %02X
			// %06X\n",addr,val,pbr|pc);
			if (!lorom)
				sram[addr & srammask] = val;
			return;
		case 0x8000:
		case 0x9000:
		case 0xA000:
		case 0xB000:
		case 0xC000:
		case 0xD000:
		case 0xE000:
		case 0xF000:
			return;
		default:
			printf("Bad write %06X %02X\n", addr, val);
			dumpregs();
			exit(-1);
		}
	}
	if ((addr >> 16) >= 0xD0 && (addr >> 16) <= 0xFE)
		return;
	// if ((addr>>16)==0xD0) return;
	if ((addr >> 16) == 0x70) {
		// snemdebug("Write SRAM %04X %04X
		// %02X\n",addr,addr&srammask,val);
		sram[addr & srammask] = val;
		return;
	}
	if ((addr >> 16) == 0x60)
		return;
	if ((addr >= 0xC00000 && addr < 0xFE0000))
		return;
	if ((addr >= 0x710000 && addr < 0x7E0000))
		return;
	printf("Bad write %06X %02X\n", addr, val);
	dumpregs();
	exit(-1);
}
