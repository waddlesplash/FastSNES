/* Writes to VRAM at FC80 from 7E0000,x (x=3000ish)
  Writes to 7E0000,x at FA8A - reads from 7F0D50,x
  8Da9? */
/* Writes to VRAM at FCFC from 7E0000,x (x=5000ish)
  Writes to 7E0000,x at FB55
  8DA9? */
/* Snem 0.1 by Tom Walker
  PPU emulation */

#include <allegro.h>
#include <stdlib.h>

#include "snem.h"
#include "util.h"

#define uint unsigned int
#define uint16 uint16_t
#define uint32 uint32_t

unsigned char voiceon;
void writeppu(uint16_t addr, unsigned char val);

inline uint16 cgadd(uint32 x, uint32 y)
{
	uint sum = x + y;
	uint carries = (sum - ((x ^ y) & 0x0821)) & 0x10820;
	return (sum - carries) | (carries - (carries >> 5));
}

inline uint16 cgaddh(uint32 x, uint32 y)
{
	return (x + y - ((x ^ y) & 0x0821)) >> 1;
}
inline unsigned int cgsub(uint32_t x, uint32_t y)
{
	unsigned int sub = x - y;
	unsigned int borrows = (~(sub + ((~(x ^ y)) & 0x10820))) & 0x10820;
	sub += borrows;
	return sub & ~(borrows - (borrows >> 5));
}
inline uint16 cgsubh(uint32 x, uint32 y)
{
	uint sub = x - y;
	uint borrows = (~(sub + ((~(x ^ y)) & 0x10820))) & 0x10820;
	sub += borrows;
	return ((sub & ~(borrows - (borrows >> 5))) & 0xf79e) >> 1;
}

uint32_t wroteaddr;
int hcount, vcount;
uint32_t window[10][164];
int twowrite = 0;
int windowschanged;
typedef struct {
	unsigned char r, g, b;
} rgb_color;
struct {
	unsigned char screna;
	unsigned char portctrl;
	uint16_t vramaddr;
	uint16_t bg[4], chr[4];
	int mode;
	unsigned char main, sub;
	uint16_t palbuffer;
	uint16_t pal[256];
	int palindex;
	int xscroll[4], yscroll[4];
	int ylatch;
	uint32_t matrixr;
	uint16_t m7a, m7b, m7c, m7d, m7x, m7y;
	unsigned char m7sel;
	int size[4];
	int vinc;
	int sprsize;
	int spraddr;
	uint16_t sprbase;
	int firstread;
	int tilesize;
	uint32_t wramaddr;
	unsigned char windena1, windena2, windena3;
	int w1left, w1right, w2left, w2right;
	unsigned char windlogic, windlogic2;
	unsigned char wmaskmain, wmasksub;
	uint16_t spraddrs;
	unsigned char cgadsub, cgwsel;
	rgb_color fixedc;
	uint16_t fixedcol;
	int mosaic;
	uint16_t pri;
	int prirotation;
} ppu;

BITMAP* b, *mainscr, *subscr, *otherscr, *sysb;
uint32_t bitlookup[2][4][4], masklookup[2][4];
uint16_t bitlookuph[2][4][4], masklookuph[2][4];
uint16_t pallookup[16][256];
uint32_t collookup[16];
unsigned char sprram[544];

void initppu()
{
	int c, d;
	allegro_init();
	set_color_depth(16);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, 512, 448, 0, 0);
	otherscr = create_bitmap(640, 225);
	mainscr = create_bitmap(640, 225);
	subscr = create_bitmap(640, 225);
	sysb = create_system_bitmap(256, 224);
	vramb = (unsigned char*)malloc(0x10000);
	memset(vramb, 0, 0x10000);
	vram = (uint16_t*)vramb;
	for (c = 0; c < 4; c++) {
		for (d = 0; d < 4; d++) {
			bitlookup[0][c][d] = 0;
			if (c & 1)
				bitlookup[0][c][d] |= 0x10000;
			if (d & 1)
				bitlookup[0][c][d] |= 0x20000;
			if (c & 2)
				bitlookup[0][c][d] |= 1;
			if (d & 2)
				bitlookup[0][c][d] |= 2;
			bitlookup[1][c][d] = 0;
			if (c & 2)
				bitlookup[1][c][d] |= 0x10000;
			if (d & 2)
				bitlookup[1][c][d] |= 0x20000;
			if (c & 1)
				bitlookup[1][c][d] |= 1;
			if (d & 1)
				bitlookup[1][c][d] |= 2;
			masklookup[0][c] = 0;
			if (c & 1)
				masklookup[0][c] |= 0xFFFF0000;
			if (c & 2)
				masklookup[0][c] |= 0xFFFF;
			masklookup[1][c] = 0;
			if (c & 2)
				masklookup[1][c] |= 0xFFFF0000;
			if (c & 1)
				masklookup[1][c] |= 0xFFFF;
		}
		for (d = 0; d < 4; d++) {
			bitlookuph[0][c][d] = 0;
			if (c & 2)
				bitlookuph[0][c][d] |= 1;
			if (d & 2)
				bitlookuph[0][c][d] |= 2;
			bitlookuph[1][c][d] = 0;
			if (c & 1)
				bitlookuph[1][c][d] |= 1;
			if (d & 1)
				bitlookuph[1][c][d] |= 2;
			masklookuph[0][c] = 0;
			if (c & 2)
				masklookuph[0][c] |= 0xFFFF;
			masklookuph[1][c] = 0;
			if (c & 1)
				masklookuph[1][c] |= 0xFFFF;
		}
	}
	for (c = 0; c < 16; c++) {
		collookup[c] = (c << 4) | (c << 20);
	}
	// snemdebug("%08X %08X\n",vram,vramb);
}

void resetppu()
{
	int c, d;
	memset(&ppu, 0, sizeof(ppu));
	// snemdebug("1 %08X %08X\n",vram,vramb);
	ppu.portctrl = ppu.vramaddr = 0;
	ppu.palbuffer = 0;
	// snemdebug("2 %08X %08X\n",vram,vramb);
	memset(vram, 0, 0x10000);
	ppumask = 0;
	for (c = 0; c < 10; c++) {
		for (d = 0; d < 164; d++) {
			window[c][d] = 0xFFFFFFFF;
		}
	}
	for (d = 0; d < 164; d++) {
		window[8][d] = 0;
	}
}

void dumpvram()
{
	FILE* f = fopen("vram.dmp", "wb");
	fwrite(vram, 0x10000, 1, f);
	fwrite(ppu.pal, 512, 1, f);
	fwrite(sprram, 544, 1, f);
	fclose(f);
	snemdebug("left1 %i right1 %i left2 %i right2 %i\n", ppu.w1left, ppu.w1right,
		   ppu.w2left, ppu.w2right);
	snemdebug("Window logic %02X enable %02X\n", ppu.windlogic, ppu.windena1);
	snemdebug("BG0 : %04X  BG1 : %04X  BG2 : %04X  BG3 : %04X\n", ppu.bg[0] << 1,
		   ppu.bg[1] << 1, ppu.bg[2] << 1, ppu.bg[3] << 1);
	snemdebug("CH0 : %04X  CH1 : %04X  CH2 : %04X  CH3 : %04X\n", ppu.chr[0] << 1,
		   ppu.chr[1] << 1, ppu.chr[2] << 1, ppu.chr[3] << 1);
	snemdebug("BG3 yscroll %i\n", ppu.yscroll[2]);
}

int bgtype[8][4] = {
	{2, 3, 3, 3},
	{4, 4, 2, 0},
	{4, 4, 0, 0},
	{8, 4, 0, 0},
	{8, 2, 0, 0},
	{5, 6, 0, 0},
	{0, 0, 0, 0},
	{7, 0, 0, 0},
};

/* 0-3 = BG (low priority)
  4-7 = BG (high priority)
  8-11 - Sprites */
int draworder[16][12] = {
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11}, /* Mode 7 - needs changing */
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 9, 1, 0, 10, 5, 4, 11, 6}, /* Mode 1 - priority bit set */
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11},
	{3, 2, 8, 7, 6, 9, 1, 0, 10, 5, 4, 11} /* Mode 7 - needs changing */
};

/* Lookup tables for address calculations */
uint16_t ylookup[4][64] = {
	{0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0, 0x100, 0x120,
	 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220, 0x240, 0x260,
	 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0,
	 0x3C0, 0x3E0, 0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
	 0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220,
	 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360,
	 0x380, 0x3A0, 0x3C0, 0x3E0},
	{0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0, 0x100, 0x120,
	 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220, 0x240, 0x260,
	 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0,
	 0x3C0, 0x3E0, 0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0,
	 0x100, 0x120, 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220,
	 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360,
	 0x380, 0x3A0, 0x3C0, 0x3E0},
	{0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0, 0x100, 0x120,
	 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220, 0x240, 0x260,
	 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0,
	 0x3C0, 0x3E0, 0x400, 0x420, 0x440, 0x460, 0x480, 0x4A0, 0x4C0, 0x4E0,
	 0x500, 0x520, 0x540, 0x560, 0x580, 0x5A0, 0x5C0, 0x5E0, 0x600, 0x620,
	 0x640, 0x660, 0x680, 0x6A0, 0x6C0, 0x6E0, 0x700, 0x720, 0x740, 0x760,
	 0x780, 0x7A0, 0x7C0, 0x7E0},
	{0x000, 0x020, 0x040, 0x060, 0x080, 0x0A0, 0x0C0, 0x0E0, 0x100, 0x120,
	 0x140, 0x160, 0x180, 0x1A0, 0x1C0, 0x1E0, 0x200, 0x220, 0x240, 0x260,
	 0x280, 0x2A0, 0x2C0, 0x2E0, 0x300, 0x320, 0x340, 0x360, 0x380, 0x3A0,
	 0x3C0, 0x3E0, 0x800, 0x820, 0x840, 0x860, 0x880, 0x8A0, 0x8C0, 0x8E0,
	 0x900, 0x920, 0x940, 0x960, 0x980, 0x9A0, 0x9C0, 0x9E0, 0xA00, 0xA20,
	 0xA40, 0xA60, 0xA80, 0xAA0, 0xAC0, 0xAE0, 0xB00, 0xB20, 0xB40, 0xB60,
	 0xB80, 0xBA0, 0xBC0, 0xBE0}};

uint16_t xlookup[2][64] = {
	{0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, 0x009,
	 0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F, 0x010, 0x011, 0x012, 0x013,
	 0x014, 0x015, 0x016, 0x017, 0x018, 0x019, 0x01A, 0x01B, 0x01C, 0x01D,
	 0x01E, 0x01F, 0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	 0x008, 0x009, 0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F, 0x010, 0x011,
	 0x012, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x019, 0x01A, 0x01B,
	 0x01C, 0x01D, 0x01E, 0x01F},
	{0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, 0x009,
	 0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F, 0x010, 0x011, 0x012, 0x013,
	 0x014, 0x015, 0x016, 0x017, 0x018, 0x019, 0x01A, 0x01B, 0x01C, 0x01D,
	 0x01E, 0x01F, 0x400, 0x401, 0x402, 0x403, 0x404, 0x405, 0x406, 0x407,
	 0x408, 0x409, 0x40A, 0x40B, 0x40C, 0x40D, 0x40E, 0x40F, 0x410, 0x411,
	 0x412, 0x413, 0x414, 0x415, 0x416, 0x417, 0x418, 0x419, 0x41A, 0x41B,
	 0x41C, 0x41D, 0x41E, 0x41F},
};

int sprsize[8][2] =
	{{1, 2}, {1, 4}, {1, 8}, {2, 4}, {2, 8}, {4, 8}, {4, 8}, {4, 8}};

// FILE *wfile;

#define WINDOWLOGIC(windena, windlogic)                                        \
	for (x = 0; x < 256; x++) {                                                \
		if (windena & 2) {                                                     \
			if (x < ppu.w1left || x > ppu.w1right)                             \
				w2[x] = 0xFFFF;                                                \
			else                                                               \
				w2[x] = 0;                                                     \
		} else                                                                 \
			w2[x] = 0;                                                         \
		if (windena & 1)                                                       \
			w2[x] ^= 0xFFFF;                                                   \
		if (windena & 8) {                                                     \
			if (x < ppu.w2left || x > ppu.w2right)                             \
				w3[x] = 0xFFFF;                                                \
			else                                                               \
				w3[x] = 0;                                                     \
		} else                                                                 \
			w3[x] = 0;                                                         \
		if (windena & 4)                                                       \
			w3[x] ^= 0xFFFF;                                                   \
		switch (windlogic & 3) {                                               \
		case 0:                                                                \
			w[x] = w2[x] | w3[x];                                              \
			break;                                                             \
		case 1:                                                                \
			w[x] = w2[x] | w3[x];                                              \
			break;                                                             \
		case 2:                                                                \
			w[x] = w2[x] ^ w3[x];                                              \
			break;                                                             \
		case 3:                                                                \
			w[x] = ~(w2[x] ^ w3[x]);                                           \
			break;                                                             \
		}                                                                      \
	}

int windowdisable = 0;
void recalcwindows()
{
	int x;
	uint16_t* w;
	uint16_t w2[256], w3[256];
	// if (!wfile) { wfile=fopen("window.dmp","wb"); }
	w = &window[0][32];
	if (ppu.windena1 & 0xA && !windowdisable) { /* BG1 */
		WINDOWLOGIC((ppu.windena1 & 0xF), (ppu.windlogic & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	w = &window[1][32];
	if (ppu.windena1 & 0xA0 && !windowdisable) { /* BG2 */
		WINDOWLOGIC((ppu.windena1 >> 4), ((ppu.windlogic >> 2) & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	w = &window[2][32];
	if (ppu.windena2 & 0xA && !windowdisable) { /* BG3 */
		WINDOWLOGIC((ppu.windena2 & 0xF), ((ppu.windlogic >> 4) & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	w = &window[3][32];
	if (ppu.windena2 & 0xA0 && !windowdisable) { /* BG4 */
		WINDOWLOGIC((ppu.windena2 >> 4), ((ppu.windlogic >> 6) & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	w = &window[9][32];
	if (ppu.windena3 & 0xA && !windowdisable) { /* OBJ */
		WINDOWLOGIC((ppu.windena3 & 0xF), (ppu.windlogic2 & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	w = &window[5][32];
	if (ppu.windena3 & 0xA0 && !windowdisable) { /* Colour window */
		WINDOWLOGIC((ppu.windena3 >> 4), ((ppu.windlogic2 >> 2) & 3))
	} else {
		for (x = 0; x < 256; x++) {
			w[x] = 0xFFFF;
		}
	}
	for (x = 0; x < 128; x++) {
		window[6][x + 32] = window[5][x + 32] ^ 0xFFFFFFFF;
	}
}

#define CONTINUOUS 1
#define INDIRECT 2

void dohdma(int line)
{
	int c;
	for (c = 0; c < 8; c++) {
		if (!line) {
			hdmaaddr[c] = dmasrc[c];
			hdmacount[c] = 0;
			// if (c==2) {
			// snemdebug("Reset HDMA %i loading from
			// %02X%04X
			// %i\n",c,dmabank[c],hdmaaddr[c],hdmacount[c]); }
		}
		if (hdmaena & (1 << c) && hdmacount[c] != -1) {
			if (hdmacount[c] <= 0) {
				hdmacount[c] = readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
				// snemdebug("HDMA %i count
				// now %04X at %02X%04X
				// %02X
				// %04X\n",c,hdmacount[c],dmabank[c],hdmaaddr[c],dmactrl[c],hdmadat[c]);
				if (!hdmacount[c])
					goto finishhdma;
				hdmastat[c] = 0;
				if (hdmacount[c] & 0x80) {
					if (hdmacount[c] != 0x80) {
						hdmacount[c] &= 0x7F;
					}
					hdmastat[c] |= CONTINUOUS;
					// snemdebug("Continuous
					// for %i
					// lines\n",hdmacount[c]);
				}
				if (dmactrl[c] & 0x40) {
					hdmastat[c] |= INDIRECT;
					hdmaaddr2[c] = readmemw((dmabank[c] << 16) | hdmaaddr[c]);
					// snemdebug("Indirect
					// :
					// %02X%04X\n",dmaibank[c],hdmaaddr2[c]);
					hdmaaddr[c] += 2;
				}
				// snemdebug("Channel %i now
				// %02X%04X\n",c,dmabank[c],hdmaaddr[c]);
				// if (c==5) {
				// snemdebug("Channel 4 :
				// dest %04X read from
				// %02X %04X %04X stat
				// %i\n",dmadest[c],dmabank[c],hdmaaddr[c],hdmaaddr2[c],hdmastat[c]); }
				switch (dmactrl[c] & 7) {
				case 1: /* Two registers */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					break;
				case 2: /* One register write twice */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
				case 0: /* One register write once */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					// if (c==2) {
					// snemdebug("Channel
					// 2 now
					// %02X%04X\n",dmabank[c],hdmaaddr[c]); }
					break;
				case 3: /* Two registers write twice */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					break;
				case 4: /* Four registers */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 2, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 3, hdmadat[c]);
					break;
				default:
					snemlog("Bad HDMA transfer mode %i %02X %i\n",
							dmactrl[c] & 7, dmadest[c], hdmastat[c]);
					dumpregs();
					exit(-1);
				}
			} else if (hdmastat[c] & CONTINUOUS) {
				switch (dmactrl[c] & 7) {
				case 1: /* Two registers */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					break;
				case 2: /* One register write twice */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
				case 0: /* One register write once */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					break;
				case 3: /* Two registers write twice */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					break;
				case 4: /* Four registers */
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c], hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 1, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 2, hdmadat[c]);
					if (hdmastat[c] & INDIRECT)
						hdmadat[c] =
							readmem((dmaibank[c] << 16) | (hdmaaddr2[c]++));
					else
						hdmadat[c] =
							readmem((dmabank[c] << 16) | (hdmaaddr[c]++));
					writeppu(dmadest[c] + 3, hdmadat[c]);
					break;
				default:
					snemlog("Bad HDMA2 transfer mode %i\n", dmactrl[c] & 7);
					dumpregs();
					exit(-1);
				}
			}
		finishhdma:
			hdmacount[c]--;
		}
	}
}

void doblit()
{
	// drawvol(otherscr);
	/*for (int c=0;c<8;c++)	{
	if (voiceon&1)
	   rectfill(otherscr,(c*20)+8,2,(c*20)+24,10,makecol(255,255,255));
	voiceon>>=1;
	} */
	blit(otherscr, sysb, 64, 1, 0, 0, 256, 224);
	stretch_blit(sysb, screen, 0, 0, 256, 224, 0, 0, 512, 448);
}

#define sgetr(c) (((c >> _rgb_r_shift_16) & 0x1F) << 3)
#define sgetg(c) (((c >> _rgb_g_shift_16) & 0x3F) << 2)
#define sgetb(c) (((c >> _rgb_b_shift_16) & 0x1F) << 3)
void docolour(uint16_t* pw, uint16_t* pw2, uint16_t* pw3,
			  uint16_t* pw4)
{
	int x;
	//int asr, asg, asb;
	uint16_t dat;
	uint16_t* pal = pallookup[ppu.screna & 15];
	switch ((ppu.cgadsub >> 6) | ((ppu.cgwsel & 2) << 1)) {
	case 0:
		if (!ppu.fixedcol) {
			for (x = 64; x < 320; x++)
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
		} else {
			for (x = 64; x < 320; x++) {
				if (pw3[x] & 0x4000 && pw4[x]) {
					pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
					dat = ppu.fixedcol;
					pw[x] = cgadd(pw[x], dat);
				} else
					pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
			}
		}
		break;
	case 4:
		for (x = 64; x < 320; x++) {
			if (pw3[x] & 0x4000 && pw4[x]) {
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
				if (!(pw2[x] & 0x2000))
					dat = pallookup[ppu.screna & 15][pw2[x] & 255];
				else
					dat = ppu.fixedcol;
				pw[x] = cgadd(pw[x], dat);
			} else
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
		}
		break;
	case 1:
		for (x = 64; x < 320; x++) {
			if (pw3[x] & 0x4000 && pw4[x]) {
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
				dat = ppu.fixedcol;
				pw[x] = ((pw[x] & 0xF7DE) + (dat & 0xF7DE)) >> 1;
			} else
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
		}
		break;
	case 5:
		for (x = 64; x < 320; x++) {
			if (pw3[x] & 0x4000 && pw4[x]) {
				pw[x] = pal[pw3[x] & 255];
				if (!(pw2[x] & 0x2000)) {
					dat = pal[pw2[x] & 255];
					pw[x] = ((pw[x] & 0xF7DE) + (dat & 0xF7DE)) >> 1;
				} else
					pw[x] = cgadd(pw[x], ppu.fixedcol);
			} else
				pw[x] = pal[pw3[x] & 255];
		}
		break;
	case 2:
	case 6:
		for (x = 64; x < 320; x++) {
			if (pw3[x] & 0x4000 && pw4[x]) {
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
				if (ppu.cgwsel & 2 && !(pw2[x] & 0x2000))
					dat = pallookup[ppu.screna & 15][pw2[x] & 255];
				else
					dat = ppu.fixedcol;
				pw[x] = cgsub(pw[x], dat);
			} else
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
		}
		break;
	case 3:
	case 7:
		for (x = 64; x < 320; x++) {
			if (pw3[x] & 0x4000 && pw4[x]) {
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
				if (ppu.cgwsel & 2 && !(pw2[x] & 0x2000))
					dat = pallookup[ppu.screna & 15][pw2[x] & 255];
				else
					dat = ppu.fixedcol;
				/* asr=(sgetr(pw[x])-sgetr(dat))>>1;
				if (asr<0) asr=0;
				asg=(sgetg(pw[x])-sgetg(dat))>>1;
				if (asg<0) asg=0;
				asb=(sgetb(pw[x])-sgetb(dat))>>1;
				if (asb<0) asb=0;
				pw[x]=(((asr >> 3) <<
				   _rgb_r_shift_16) |
				   ((asg >> 2) <<
				   _rgb_g_shift_16) |
				   ((asb >> 3) <<
				   _rgb_b_shift_16)); */
				pw[x] = cgsubh(pw[x], dat);
			} else
				pw[x] = pallookup[ppu.screna & 15][pw3[x] & 255];
		}
		break;
	}
}

#define getm7pixel(x2, y2)                                                     \
	temp = vramb[(((y2 & ~7) << 5) | ((x2 & ~7) >> 2)) & 0x7FFF];              \
	col = vramb[((temp << 7) + ((y2 & 7) << 4) + ((x2 & 7) << 1) + 1) & 0x7FFF]

int lastline;
void drawline(int line)
{
	uint32_t aa, bb, cc, dd, arith;
	int ma, mb, mc, md, hoff, voff, cx, cy;
	int c, d, x, xx, xxx, pri, y, yy, ss, sprc;
	//int asr, asg, asb;
	uint16_t addr, tile, dat, baseaddr;
	reg b1, b2, b3, b4, b5;
	uint32_t* p;
	uint16_t* pw, *pw2, *pw3, *pw4;
	uint32_t* wp;
	int col;
	int l;
	unsigned char layers = (ppu.main | ppu.sub); //&~ppumask;
	int xsize, ysize;
	uint16_t* xlk;
	unsigned char temp;
	unsigned char wmask;
	lastline = line;
	if (windowschanged) {
		recalcwindows();
		windowschanged = 0;
	}
	if (ppu.spraddr & 0x200)
		ppu.pri = (ppu.spraddr & 0x1F) << 2;
	else
		ppu.pri = ppu.spraddr >> 2;
	// if (ppu.prirotation) { snemlog("PPU.PRI %02X\n",ppu.pri); }
	for (ss = 0; ss < 2; ss++) {
		if (ss) {
			b = mainscr;
			layers = ppu.main & ~ppumask;
			wmask = ppu.wmaskmain;
		} else {
			b = subscr;
			layers = ppu.sub & ~ppumask;
			wmask = ppu.wmasksub;
		}
		if (!line)
			ppu.spraddr = ppu.spraddrs;
		if (ppu.screna & 0x80) {
			hline(b, 64, line, 320, 0);
		} else {
			if (ss)
				hline(b, 64, line, 320, (ppu.cgadsub & 0x20) ? 0x4000 : 0);
			else
				hline(b, 64, line, 320, 0x2000);
			for (d = 0; d < 12; d++) {
				c = draworder[ppu.mode][d];
				pri = (c & 4) ? 0x2000 : 0;
				if (c & 8) { /* Sprites */
					if (layers & 16 && !(ppumask & 16)) {
						pri = c & 3;
						addr = 0x1FC;
						for (sprc = 127; sprc >= 0; sprc--) {
							/* if (ppu.prirotation) { c=(sprc+(ppu.pri-1))&127; }
                            else */	c =	sprc;
							dat = sprram[(c >> 2) + 512];
							dat >>= ((c & 3) << 1);
							dat &= 3;
							y = (sprram[addr + 1] + 1) & 255;
							x = sprram[addr];
							x = (x + ((dat & 1) << 8) + 64) & 511;
							if (y > 239)
								y |= 0xFFFFFF00;
							if (line >= y &&
								line <
									(y + (sprsize[ppu.sprsize][(dat >> 1) & 1]
										  << 3)) &&
								pri == ((sprram[addr + 3] >> 4) & 3) &&
								((x < 320) /* || (x>456) */)) { /* Draw sprite */
								// x-=56;
								// x&=511;
								// p=(uint32_t *)(b->line[line]+( ((64-((x^63)&63))+(x&~63))<<1) );
								if (wmask & 0x10)
									wp = (uint32_t*)(((unsigned char*)
															   window[9]) +
														  (x << 1));
								else
									wp = (uint32_t*)(((unsigned char*)
															   window[7]) +
														  (x << 1));
								p = (uint32_t*)(b->line[line] + (x << 1));
								tile = ((sprram[addr + 2] |
										 ((sprram[addr + 3] & 1) << 8))
										<< 4) +
									   ppu.sprbase; /* Calculate tile address */
								y = line - y;
								if (sprram[addr + 3] & 0x80) {
									y = y ^
										((sprsize[ppu.sprsize][(dat >> 1) & 1]
										  << 3) - 1);
								}
								tile += (y & 7);
								tile += ((y & ~7) << 5);
								dat >>= 1;
								col = ((sprram[addr + 3] >> 1) & 7) | 8;
								if ((col & 4) && (ppu.cgadsub & 0x10))
									arith = 0x40004000;
								else
									arith = 0;
								col = collookup[col] | arith;
								if (sprram[addr + 3] & 0x40) {
									tile += ((sprsize[ppu.sprsize][dat & 1] - 1)
											 << 4);
									for (xxx = 0;
										 xxx < sprsize[ppu.sprsize][dat & 1];
										 xxx++) {
										b1.w = vram[tile & 0x7FFF];
										b2.w = vram[(tile + 8) & 0x7FFF];
										b3.w = b1.w | b2.w;
										b3.b.l |= b3.b.h;
										if (b3.b.l) {
											p[0] &=
												~(masklookup[1][b3.b.l & 3] &
												  wp[0]);
											p[0] |=
												(bitlookup[1][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[1][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (col &
												  masklookup[1][b3.b.l & 3])) &
												wp[0];
											p[1] &=
												~(masklookup[1][(b3.b.l >> 2) &
																3] &
												  wp[1]);
											p[1] |=
												(bitlookup[1][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (bitlookup[1][(b2.b.l >> 2) &
															   3]
														   [(b2.b.h >> 2) & 3]
												  << 2) |
												 (col &
												  masklookup[1][(b3.b.l >> 2) &
																3])) &
												wp[1];
											p[2] &=
												~(masklookup[1][(b3.b.l >> 4) &
																3] &
												  wp[2]);
											p[2] |=
												(bitlookup[1][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (bitlookup[1][(b2.b.l >> 4) &
															   3]
														   [(b2.b.h >> 4) & 3]
												  << 2) |
												 (col &
												  masklookup[1][(b3.b.l >> 4) &
																3])) &
												wp[2];
											p[3] &=
												~(masklookup[1][(b3.b.l >> 6) &
																3] &
												  wp[3]);
											p[3] |=
												(bitlookup[1][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (bitlookup[1][(b2.b.l >> 6) &
															   3]
														   [(b2.b.h >> 6) & 3]
												  << 2) |
												 (col &
												  masklookup[1][(b3.b.l >> 6) &
																3])) &
												wp[3];
										}
										p += 4;
										wp += 4;
										tile -= 16;
									}
								} else {
									for (xxx = 0;
										 xxx < sprsize[ppu.sprsize][dat & 1];
										 xxx++) {
										b1.w = vram[tile & 0x7FFF];
										b2.w = vram[(tile + 8) & 0x7FFF];
										b3.w = b1.w | b2.w;
										b3.b.l |= b3.b.h;
										if (b3.b.l) {
											p[3] &=
												~(masklookup[0][b3.b.l & 3] &
												  wp[3]);
											p[3] |=
												(bitlookup[0][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[0][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[3];
											p[2] &=
												~(masklookup[0][(b3.b.l >> 2) &
																3] &
												  wp[2]);
											p[2] |=
												(bitlookup[0][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (bitlookup[0][(b2.b.l >> 2) &
															   3]
														   [(b2.b.h >> 2) & 3]
												  << 2) |
												 (col &
												  masklookup[0][(b3.b.l >> 2) &
																3])) &
												wp[2];
											p[1] &=
												~(masklookup[0][(b3.b.l >> 4) &
																3] &
												  wp[1]);
											p[1] |=
												(bitlookup[0][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (bitlookup[0][(b2.b.l >> 4) &
															   3]
														   [(b2.b.h >> 4) & 3]
												  << 2) |
												 (col &
												  masklookup[0][(b3.b.l >> 4) &
																3])) &
												wp[1];
											p[0] &=
												~(masklookup[0][(b3.b.l >> 6) &
																3] &
												  wp[0]);
											p[0] |=
												(bitlookup[0][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (bitlookup[0][(b2.b.l >> 6) &
															   3]
														   [(b2.b.h >> 6) & 3]
												  << 2) |
												 (col &
												  masklookup[0][(b3.b.l >> 6) &
																3])) &
												wp[0];
										}
										p += 4;
										wp += 4;
										tile += 16;
									}
								}
							}
							addr -= 4;
						}
					}
				} else if (layers & (1 << (c & 3))) {
					c &= 3;
					if (ppu.cgadsub & (1 << c))
						arith = 0x40004000;
					else
						arith = 0;
					p = (uint32_t*)(b->line[line] +
										 ((64 - (ppu.xscroll[c] & 7)) << 1));
					if (wmask & (1 << c))
						wp = (uint32_t*)((unsigned char*)window[c] +
											  ((64 - (ppu.xscroll[c] & 7))
											   << 1));
					else
						wp = (uint32_t*)((unsigned char*)window[7] +
											  ((64 - (ppu.xscroll[c] & 7))
											   << 1));
					l = (line + ppu.yscroll[c]) & 1023;
					xx = ppu.xscroll[c] >> 3;
					if ((ppu.mode & 7) != 7) {
						if (ppu.size[c] & 1)
							xsize = 63;
						else
							xsize = 31;
						if (ppu.size[c] & 2)
							ysize = 511;
						else
							ysize = 255;
						if (ppu.tilesize & (1 << c))
							baseaddr =
								ppu.bg[c] +
								ylookup[(ppu.size[c] & 3) /* >>1 */][l >> 4];
						else
							baseaddr =
								ppu.bg[c] + ylookup[(ppu.size[c] &
													 3) /* >>1 */][(l >> 3) & 63];
						xlk = xlookup[ppu.size[c] & 1];
						for (x = 0; x < 33; x++) {
							if (ppu.tilesize & (1 << c))
								addr = baseaddr + xlk[((x + xx) >> 1) & 63];
							else
								addr = baseaddr + xlk[(x + xx) & 63];
							dat = vram[addr & 0x7FFF];
							if ((dat & 0x2000) != pri)
								goto skiptile;
							tile = dat & 0x3FF;
							col = (dat >> 10) & 7;
							switch (bgtype[ppu.mode & 7][c]) {
							case 2:
								tile <<= 3;
								if (ppu.tilesize & (1 << c)) {
									if (dat & 0x8000)
										tile += (((l ^ 8) & 8) << 4);
									else
										tile += ((l & 8) << 4);
									if (dat & 0x4000)
										tile += (((x + xx + 1) & 1) << 3);
									else
										tile += (((x + xx) & 1) << 3);
								}
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								tile &= 0x7FFF;
								b1.w = vram[tile];
								b3.b.l = (b1.b.l | b1.b.h);
								if (b3.b.l) {
									col = (collookup[col] >> 2) | arith;
									if (dat & 0x4000) {
										p[0] &= ~(masklookup[1][b3.b.l & 3] &
												  wp[0]);
										p[0] |= (bitlookup[1][b1.b.l &
															  3][b1.b.h & 3] |
												 (col &
												  masklookup[1][b3.b.l & 3])) &
												wp[0];
										p[1] &=
											~(masklookup[1][(b3.b.l >> 2) & 3] &
											  wp[1]);
										p[1] |= (bitlookup[1][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 2) &
																3])) &
												wp[1];
										p[2] &=
											~(masklookup[1][(b3.b.l >> 4) & 3] &
											  wp[2]);
										p[2] |= (bitlookup[1][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 4) &
																3])) &
												wp[2];
										p[3] &=
											~(masklookup[1][(b3.b.l >> 6) & 3] &
											  wp[3]);
										p[3] |= (bitlookup[1][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 6) &
																3])) &
												wp[3];
									} else {
										p[3] &= ~(masklookup[0][b3.b.l & 3] &
												  wp[3]);
										p[3] |= (bitlookup[0][b1.b.l &
															  3][b1.b.h & 3] |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[3];
										p[2] &=
											~(masklookup[0][(b3.b.l >> 2) & 3] &
											  wp[2]);
										p[2] |= (bitlookup[0][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 2) &
																3])) &
												wp[2];
										p[1] &=
											~(masklookup[0][(b3.b.l >> 4) & 3] &
											  wp[1]);
										p[1] |= (bitlookup[0][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 4) &
																3])) &
												wp[1];
										p[0] &=
											~(masklookup[0][(b3.b.l >> 6) & 3] &
											  wp[0]);
										p[0] |= (bitlookup[0][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 6) &
																3])) &
												wp[0];
									}
								}
								break;
							case 3:
								tile <<= 3;
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								b1.w = vram[tile & 0x7FFF];
								b3.b.l = (b1.b.l | b1.b.h);
								if (b3.b.l) {
									col = (collookup[col] >> 2) | arith;
									col |= (c << 5) | (c << 21);
									if (dat & 0x4000) {
										p[0] &= ~(masklookup[1][b3.b.l & 3] &
												  wp[0]);
										p[0] |= (bitlookup[1][b1.b.l &
															  3][b1.b.h & 3] |
												 (col &
												  masklookup[1][b3.b.l & 3])) &
												wp[0];
										p[1] &=
											~(masklookup[1][(b3.b.l >> 2) & 3] &
											  wp[1]);
										p[1] |= (bitlookup[1][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 2) &
																3])) &
												wp[1];
										p[2] &=
											~(masklookup[1][(b3.b.l >> 4) & 3] &
											  wp[2]);
										p[2] |= (bitlookup[1][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 4) &
																3])) &
												wp[2];
										p[3] &=
											~(masklookup[1][(b3.b.l >> 6) & 3] &
											  wp[3]);
										p[3] |= (bitlookup[1][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (col &
												  masklookup[1][(b3.b.l >> 6) &
																3])) &
												wp[3];
									} else {
										p[3] &= ~(masklookup[0][b3.b.l & 3] &
												  wp[3]);
										p[3] |= (bitlookup[0][b1.b.l &
															  3][b1.b.h & 3] |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[3];
										p[2] &=
											~(masklookup[0][(b3.b.l >> 2) & 3] &
											  wp[2]);
										p[2] |= (bitlookup[0][(b1.b.l >> 2) &
															  3][(b1.b.h >> 2) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 2) &
																3])) &
												wp[2];
										p[1] &=
											~(masklookup[0][(b3.b.l >> 4) & 3] &
											  wp[1]);
										p[1] |= (bitlookup[0][(b1.b.l >> 4) &
															  3][(b1.b.h >> 4) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 4) &
																3])) &
												wp[1];
										p[0] &=
											~(masklookup[0][(b3.b.l >> 6) & 3] &
											  wp[0]);
										p[0] |= (bitlookup[0][(b1.b.l >> 6) &
															  3][(b1.b.h >> 6) &
																 3] |
												 (col &
												  masklookup[0][(b3.b.l >> 6) &
																3])) &
												wp[0];
									}
								}
								break;
							case 4:
								tile <<= 4;
								if (ppu.tilesize & (1 << c)) {
									if (dat & 0x8000)
										tile += (((l ^ 8) & 8) << 5);
									else
										tile += ((l & 8) << 5);
									if (dat & 0x4000)
										tile += (((x + xx + 1) & 1) << 4);
									else
										tile += (((x + xx) & 1) << 4);
								}
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								b1.w = vram[tile & 0x7FFF];
								b2.w = vram[(tile + 8) & 0x7FFF];
								b3.w = b1.w | b2.w;
								b3.b.l |= b3.b.h;
								if (b3.b.l) {
									col = collookup[col] | arith;
									if (dat & 0x4000) {
										p[0] &= ~(masklookup[1][b3.b.l & 3] &
												  wp[0]);
										p[0] |= (bitlookup[1][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[1][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (col &
												  masklookup[1][b3.b.l & 3])) &
												wp[0];
										p[1] &=
											~(masklookup[1][(b3.b.l >> 2) & 3] &
											  wp[1]);
										p[1] |=
											(bitlookup[1][(b1.b.l >> 2) &
														  3][(b1.b.h >> 2) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 2) &
														   3][(b2.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookup[1][(b3.b.l >> 2) &
															3])) &
											wp[1];
										p[2] &=
											~(masklookup[1][(b3.b.l >> 4) & 3] &
											  wp[2]);
										p[2] |=
											(bitlookup[1][(b1.b.l >> 4) &
														  3][(b1.b.h >> 4) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 4) &
														   3][(b2.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookup[1][(b3.b.l >> 4) &
															3])) &
											wp[2];
										p[3] &=
											~(masklookup[1][(b3.b.l >> 6) & 3] &
											  wp[3]);
										p[3] |=
											(bitlookup[1][(b1.b.l >> 6) &
														  3][(b1.b.h >> 6) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 6) &
														   3][(b2.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookup[1][(b3.b.l >> 6) &
															3])) &
											wp[3];
									} else {
										p[3] &= ~(masklookup[0][b3.b.l & 3] &
												  wp[3]);
										p[3] |= (bitlookup[0][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[0][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[3];
										p[2] &=
											~(masklookup[0][(b3.b.l >> 2) & 3] &
											  wp[2]);
										p[2] |=
											(bitlookup[0][(b1.b.l >> 2) &
														  3][(b1.b.h >> 2) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 2) &
														   3][(b2.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookup[0][(b3.b.l >> 2) &
															3])) &
											wp[2];
										p[1] &=
											~(masklookup[0][(b3.b.l >> 4) & 3] &
											  wp[1]);
										p[1] |=
											(bitlookup[0][(b1.b.l >> 4) &
														  3][(b1.b.h >> 4) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 4) &
														   3][(b2.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookup[0][(b3.b.l >> 4) &
															3])) &
											wp[1];
										p[0] &=
											~(masklookup[0][(b3.b.l >> 6) & 3] &
											  wp[0]);
										p[0] |=
											(bitlookup[0][(b1.b.l >> 6) &
														  3][(b1.b.h >> 6) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 6) &
														   3][(b2.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookup[0][(b3.b.l >> 6) &
															3])) &
											wp[0];
									}
								}
								break;
							case 5: /* 4bpp - high res */
								tile <<= 4;
								if (ppu.tilesize & (1 << c)) {
									if (dat & 0x8000)
										tile += (((l ^ 8) & 8) << 5);
									else
										tile += ((l & 8) << 5);
									if (dat & 0x4000)
										tile += (((x + xx + 1) & 1) << 4);
									else
										tile += (((x + xx) & 1) << 4);
								}
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								b1.w = vram[tile & 0x7FFF];
								b2.w = vram[(tile + 8) & 0x7FFF];
								b3.w = vram[(tile + 16) & 0x7FFF];
								b4.w = vram[(tile + 24) & 0x7FFF];
								b5.b.l = b1.b.l | b1.b.h | b2.b.l | b2.b.h;
								b5.b.h = b3.b.l | b3.b.h | b4.b.l | b4.b.h;
								if (b5.b.l | b5.b.h) {
									pw = p;
									col = collookup[col] | arith;
									if (dat & 0x4000) {
										pw[0] &= ~(masklookuph[1][b5.b.h & 3] &
												   wp[0]);
										pw[0] |=
											(bitlookuph[1][b3.b.l & 3][b3.b.h &
																	   3] |
											 (bitlookuph[1][b4.b.l & 3][b4.b.h &
																		3]
											  << 2) |
											 (col &
											  masklookuph[1][b5.b.h & 3])) &
											wp[0];
										pw[1] &=
											~(masklookuph[1][(b5.b.h >> 2) &
															 3] &
											  (wp[0] >> 16));
										pw[1] |=
											(bitlookuph[1][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) &
															  3] |
											 (bitlookuph[1][(b4.b.l >> 2) & 3]
														[(b4.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.h >> 2) &
															 3])) &
											(wp[0] >> 16);
										pw[2] &=
											~(masklookuph[1][(b5.b.h >> 4) &
															 3] &
											  wp[1]);
										pw[2] |=
											(bitlookuph[1][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) &
															  3] |
											 (bitlookuph[1][(b4.b.l >> 4) & 3]
														[(b4.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.h >> 4) &
															 3])) &
											wp[1];
										pw[3] &=
											~(masklookuph[1][(b5.b.h >> 6) &
															 3] &
											  (wp[1] >> 16));
										pw[3] |=
											(bitlookuph[1][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) &
															  3] |
											 (bitlookuph[1][(b4.b.l >> 6) & 3]
														[(b4.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.h >> 6) &
															 3])) &
											(wp[1] >> 16);
										pw[4] &= ~(masklookuph[1][b5.b.l & 3] &
												   wp[2]);
										pw[4] |=
											(bitlookuph[1][b1.b.l & 3][b1.b.h &
																	   3] |
											 (bitlookuph[1][b2.b.l & 3][b2.b.h &
																		3]
											  << 2) |
											 (col &
											  masklookuph[1][b5.b.l & 3])) &
											wp[2];
										pw[5] &=
											~(masklookuph[1][(b5.b.l >> 2) &
															 3] &
											  (wp[2] >> 16));
										pw[5] |=
											(bitlookuph[1][(b1.b.l >> 2) &
														   3][(b1.b.h >> 2) &
															  3] |
											 (bitlookuph[1][(b2.b.l >> 2) & 3]
														[(b2.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.l >> 2) &
															 3])) &
											(wp[2] >> 16);
										pw[6] &=
											~(masklookuph[1][(b5.b.l >> 4) &
															 3] &
											  wp[3]);
										pw[6] |=
											(bitlookuph[1][(b1.b.l >> 4) &
														   3][(b1.b.h >> 4) &
															  3] |
											 (bitlookuph[1][(b2.b.l >> 4) & 3]
														[(b2.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.l >> 4) &
															 3])) &
											wp[3];
										pw[7] &=
											~(masklookuph[1][(b5.b.l >> 6) &
															 3] &
											  (wp[3] >> 16));
										pw[7] |=
											(bitlookuph[1][(b1.b.l >> 6) &
														   3][(b1.b.h >> 6) &
															  3] |
											 (bitlookuph[1][(b2.b.l >> 6) & 3]
														[(b2.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookuph[1][(b5.b.l >> 6) &
															 3])) &
											(wp[3] >> 16);
										/* pw[0]&=~(masklookuph[1][b5.b.l&3]&wp[0]);
										pw[0]|=(bitlookuph[1][b1.b.l&3][b1.b.h&3]|(bitlookuph[1][b2.b.l&3][b2.b.h&3]<<2)|(col&masklookuph[1][b5.b.l&3]))&wp[0];
										pw[1]&=~(masklookuph[1][(b5.b.l>>2)&3]&(wp[0]>>16));
										pw[1]|=(bitlookuph[1][(b1.b.l>>2)&3][(b1.b.h>>2)&3]|(bitlookuph[1][(b2.b.l>>2)&3][(b2.b.h>>2)&3]<<2)|(col&masklookuph[1][(b5.b.l>>2)&3]))&(wp[0]>>16);
										pw[2]&=~(masklookuph[1][(b5.b.l>>4)&3]&wp[1]);
										pw[2]|=(bitlookuph[1][(b1.b.l>>4)&3][(b1.b.h>>4)&3]|(bitlookuph[1][(b2.b.l>>4)&3][(b2.b.h>>4)&3]<<2)|(col&masklookuph[1][(b5.b.l>>4)&3]))&wp[1];
										pw[3]&=~(masklookuph[1][(b5.b.l>>6)&3]&(wp[1]>>16));
										pw[3]|=(bitlookuph[1][(b1.b.l>>6)&3][(b1.b.h>>6)&3]|(bitlookuph[1][(b2.b.l>>6)&3][(b2.b.h>>6)&3]<<2)|(col&masklookuph[1][(b5.b.l>>6)&3]))&(wp[1]>>16);
										pw[4]&=~(masklookuph[1][b5.b.h&3]&wp[2]);
										pw[4]|=(bitlookuph[1][b3.b.l&3][b3.b.h&3]|(bitlookuph[1][b4.b.l&3][b4.b.h&3]<<2)|(col&masklookuph[1][b5.b.h&3]))&wp[2];
										pw[5]&=~(masklookuph[1][(b5.b.h>>2)&3]&(wp[2]>>16));
										pw[5]|=(bitlookuph[1][(b3.b.l>>2)&3][(b3.b.h>>2)&3]|(bitlookuph[1][(b4.b.l>>2)&3][(b4.b.h>>2)&3]<<2)|(col&masklookuph[1][(b5.b.h>>2)&3]))&(wp[2]>>16);
										pw[6]&=~(masklookuph[1][(b5.b.h>>4)&3]&wp[3]);
										pw[6]|=(bitlookuph[1][(b3.b.l>>4)&3][(b3.b.h>>4)&3]|(bitlookuph[1][(b4.b.l>>4)&3][(b4.b.h>>4)&3]<<2)|(col&masklookuph[1][(b5.b.h>>4)&3]))&wp[3];
										pw[7]&=~(masklookuph[1][(b5.b.h>>6)&3]&(wp[3]>>16));
										pw[7]|=(bitlookuph[1][(b3.b.l>>6)&3][(b3.b.h>>6)&3]|(bitlookuph[1][(b4.b.l>>6)&3][(b4.b.h>>6)&3]<<2)|(col&masklookuph[1][(b5.b.h>>6)&3]))&(wp[3]>>16); */
									} else {
										pw[7] &= ~(masklookuph[0][b5.b.h & 3] &
												   (wp[3] >> 16));
										pw[7] |=
											(bitlookuph[0][b3.b.l & 3][b3.b.h &
																	   3] |
											 (bitlookuph[0][b4.b.l & 3][b4.b.h &
																		3]
											  << 2) |
											 (col &
											  masklookuph[0][b5.b.h & 3])) &
											(wp[3] >> 16);
										pw[6] &=
											~(masklookuph[0][(b5.b.h >> 2) &
															 3] &
											  wp[3]);
										pw[6] |=
											(bitlookuph[0][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) &
															  3] |
											 (bitlookuph[0][(b4.b.l >> 2) & 3]
														[(b4.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.h >> 2) &
															 3])) &
											wp[3];
										pw[5] &=
											~(masklookuph[0][(b5.b.h >> 4) &
															 3] &
											  (wp[2] >> 16));
										pw[5] |=
											(bitlookuph[0][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) &
															  3] |
											 (bitlookuph[0][(b4.b.l >> 4) & 3]
														[(b4.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.h >> 4) &
															 3])) &
											(wp[2] >> 16);
										pw[4] &=
											~(masklookuph[0][(b5.b.h >> 6) &
															 3] &
											  wp[2]);
										pw[4] |=
											(bitlookuph[0][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) &
															  3] |
											 (bitlookuph[0][(b4.b.l >> 6) & 3]
														[(b4.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.h >> 6) &
															 3])) &
											wp[2];
										pw[3] &= ~(masklookuph[0][b5.b.l & 3] &
												   (wp[1] >> 16));
										pw[3] |=
											(bitlookuph[0][b1.b.l & 3][b1.b.h &
																	   3] |
											 (bitlookuph[0][b2.b.l & 3][b2.b.h &
																		3]
											  << 2) |
											 (col &
											  masklookuph[0][b5.b.l & 3])) &
											(wp[1] >> 16);
										pw[2] &=
											~(masklookuph[0][(b5.b.l >> 2) &
															 3] &
											  wp[1]);
										pw[2] |=
											(bitlookuph[0][(b1.b.l >> 2) &
														   3][(b1.b.h >> 2) &
															  3] |
											 (bitlookuph[0][(b2.b.l >> 2) & 3]
														[(b2.b.h >> 2) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.l >> 2) &
															 3])) &
											wp[1];
										pw[1] &=
											~(masklookuph[0][(b5.b.l >> 4) &
															 3] &
											  (wp[0] >> 16));
										pw[1] |=
											(bitlookuph[0][(b1.b.l >> 4) &
														   3][(b1.b.h >> 4) &
															  3] |
											 (bitlookuph[0][(b2.b.l >> 4) & 3]
														[(b2.b.h >> 4) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.l >> 4) &
															 3])) &
											(wp[0] >> 16);
										pw[0] &=
											~(masklookuph[0][(b5.b.l >> 6) &
															 3] &
											  wp[0]);
										pw[0] |=
											(bitlookuph[0][(b1.b.l >> 6) &
														   3][(b1.b.h >> 6) &
															  3] |
											 (bitlookuph[0][(b2.b.l >> 6) & 3]
														[(b2.b.h >> 6) & 3]
											  << 2) |
											 (col &
											  masklookuph[0][(b5.b.l >> 6) &
															 3])) &
											wp[0];
									}
								}
								break;
							case 6: /* 2bpp - high res */
								tile <<= 3;
								if (ppu.tilesize & (1 << c)) {
									if (dat & 0x8000)
										tile += (((l ^ 8) & 8) << 5);
									else
										tile += ((l & 8) << 5);
									if (dat & 0x4000)
										tile += (((x + xx + 1) & 1) << 4);
									else
										tile += (((x + xx) & 1) << 4);
								}
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								b1.w = vram[tile & 0x7FFF];
								b3.w = vram[(tile + 8) & 0x7FFF];
								b5.b.l = b1.b.l | b1.b.h;
								b5.b.h = b3.b.l | b3.b.h;
								if (b5.b.l | b5.b.h) {
									pw = p;
									col = (collookup[col] >> 2) | arith;
									if (dat & 0x4000) {
										pw[0] &= ~(masklookuph[1][b5.b.h & 3] &
												   wp[0]);
										pw[0] |=
											(bitlookuph[1][b3.b.l & 3][b3.b.h &
																	   3] |
											 (col &
											  masklookuph[1][b5.b.h & 3])) &
											wp[0];
										pw[1] &=
											~(masklookuph[1][(b5.b.h >> 2) &
															 3] &
											  (wp[0] >> 16));
										pw[1] |=
											(bitlookuph[1][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.h >> 2) &
															 3])) &
											(wp[0] >> 16);
										pw[2] &=
											~(masklookuph[1][(b5.b.h >> 4) &
															 3] &
											  wp[1]);
										pw[2] |=
											(bitlookuph[1][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.h >> 4) &
															 3])) &
											wp[1];
										pw[3] &=
											~(masklookuph[1][(b5.b.h >> 6) &
															 3] &
											  (wp[1] >> 16));
										pw[3] |=
											(bitlookuph[1][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.h >> 6) &
															 3])) &
											(wp[1] >> 16);
										pw[4] &= ~(masklookuph[1][b5.b.l & 3] &
												   wp[2]);
										pw[4] |=
											(bitlookuph[1][b1.b.l & 3][b1.b.h &
																	   3] |
											 (col &
											  masklookuph[1][b5.b.l & 3])) &
											wp[2];
										pw[5] &=
											~(masklookuph[1][(b5.b.l >> 2) &
															 3] &
											  (wp[2] >> 16));
										pw[5] |=
											(bitlookuph[1][(b1.b.l >> 2) &
														   3][(b1.b.h >> 2) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.l >> 2) &
															 3])) &
											(wp[2] >> 16);
										pw[6] &=
											~(masklookuph[1][(b5.b.l >> 4) &
															 3] &
											  wp[3]);
										pw[6] |=
											(bitlookuph[1][(b1.b.l >> 4) &
														   3][(b1.b.h >> 4) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.l >> 4) &
															 3])) &
											wp[3];
										pw[7] &=
											~(masklookuph[1][(b5.b.l >> 6) &
															 3] &
											  (wp[3] >> 16));
										pw[7] |=
											(bitlookuph[1][(b1.b.l >> 6) &
														   3][(b1.b.h >> 6) &
															  3] |
											 (col &
											  masklookuph[1][(b5.b.l >> 6) &
															 3])) &
											(wp[3] >> 16);
										/*                                                                                pw[0]&=~(masklookuph[1][b5.b.l&3]&wp[0]);
										pw[0]|=(bitlookuph[1][b1.b.l&3][b1.b.h&3]|(col&masklookuph[1][b5.b.l&3]))&wp[0];
										pw[1]&=~(masklookuph[1][(b5.b.l>>2)&3]&(wp[0]>>16));
										pw[1]|=(bitlookuph[1][(b1.b.l>>2)&3][(b1.b.h>>2)&3]|(col&masklookuph[1][(b5.b.l>>2)&3]))&(wp[0]>>16);
										pw[2]&=~(masklookuph[1][(b5.b.l>>4)&3]&wp[1]);
										pw[2]|=(bitlookuph[1][(b1.b.l>>4)&3][(b1.b.h>>4)&3]|(col&masklookuph[1][(b5.b.l>>4)&3]))&wp[1];
										pw[3]&=~(masklookuph[1][(b5.b.l>>6)&3]&(wp[1]>>16));
										pw[3]|=(bitlookuph[1][(b1.b.l>>6)&3][(b1.b.h>>6)&3]|(col&masklookuph[1][(b5.b.l>>6)&3]))&(wp[1]>>16);
										pw[4]&=~(masklookuph[1][b5.b.h&3]&wp[2]);
										pw[4]|=(bitlookuph[1][b3.b.l&3][b3.b.h&3]|(col&masklookuph[1][b5.b.h&3]))&wp[2];
										pw[5]&=~(masklookuph[1][(b5.b.h>>2)&3]&(wp[2]>>16));
										pw[5]|=(bitlookuph[1][(b3.b.l>>2)&3][(b3.b.h>>2)&3]|(col&masklookuph[1][(b5.b.h>>2)&3]))&(wp[2]>>16);
										pw[6]&=~(masklookuph[1][(b5.b.h>>4)&3]&wp[3]);
										pw[6]|=(bitlookuph[1][(b3.b.l>>4)&3][(b3.b.h>>4)&3]|(col&masklookuph[1][(b5.b.h>>4)&3]))&wp[3];
										pw[7]&=~(masklookuph[1][(b5.b.h>>6)&3]&(wp[3]>>16));
										pw[7]|=(bitlookuph[1][(b3.b.l>>6)&3][(b3.b.h>>6)&3]|(col&masklookuph[1][(b5.b.h>>6)&3]))&(wp[3]>>16); */
									} else {
										pw[7] &= ~(masklookuph[0][b5.b.h & 3] &
												   (wp[3] >> 16));
										pw[7] |=
											(bitlookuph[0][b3.b.l & 3][b3.b.h &
																	   3] |
											 (col &
											  masklookuph[0][b5.b.h & 3])) &
											(wp[3] >> 16);
										pw[6] &=
											~(masklookuph[0][(b5.b.h >> 2) &
															 3] &
											  wp[3]);
										pw[6] |=
											(bitlookuph[0][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.h >> 2) &
															 3])) &
											wp[3];
										pw[5] &=
											~(masklookuph[0][(b5.b.h >> 4) &
															 3] &
											  (wp[2] >> 16));
										pw[5] |=
											(bitlookuph[0][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.h >> 4) &
															 3])) &
											(wp[2] >> 16);
										pw[4] &=
											~(masklookuph[0][(b5.b.h >> 6) &
															 3] &
											  wp[2]);
										pw[4] |=
											(bitlookuph[0][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.h >> 6) &
															 3])) &
											wp[2];
										pw[3] &= ~(masklookuph[0][b5.b.l & 3] &
												   (wp[1] >> 16));
										pw[3] |=
											(bitlookuph[0][b1.b.l & 3][b1.b.h &
																	   3] |
											 (col &
											  masklookuph[0][b5.b.l & 3])) &
											(wp[1] >> 16);
										pw[2] &=
											~(masklookuph[0][(b5.b.l >> 2) &
															 3] &
											  wp[1]);
										pw[2] |=
											(bitlookuph[0][(b1.b.l >> 2) &
														   3][(b1.b.h >> 2) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.l >> 2) &
															 3])) &
											wp[1];
										pw[1] &=
											~(masklookuph[0][(b5.b.l >> 4) &
															 3] &
											  (wp[0] >> 16));
										pw[1] |=
											(bitlookuph[0][(b1.b.l >> 4) &
														   3][(b1.b.h >> 4) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.l >> 4) &
															 3])) &
											(wp[0] >> 16);
										pw[0] &=
											~(masklookuph[0][(b5.b.l >> 6) &
															 3] &
											  wp[0]);
										pw[0] |=
											(bitlookuph[0][(b1.b.l >> 6) &
														   3][(b1.b.h >> 6) &
															  3] |
											 (col &
											  masklookuph[0][(b5.b.l >> 6) &
															 3])) &
											wp[0];
									}
								}
								break;
							case 8:
								tile <<= 5;
								if (dat & 0x8000)
									tile += ((l & 7) ^ 7) + ppu.chr[c];
								else
									tile += (l & 7) + ppu.chr[c];
								b1.w = vram[tile];
								b2.w = vram[tile + 8];
								b3.w = vram[tile + 16];
								b4.w = vram[tile + 24];
								b5.w = b1.w | b2.w | b3.w | b4.w;
								b5.b.l |= b5.b.h;
								if (b5.w) {
									col = arith;
									if (dat & 0x4000) {
										p[0] &= ~(masklookup[1][b5.b.l & 3] &
												  wp[0]);
										p[0] |= (bitlookup[1][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[1][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (bitlookup[1][b3.b.l &
															   3][b3.b.h & 3]
												  << 4) |
												 (bitlookup[1][b4.b.l &
															   3][b4.b.h & 3]
												  << 6) |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[0];
										p[1] &=
											~(masklookup[1][(b5.b.l >> 2) & 3] &
											  wp[1]);
										p[1] |=
											(bitlookup[1][(b1.b.l >> 2) &
														  3][(b1.b.h >> 2) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 2) &
														   3][(b2.b.h >> 2) & 3]
											  << 2) |
											 (bitlookup[1][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) & 3]
											  << 4) |
											 (bitlookup[1][(b4.b.l >> 2) &
														   3][(b4.b.h >> 2) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 2) &
															3])) &
											wp[1];
										p[2] &=
											~(masklookup[1][(b5.b.l >> 4) & 3] &
											  wp[2]);
										p[2] |=
											(bitlookup[1][(b1.b.l >> 4) &
														  3][(b1.b.h >> 4) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 4) &
														   3][(b2.b.h >> 4) & 3]
											  << 2) |
											 (bitlookup[1][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) & 3]
											  << 4) |
											 (bitlookup[1][(b4.b.l >> 4) &
														   3][(b4.b.h >> 4) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 4) &
															3])) &
											wp[2];
										p[3] &=
											~(masklookup[1][(b5.b.l >> 6) & 3] &
											  wp[3]);
										p[3] |=
											(bitlookup[1][(b1.b.l >> 6) &
														  3][(b1.b.h >> 6) &
															 3] |
											 (bitlookup[1][(b2.b.l >> 6) &
														   3][(b2.b.h >> 6) & 3]
											  << 2) |
											 (bitlookup[1][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) & 3]
											  << 4) |
											 (bitlookup[1][(b4.b.l >> 6) &
														   3][(b4.b.h >> 6) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 6) &
															3])) &
											wp[3];
									} else {
										p[3] &= ~(masklookup[0][b5.b.l & 3] &
												  wp[3]);
										p[3] |= (bitlookup[0][b1.b.l &
															  3][b1.b.h & 3] |
												 (bitlookup[0][b2.b.l &
															   3][b2.b.h & 3]
												  << 2) |
												 (bitlookup[0][b3.b.l &
															   3][b3.b.h & 3]
												  << 4) |
												 (bitlookup[0][b4.b.l &
															   3][b4.b.h & 3]
												  << 6) |
												 (col &
												  masklookup[0][b3.b.l & 3])) &
												wp[3];
										p[2] &=
											~(masklookup[0][(b5.b.l >> 2) & 3] &
											  wp[2]);
										p[2] |=
											(bitlookup[0][(b1.b.l >> 2) &
														  3][(b1.b.h >> 2) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 2) &
														   3][(b2.b.h >> 2) & 3]
											  << 2) |
											 (bitlookup[0][(b3.b.l >> 2) &
														   3][(b3.b.h >> 2) & 3]
											  << 4) |
											 (bitlookup[0][(b4.b.l >> 2) &
														   3][(b4.b.h >> 2) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 2) &
															3])) &
											wp[2];
										p[1] &=
											~(masklookup[0][(b5.b.l >> 4) & 3] &
											  wp[1]);
										p[1] |=
											(bitlookup[0][(b1.b.l >> 4) &
														  3][(b1.b.h >> 4) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 4) &
														   3][(b2.b.h >> 4) & 3]
											  << 2) |
											 (bitlookup[0][(b3.b.l >> 4) &
														   3][(b3.b.h >> 4) & 3]
											  << 4) |
											 (bitlookup[0][(b4.b.l >> 4) &
														   3][(b4.b.h >> 4) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 4) &
															3])) &
											wp[1];
										p[0] &=
											~(masklookup[0][(b5.b.l >> 6) & 3] &
											  wp[0]);
										p[0] |=
											(bitlookup[0][(b1.b.l >> 6) &
														  3][(b1.b.h >> 6) &
															 3] |
											 (bitlookup[0][(b2.b.l >> 6) &
														   3][(b2.b.h >> 6) & 3]
											  << 2) |
											 (bitlookup[0][(b3.b.l >> 6) &
														   3][(b3.b.h >> 6) & 3]
											  << 4) |
											 (bitlookup[0][(b4.b.l >> 6) &
														   3][(b4.b.h >> 6) & 3]
											  << 6) |
											 (col &
											  masklookup[0][(b3.b.l >> 6) &
															3])) &
											wp[0];
									}
								}
								break;
							}
						skiptile:
							p += 4;
							wp += 4;
						}
					} else if (!pri) {
						pw = (uint16_t*)(((b->line[line])) + 128);
						pw2 = (uint16_t*)((window[c]) + 32);
						cx = (((int)ppu.m7x << 19) >> 19);
						cy = (((int)ppu.m7y << 19) >> 19);
						hoff = ((int)ppu.xscroll[0] << 19) >> 19;
						voff = ((int)ppu.yscroll[0] << 19) >> 19;
						ma = ((int)ppu.m7a << 16) >> 16;
						mb = ((int)ppu.m7b << 16) >> 16;
						mc = ((int)ppu.m7c << 16) >> 16;
						md = ((int)ppu.m7d << 16) >> 16;
						y = line + (voff - cy);
						bb = (mb * y) + (cx << 8);
						dd = (md * y) + (cy << 8);
						x = hoff - cx;
						aa = (ma * x);
						cc = (mc * x);
						for (x = 0; x < 256; x++) {
							xx = ((aa + bb) >> 8);
							yy = ((cc + dd) >> 8);
							if (!(ppu.m7sel & 0x80)) {
								xx &= 0x3ff;
								yy &= 0x3ff;
								temp = vramb[(((yy & ~7) << 5) |
											  ((xx & ~7) >> 2))];
								col = vramb[((temp << 7) + ((yy & 7) << 4) +
											 ((xx & 7) << 1) + 1)];
							} else {
								if ((xx | yy) & 0xFFFFFC00) {
									switch (ppu.m7sel >> 6) {
									case 2:
										col = 0;
										break;
									case 3:
										col = vramb[(((yy & 7) << 4) +
													 ((xx & 7) << 1) + 1) &
													0x7FFF];
										break;
									}
								} else {
									temp = vramb[(((yy & ~7) << 5) |
												  ((xx & ~7) >> 2))];
									col = vramb[((temp << 7) + ((yy & 7) << 4) +
												 ((xx & 7) << 1) + 1)];
								}
							}
							if (col && *pw2)
								*pw = (col | (uint16_t)arith);
							aa += ma;
							cc += mc;
							pw++;
							pw2++;
						}
					}
				}
			}
		}
		b = otherscr;
	}
	pw = (uint16_t*)otherscr->line[line];
	pw2 = (uint16_t*)subscr->line[line];
	pw3 = (uint16_t*)mainscr->line[line];
	switch (ppu.cgwsel & 0x30) {
	case 0x00:
		pw4 = (uint16_t*)window[7];
		break;
	case 0x10:
		pw4 = (uint16_t*)window[6];
		break;
	case 0x20:
		pw4 = (uint16_t*)window[5];
		break;
	case 0x30:
		pw4 = (uint16_t*)window[8];
		break;
	}
	// snemdebug("CGWSEL %02X\n",ppu.cgwsel&0x30);
	docolour(pw, pw2, pw3, pw4);
	if (line == 224)
		doblit();
	if (line < 225) /* Process HDMA */
		dohdma(line);
}

void dumphdma()
{
	int c;
	for (c = 0; c < 8; c++) {
		snemdebug("HDMA %i %s - src %06X dest %04X mode %02X stat %i len %i\n", c,
			   (hdmaena & (1 << c)) ? "on" : "off",
			   (dmabank[c] << 16) | dmasrc[c], dmadest[c], dmactrl[c],
			   hdmastat[c], hdmacount[c]);
	}
}

void writeppu(uint16_t addr, unsigned char val)
{
	int r, g, b, c;
	uint16_t tempaddr;
	// snemdebug("Write PPU %04X %02X %04X\n",addr,val,x.w);
	switch (addr & 0xFF) {
	case 0x00: /* Screen enable */
		snemlog("Screen enable %02X %06X %i\n", val, pbr | pc, lines);
		// if (val==0x80) { output=1; timetolive=5000; }
		ppu.screna = val;
		break;
	case 0x01: /* Sprite size */
		ppu.sprsize = val >> 5;
		ppu.sprbase = (val & 7) << 13;
		// snemdebug("Sprite size write %02X %06X\n",val,pbr|pc);
		break;
	case 0x02: /* Sprite address low */
		ppu.spraddr = (ppu.spraddr & 0x200) | (val << 1);
		ppu.spraddrs = ppu.spraddr;
		break;
	case 0x03: /* Sprite address high */
			   // snemdebug("Write sprite address %06X\n",pbr|pc);
		if (val & 1)
			ppu.spraddr |= 0x200;
		else
			ppu.spraddr &= ~0x200;
		// ppu.spraddr=(ppu.spraddr&0x1FE)|(val&0x200);
		ppu.spraddrs = ppu.spraddr;
		ppu.prirotation = val & 0x80;
		// snemlog("PRIROTATE %i\n",ppu.prirotation);
		break;
	case 0x04: /* Sprite data */
			   // snemdebug("Write SPR %04X %02X
			   // %06X\n",ppu.spraddr,val,pbr|pc);
		// snemdebug("SPR %02X %04X %06X\n",val,ppu.spraddr,pbr|pc);
		sprram[ppu.spraddr++] = val;
		if (ppu.spraddr >= 544)
			ppu.spraddr = 0;
		break;
	case 0x05: /* Screen mode */
		ppu.mode = val & 15;
		ppu.tilesize = val >> 4;
		// snemdebug("PPU mode %i %01X\n",ppu.mode,ppu.mode>>4);
		break;
	case 0x06: /* Mosaic */
		ppu.mosaic = val >> 4;
		break;
	case 0x07: /* BG1 address */
		ppu.bg[0] = (val & 0xFC) << 8;
		ppu.size[0] = val & 3;
		// snemdebug("BG0 %04X\n",ppu.bg[0]<<1);
		break;
	case 0x08: /* BG2 address */
		ppu.bg[1] = (val & 0xFC) << 8;
		ppu.size[1] = val & 3;
		// snemdebug("BG1 %04X\n",ppu.bg[1]<<1);
		break;
	case 0x09: /* BG3 address */
		ppu.bg[2] = (val & 0xFC) << 8;
		ppu.size[2] = val & 3;
		// snemdebug("BG2 %04X\n",ppu.bg[2]<<1);
		break;
	case 0x0A: /* BG4 address */
		ppu.bg[3] = (val & 0xFC) << 8;
		ppu.size[3] = val & 3;
		// snemdebug("BG3 %04X\n",ppu.bg[3]<<1);
		break;
	case 0x0B: /* BG1+2 address */
		ppu.chr[0] = (val & 0xF) << 12;
		ppu.chr[1] = (val & 0xF0) << 8;
		// snemdebug("CHR0 %04X\nCHR1
		// %04X\n",ppu.chr[0]<<1,ppu.chr[1]<<1);
		break;
	case 0x0C: /* BG3+4 address */
		ppu.chr[2] = (val & 0xF) << 12;
		ppu.chr[3] = (val & 0xF0) << 8;
		// snemdebug("CHR2 %04X\nCHR3
		// %04X\n",ppu.chr[2]<<1,ppu.chr[3]<<1);
		break;
	case 0x0D: /* BG1 xscroll */
		ppu.xscroll[0] = (ppu.xscroll[0] >> 8) | (val << 8);
		break;
	case 0x0E: /* BG1 yscroll */
		ppu.yscroll[0] = (ppu.yscroll[0] >> 8) | (val << 8);
		// snemdebug("BG1 yscroll %i %06X
		// %04X\n",ppu.yscroll[0],pbr|pc,dp);
		break;
	case 0x0F: /* BG2 xscroll */
		ppu.xscroll[1] = (ppu.xscroll[1] >> 8) | (val << 8);
		break;
	case 0x10: /* BG2 yscroll */
		ppu.yscroll[1] = (ppu.yscroll[1] >> 8) | (val << 8);
		// snemdebug("BG2 yscroll %i %06X\n",ppu.yscroll[2],pbr|pc);
		break;
	case 0x11: /* BG3 xscroll */
		ppu.xscroll[2] = (ppu.xscroll[2] >> 8) | (val << 8);
		// snemdebug("BG3 xscroll %i %06X\n",ppu.xscroll[2],pbr|pc);
		break;
	case 0x12: /* BG3 yscroll */
		ppu.yscroll[2] = (ppu.yscroll[2] >> 8) | (val << 8);
		// snemdebug("BG3 yscroll %i %06X\n",ppu.yscroll[2],pbr|pc);
		break;
	case 0x13: /* BG4 xscroll */
		ppu.xscroll[3] = (ppu.xscroll[3] >> 8) | (val << 8);
		break;
	case 0x14: /* BG4 yscroll */
		ppu.yscroll[3] = (ppu.yscroll[3] >> 8) | (val << 8);
		break;
	case 0x15: /* Video port control */
		ppu.portctrl = val;
		// snemdebug("Video port control %02X\n",val);
		switch (val & 3) {
		case 0:
			ppu.vinc = 1;
			break;
		case 1:
			ppu.vinc = 32;
			break;
		case 2:
		case 3:
			ppu.vinc = 128;
		}
		// snemdebug("vinc %i remap %i\n",ppu.vinc,(val>>2)&3);
		/* if (val&0xC)
		{
		snemdebug("Bad VRAM write mode %i\n",val&15);
		dumpregs();
		exit(-1);
} */
		break;
	case 0x16: /* VRAM address low */
		ppu.vramaddr = (ppu.vramaddr & 0xFF00) | val;
		// snemdebug("%06X VRAM addr %04X
		// %i\n",ppu.vramaddr,pbr|pc,ppu.vinc);
		ppu.firstread = 1;
		wroteaddr = pbr | pc;
		break;
	case 0x17: /* VRAM address high */
		ppu.vramaddr = (ppu.vramaddr & 0xFF) | (val << 8);
		// snemdebug("%06X VRAM addr %04X
		// %i\n",ppu.vramaddr,pbr|pc,ppu.vinc);
		ppu.firstread = 1;
		wroteaddr = pbr | pc;
		break;
	case 0x18:
		ppu.firstread = 1;
		// snemdebug("%04X",ppu.vramaddr);
		// if (((ppu.vramaddr<<1)&0xF000)==0x1000) { snemdebug("Write
		// %04X %02X %06X\n",(ppu.vramaddr<<1),val,pbr|pc); }
		// if (val==0x7F) { snemdebug("Write %04X 7F
		// %06X\n",ppu.vramaddr,pbr|pc); }
		// if (((ppu.vramaddr<<1)==0xE000) ||
		// ((ppu.vramaddr<<1)==0xD2B0) ||
		// ((ppu.vramaddr<<1)==0xC18E))
		// {
		// output=1;
		// timetolive=50;
		// }
		// snemdebug("Write %04X %02X %06X %06X %02X%02X%02X
		// %02X%02X\n",(ppu.vramaddr<<1),val,pbr|pc,wroteaddr,ram[0xE4],ram[0xE3],ram[0xE2],ram[0xEF],ram[0xEE]);
		tempaddr = ppu.vramaddr;
		switch (ppu.portctrl & 0xC) {
		case 0x4:
			tempaddr = (tempaddr & 0xff00) | ((tempaddr & 0x001f) << 3) |
					   ((tempaddr >> 5) & 7);
			break;
		case 0x8:
			tempaddr = (tempaddr & 0xfe00) | ((tempaddr & 0x003f) << 3) |
					   ((tempaddr >> 6) & 7);
			break;
		case 0xC:
			tempaddr = (tempaddr & 0xfc00) | ((tempaddr & 0x007f) << 3) |
					   ((tempaddr >> 7) & 7);
			break;
		}
		// if ((ppu.portctrl&0xC)==4) {
		//	tempaddr = (tempaddr & 0xff00) | ((tempaddr &
		//	0x001f) << 3) | ((tempaddr >> 5) & 7);
		/*	temp=tempaddr&0xFF00;
			temp|=((tempaddr&0x1F)<<3);
			tempaddr=temp|((temp>>5)&7); */
		//	}
		//	if (((tempaddr<<1)&0xFC00)==0x6000) { snemdebug("VRAM write
		//	%04X %02X %06X %04X\n",tempaddr<<1,val,pbr|pc,x.w); }
		vramb[(tempaddr << 1) & 0xFFFF] = val;
		if (!(ppu.portctrl & 0x80))
			ppu.vramaddr += ppu.vinc;
		break;
	case 0x19:
		ppu.firstread = 1;
		// snemdebug("%04X",ppu.vramaddr);
		// if ((ppu.vramaddr&~0x7FF)==ppu.bg[2]) { snemdebug("Write
		// %04X %02X %06X\n",(ppu.vramaddr<<1)+1,val,pbr|pc); }
		tempaddr = ppu.vramaddr;
		switch (ppu.portctrl & 0xC) {
		case 0x4:
			tempaddr = (tempaddr & 0xff00) | ((tempaddr & 0x001f) << 3) |
					   ((tempaddr >> 5) & 7);
			break;
		case 0x8:
			tempaddr = (tempaddr & 0xfe00) | ((tempaddr & 0x003f) << 3) |
					   ((tempaddr >> 6) & 7);
			break;
		case 0xC:
			tempaddr = (tempaddr & 0xfc00) | ((tempaddr & 0x007f) << 3) |
					   ((tempaddr >> 7) & 7);
			break;
		}
		// if ((ppu.portctrl&0xC)==4)
		// {
		//	tempaddr = (tempaddr & 0xff00) | ((tempaddr &
		//	0x001f) << 3) | ((tempaddr >> 5) & 7);
		/*	temp=tempaddr&0xFF00;
			temp|=((tempaddr&0x1F)<<3);
			tempaddr=temp|((temp>>5)&7); */
		//	}
		//	if (!tempaddr) snemdebug("VRAM write %04X %02X
		//	%06X\n",tempaddr<<1,val,pbr|pc);
		vramb[((tempaddr << 1) & 0xFFFF) | 1] = val;
		if (ppu.portctrl & 0x80)
			ppu.vramaddr += ppu.vinc;
		break;
	case 0x1A:
		ppu.m7sel = val;
		return;
	case 0x1B:
		ppu.m7a = (ppu.m7a >> 8) | (val << 8);
		ppu.matrixr = (int16_t)ppu.m7a * ((int16_t)ppu.m7b >> 8);
		// snemdebug("M7A %04X\n",ppu.m7a);
		return;
	case 0x1C:
		ppu.m7b = (ppu.m7b >> 8) | (val << 8);
		ppu.matrixr = (int16_t)ppu.m7a * ((int16_t)ppu.m7b >> 8);
		// snemdebug("M7B %04X\n",ppu.m7b);
		// m7write=1;
		return;
	case 0x1D:
		ppu.m7c = (ppu.m7c >> 8) | (val << 8);
		// snemdebug("M7C %04X\n",ppu.m7c);
		return;
	case 0x1E:
		ppu.m7d = (ppu.m7d >> 8) | (val << 8);
		// snemdebug("M7D %04X\n",ppu.m7d);
		return;
	case 0x1F:
		ppu.m7x = (ppu.m7x >> 8) | (val << 8);
		// snemdebug("M7X %04X\n",ppu.m7x);
		return;
	case 0x20:
		ppu.m7y = (ppu.m7y >> 8) | (val << 8);
		// snemdebug("M7Y %04X\n",ppu.m7y);
		return;
	case 0x21: /* Palette select */
		ppu.palindex = val;
		ppu.palbuffer = 0;
		break;
	case 0x22: /* Palette write */
		// snemdebug("2122 write %02x %06X
		// %i\n",val,pbr|pc,ppu.palindex);
		if (!ppu.palbuffer)
			ppu.palbuffer = val | 0x100;
		else {
			ppu.pal[ppu.palindex] = (val << 8) | (ppu.palbuffer & 0xFF);
			for (c = 0; c < 16; c++) {
				r = (int)((float)(ppu.pal[ppu.palindex] & 31) *
						  ((float)c / (float)15));
				g = (int)((float)((ppu.pal[ppu.palindex] >> 5) & 31) *
						  ((float)c / (float)15));
				b = (int)((float)((ppu.pal[ppu.palindex] >> 10) & 31) *
						  ((float)c / (float)15));
				pallookup[c][ppu.palindex] = makecol(r << 3, g << 3, b << 3);
			}
			// snemdebug("Pal %i = %04X %04X %i %i
			//	%i\n",ppu.palindex,ppu.pal[ppu.palindex],pallookup[ppu.palindex],r,g,b);
			ppu.palindex++;
			ppu.palindex &= 255;
			ppu.palbuffer = 0;
		}
		break;
	case 0x23: /* BG window enable */
		// snemdebug("Windena1 write %02X %06X
		// %i\n",val,pbr|pc,lastline);
		if (val != ppu.windena1)
			windowschanged = 1;
		ppu.windena1 = val;
		// windowschanged=1;
		break;
	case 0x24: /* BG window enable */
		// snemdebug("Windena2 write %02X %06X
		//	%i\n",val,pbr|pc,lastline);
		if (val != ppu.windena2)
			windowschanged = 1;
		ppu.windena2 = val;
		// windowschanged=1;
		break;
	case 0x25: /* BG window enable */
		// snemdebug("Windena3 write %02X %06X
		//	%i\n",val,pbr|pc,lastline);
		if (val != ppu.windena3)
			windowschanged = 1;
		ppu.windena3 = val;
		// windowschanged=1;
		break;
	case 0x26: /* Window 1 left */
		if (val != ppu.w1left)
			windowschanged = 1;
		ppu.w1left = val;
		// windowschanged=1;
		// snemdebug("W1L write %02X %06X\n",val,pbr|pc);
		break;
	case 0x27: /* Window 1 right */
		if (val != ppu.w1right)
			windowschanged = 1;
		ppu.w1right = val;
		// windowschanged=1;
		// snemdebug("W1R write %02X %06X\n",val,pbr|pc);
		break;
	case 0x28: /* Window 2 left */
		if (val != ppu.w2left)
			windowschanged = 1;
		ppu.w2left = val;
		// windowschanged=1;
		// snemdebug("W2L write %02X %06X\n",val,pbr|pc);
		break;
	case 0x29: /* Window 2 right */
		if (val != ppu.w2right)
			windowschanged = 1;
		ppu.w2right = val;
		// windowschanged=1;
		// snemdebug("W2R write %02X %06X\n",val,pbr|pc);
		break;
	case 0x2A: /* BG window logic */
		if (val != ppu.windlogic)
			windowschanged = 1;
		ppu.windlogic = val;
		// windowschanged=1;
		// snemdebug("WL write %02X %06X\n",val,pbr|pc);
		break;
	case 0x2B: /* BG window logic */
		if (val != ppu.windlogic2)
			windowschanged = 1;
		ppu.windlogic2 = val;
		// windowschanged=1;
		// snemdebug("WL2 write %02X %06X\n",val,pbr|pc);
		break;
	case 0x2C:
		ppu.main = val;
		// snemdebug("Main screen enable %02X %i\n",val,lastline);
		break;
	case 0x2D:
		ppu.sub = val;
		// snemdebug("Sub screen enable %02X %i\n",val,lastline);
		break;
	case 0x2E:
		ppu.wmaskmain = val;
		break;
	case 0x2F:
		ppu.wmasksub = val;
		break;
	case 0x30:
		ppu.cgwsel = val;
		// snemdebug("CGWSEL now %02X\n",val);
		break;
	case 0x31:
		ppu.cgadsub = val;
		// snemdebug("CGADSUB now %02X\n",val);
		break;
	case 0x32:
		if (val & 0x20)
			ppu.fixedc.r = (val & 0x1F) << 3;
		if (val & 0x40)
			ppu.fixedc.g = (val & 0x1F) << 3;
		if (val & 0x80)
			ppu.fixedc.b = (val & 0x1F) << 3;
		// snemdebug("FIXEDCOL %i %i
		// %i\n",ppu.fixedc.r,ppu.fixedc.g,ppu.fixedc.b);
		ppu.fixedcol = (((ppu.fixedc.r >> 3) << _rgb_r_shift_16) |
						((ppu.fixedc.g >> 2) << _rgb_g_shift_16) |
						((ppu.fixedc.b >> 3) << _rgb_b_shift_16));
		// ppu.fixedcol=makecol(ppu.fixedc.r,ppu.fixedc.g,ppu.fixedc.b);
		// snemdebug("FIXEDCOL %i %i %i
		// %04X\n",ppu.fixedc.r,ppu.fixedc.g,ppu.fixedc.b,ppu.fixedcol);
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
		writetospc(addr, val);
		break;
		setzf = 0;
		break;
	case 0x80: /* WRAM */
		// snemdebug("Write WRAM %05X %02X\n",ppu.wramaddr,val);
		ram[ppu.wramaddr & 0x1FFFF] = val;
		ppu.wramaddr++;
		break;
	case 0x81: /* WRAM addr low */
		ppu.wramaddr = (ppu.wramaddr & 0xFFFF00) | val;
		break;
	case 0x82: /* WRAM addr med */
		ppu.wramaddr = (ppu.wramaddr & 0xFF00FF) | (val << 8);
		break;
	case 0x83: /* WRAM addr high */
		ppu.wramaddr = (ppu.wramaddr & 0x00FFFF) | (val << 16);
		break;
	// default:
		// snemdebug("Write PPU %04X %02X\n",addr,val);
	}
	// snemdebug("\n");
}

int spcskip = 4;

unsigned char doskipper()
{
	int temp = spcskip;
	// snemdebug("Do skipper!\n");
	spcskip++;
	if (spcskip == 19)
		spcskip = 0;
	// skip&=3;
	// if (!(&1)) skip&=~1;
	// else        skip|=1;
	// snemdebug("Skipper %i %i  ",temp,temp>>1);
	switch (temp >> 1) {
	case 0:
	case 1:
		setzf = 2;
		return 0;
	case 2:
		if (temp & 1)
			return a.b.h;
		else
			return a.b.l;
		break;
	case 3:
		if (temp & 1)
			return x.b.h;
		else
			return x.b.l;
		break;
	case 4:
		if (temp & 1)
			return y.b.h;
		else
			return y.b.l;
		break;
	case 5:
		if (temp & 1)
			return 0xBB;
		else
			return 0xAA;
		break;
	case 6:
		setzf = 2;
		return 0;
	case 7:
		if (temp & 1)
			return 0xBB;
		else
			return 0xAA;
		break;
	case 8:
		if (temp & 1)
			return 0x33;
		else
			return 0x33;
		break;
	case 9:
		return 0;
	}
	snemlog("Shouldn't have got here %i %i\n", temp, spcskip);
	exit(-1);
}

unsigned char readppu(uint16_t addr)
{
	unsigned char temp;
	switch (addr & 0xFF) {
	case 0x34:
		// snemdebug("Read 2134\n");
		return ppu.matrixr;
	case 0x35:
		// snemdebug("Read 2134\n");
		return ppu.matrixr >> 8;
	case 0x36:
		// snemdebug("Read 2134\n");
		return ppu.matrixr >> 16;
	case 0x37: /* Latch v/h counters */
		vcount = lines;
		hcount = (1364 - cycles) >> 2;
		break;
	case 0x38: /* OAM data read */
		return sprram[ppu.spraddr++];
	case 0x39:
		if (ppu.firstread)
			temp = vramb[(ppu.vramaddr << 1) & 0xFFFF];
		else
			temp = vramb[((ppu.vramaddr << 1) - 2) & 0xFFFF];
		if (!(ppu.portctrl & 0x80)) {
			ppu.vramaddr += ppu.vinc;
			ppu.firstread = 0;
		}
		return temp;
	case 0x3A:
		if (ppu.firstread)
			temp = vramb[((ppu.vramaddr << 1) & 0xFFFF) | 1];
		else
			temp = vramb[(((ppu.vramaddr << 1) - 2) & 0xFFFF) | 1];
		if ((ppu.portctrl & 0x80)) {
			ppu.vramaddr += ppu.vinc;
			ppu.firstread = 0;
		}
		return temp;
	case 0x3D:
		temp = vcount & 0xFF;
		vcount >>= 8;
		return temp;
	case 0x3E:
		return 1;
	case 0x3F:
		if (pal)
			return 0x10;
		// snemdebug("Read type %06X\n",pbr|pc);
		return 0x00; /* NTSC */ // 0x10; /* PAL */
	case 0x40:
	case 0x42:
		return readfromspc(addr);
		return doskipper();

		spcskip++;
		if (spcskip == 41)
			spcskip = 0;
		// snemdebug("Read 2140 %i %i
		// %04X\n",spcskip,spcskip>>1,pc);
		switch (spcskip >> 1) {
		case 0:
			return a.b.l;
		case 1:
			return x.b.l;
		case 2:
			return y.b.l;
		case 3:
			return 0xFF;
		case 4:
			return 0x00;
		case 5:
			return 0x55;
		case 6:
			return 0xAA;
		case 7:
			return 1;
		case 8:
			return 0xAA;
		case 9:
			return 0xCD;
		case 10:
			return 0xBB;
		case 11:
			return 0xAA;
		case 12:
			return 7;
		case 13:
			return a.b.l;
		case 14:
			return 0xCC;
		case 15:
			return 0;
		case 16:
			return 0;
		case 17:
			return 3;
		case 18:
			return a.b.l;
		case 19:
			return a.b.l;
		case 20:
			return 2;
		}
		break;
	case 0x41:
	case 0x43:
		return readfromspc(addr);
		return doskipper();
		spcskip++;
		if (spcskip == 41)
			spcskip = 0;
		// snemdebug("Read 2141 %i %i
		// %04X\n",spcskip,spcskip>>1,pc);
		switch (spcskip >> 1) {
		case 0:
			return a.b.h;
		case 1:
			return x.b.h;
		case 2:
			return y.b.h;
		case 3:
			return 0xFF;
		case 4:
			return 2;
		case 5:
			return 0x55;
		case 6:
			return 0xAA;
		case 7:
			return 1;
		case 8:
			return 0xBB;
		case 9:
			return 0xCD;
		case 10:
			return 0xAA;
		case 11:
			return 0xBB;
		case 12:
			return 7;
		case 13:
			return a.b.l;
		case 14:
			return 0xCC;
		case 15:
			return 0;
		case 16:
			return 0;
		case 17:
			return 3;
		case 18:
			return a.b.h;
		case 19:
			return a.b.h;
		case 20:
			return 0;
		}
		break;
	case 0x80:
		temp = ram[ppu.wramaddr & 0x1FFFF];
		ppu.wramaddr++;
		return temp;

	default:
		return 0;
		snemlog("Read PPU %04X\n", addr);
		dumpregs();
		exit(-1);
	}
}

uint16_t getvramaddr()
{
	return ppu.vramaddr << 1;
}

BITMAP* dasbuffer;
void drawchar(int tile, int x, int y, int col)
{
	unsigned char dat, dat1, dat2, dat3, dat4;
	uint16_t addr = tile << 5;
	int yy, xx;
	if (!x)
		textprintf(dasbuffer, font, 128, y, makecol(255, 255, 255), "%04X",
				   addr);
	for (yy = 0; yy < 8; yy++) {
		dat1 = vramb[addr];
		dat2 = vramb[addr + 1];
		dat3 = vramb[addr + 16];
		dat4 = vramb[addr + 17];
		addr += 2;
		for (xx = 7; xx > -1; xx--) {
			dat = (dat1 & 1);
			dat |= ((dat2 & 1) << 1);
			dat |= ((dat3 & 1) << 2);
			dat |= ((dat4 & 1) << 3);
			dat |= (col << 4);
			dasbuffer->line[y + yy][x + xx] = dat;
			dat1 >>= 1;
			dat2 >>= 1;
			dat3 >>= 1;
			dat4 >>= 1;
		}
	}
}

void dumpchar()
{
	int page = 0, col = 0;
	int tile, x, y;
	while (key[KEY_F1])
		yield_timeslice();
	set_color_depth(8);
	dasbuffer = create_bitmap(256, 256);
	set_color_depth(16);
	clear(screen);
	while (!key[KEY_F10]) {
		if (key[KEY_UP]) {
			while (key[KEY_UP])
				yield_timeslice();
			page++;
			page &= 3;
		}
		if (key[KEY_DOWN]) {
			while (key[KEY_DOWN])
				yield_timeslice();
			page--;
			page &= 3;
		}
		if (key[KEY_LEFT]) {
			while (key[KEY_LEFT])
				yield_timeslice();
			col++;
			col &= 15;
		}
		if (key[KEY_RIGHT]) {
			while (key[KEY_RIGHT])
				yield_timeslice();
			col--;
			col &= 15;
		}
		if (key[KEY_D]) {
			while (key[KEY_D])
				yield_timeslice();
			desktop_palette[0].r = desktop_palette[0].g = desktop_palette[0].b =
				0;
			desktop_palette[15].r = desktop_palette[15].g =
				desktop_palette[15].b = 63;
			set_palette(desktop_palette);
		}
		if (key[KEY_F]) {
			while (key[KEY_F])
				yield_timeslice();
			// set_palette(ppu.pal);
		}
		tile = page << 9;
		// ppu.vram[0xf200]=ppu.vram[0xf201]=ppu.vram[0xf210]=ppu.vram[0xf211]=0xFF;
		for (y = 0; y < 32; y++) {
			for (x = 0; x < 16; x++) {
				drawchar(tile++, x << 3, y << 3, col);
			}
		}
		textprintf(dasbuffer, font, 128, 0, makecol(255, 255, 255), "Page %i",
				   page);
		textprintf(dasbuffer, font, 128, 8, makecol(255, 255, 255), "Col  %X",
				   col);
		blit(dasbuffer, screen, 0, 0, 0, 0, 256, 256);
	}
	clear(screen);
	destroy_bitmap(dasbuffer);
	// snemdebug("%01X\n",ppu.sdr&0xF);
}

void dumpbg2()
{
	int col = 0;
	int c, d;
	uint16_t addr = 0xB000 >> 1;
	int tile, x, y;
	unsigned char dat1, dat2;
	while (key[KEY_F2])
		yield_timeslice();
	set_color_depth(8);
	dasbuffer = create_bitmap(512, 512);
	set_color_depth(16);
	clear(screen);
	while (!key[KEY_F10]) {
		addr = 0xB000 >> 1;
		for (c = 0; c < 64; c++) {
			for (d = 0; d < 32; d++) {
				tile = vram[addr++];
				tile &= 0x3FF;
				tile = 0x6000 + (tile * 16);
				// tile>>=1;
				for (y = 0; y < 8; y++) {
					dat1 = vramb[tile];
					dat2 = vramb[tile + 1];
					tile += 2;
					for (x = 0; x < 8; x++) {
						col = (dat1 & 0x80) ? 1 : 0;
						col |= (dat2 & 0x80) ? 2 : 0;
						putpixel(dasbuffer, (d * 8) + x, (c * 8) + y, col);
						// dasbuffer->line[(c<<3)+y][(d<<3)+x]=col;
						dat1 <<= 1;
						dat2 <<= 1;
					}
				}
			}
		}
		for (c = 0; c < 64; c++)
			textprintf(dasbuffer, font, 256, c << 3, makecol(255, 255, 255),
					   "%04X", 0xB000 + (c * 64));
		blit(dasbuffer, screen, 0, 0, 0, 0, 512, 512);
	}
	clear(screen);
	destroy_bitmap(dasbuffer);
}
