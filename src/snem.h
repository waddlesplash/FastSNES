#pragma once

#include <stdint.h>

/* General */
void wakeupsoundthread();

/* SPC700 */
double spccycles;
double spctotal2;
double spctotal3;

void initspc();
void resetspc();
void execspc();
static inline void clockspc(int cyc)
{
	spccycles += cyc;
	if (spccycles > 0)
		execspc();
}
uint8_t readfromspc(uint16_t addr);
void writetospc(uint16_t addr, uint8_t val);

/* DSP */
void initdsp();
void resetdsp();
void writedsp(uint16_t a, uint8_t v);
uint8_t readdsp(uint16_t a);
void polldsp();

/* 65C816 */
/* Registers */
typedef union {
	uint16_t w;
	struct {
		uint8_t l, h;
	} b;
} reg;

reg a, x, y, s;
uint32_t pbr, dbr;
uint16_t pc, dp;

struct {
	int c, z, i, d, b, v, n, m, x, e;
} p;

extern int ins;
//extern int timetolive;
extern int inwai;

void nmi65c816();
void irq65c816();
void reset65c816();
void dumpregs();

/* Opcode table */
typedef void (*opcode_func)();
opcode_func opcodes[256][5];
void makeopcodetable();

/* CPU modes : 0 = X1M1
			  1 = X1M0
			  2 = X0M1
			  3 = X0M0
			  4 = emulation */
int cpumode;

/* Current opcode */
uint8_t opcode;
/* Global cycles count */
int cycles;

/* Memory */
uint8_t* ram;
uint8_t* rom;
uint8_t* memlookup[2048];
uint8_t memread[2048], memwrite[2048];
uint8_t accessspeed[2048];

int lorom;
void allocmem();
void loadrom(char* fn);
uint8_t readmeml(uint32_t a);
void writememl(uint32_t a, uint8_t v);

static inline uint8_t readmem(uint32_t a)
{
	// if (a==0xC05) { snemlog("Read %06X %06X
	// %02X\n",a,pbr|pc,ram[0xC05]); }
	/*        if (a>=0x7F0D50 && a<0x7F4000) */
	/*        if (pc==0xFB31 && a==0x7F139A)
			{
					output=1;
					timetolive=50;
			} */
	// if (ins==20) { snemdebug("4 %06X\n",pbr|pc); }
	// if ((a>>16)==0x7E) { snemdebug("Read %06X %06X\n",a,pbr|pc); }
	cycles -= accessspeed[(a >> 13) & 0x7FF];
	clockspc(accessspeed[(a >> 13) & 0x7FF]);
	// if ((a>>12)==0x19E) { snemdebug("%06X %08X %08X
	// %02X\n",a,memlookup[(a>>13)&0x7FF],memlookup[4],memlookup[(a>>13)&0x7FF][a&0x1FFF]); }
	if (memread[(a >> 13) & 0x7FF]) {
		// if (ins==20) { snemdebug("6
		// %02X\n",memlookup[(a>>13)&0x7FF][a&0x1FFF]); }
		return memlookup[(a >> 13) & 0x7FF][a & 0x1FFF];
	}
	// if (ins==20) { snemdebug("5 %06X\n",pbr|pc); }
	return readmeml(a);
}

static inline void writemem(uint32_t ad, uint8_t v)
{
	// if (ad==0xC05) { snemlog("WRiTE %04X %02X %06X\n",ad,v,pbr|pc);
	// if ((ad>>16)==0x7E) { snemdebug("WRiTE %04X %02X %06X\n",ad,v,pbr|pc); }
	// if ((ad&0xFFFF)==0x190C || (ad&0xFFFF)==0x190E ||
	// (ad&0xFFFF)==0x190F) { snemdebug ("WRITE %04X %02X %06X  %02X%02X%02X
	// %02X%02X%02X %04X %04X %04X
	// %04X\n",ad,v,pbr|pc,ram[0x916],ram[0x915],ram[0x914],ram[0x1C],ram[0x1B],ram[0x1A],dp,a.w,x.w,y.w); }
	// if ((ad&0xFFFF)==0x914 || (ad&0xFFFF)==0x915) { snemdebug ("WRITE %04X
	// %02X %06X  %02X%02X%02X %02X%02X%02X
	// %04X\n",ad,v,pbr|pc,ram[0x916],ram[0x915],ram[0x914],ram[0x1C],ram[0x1B],ram[0x1A],dp); }
	// if (ad>=0x7F0010 && ad<0x7F0012) { snemdebug ("WRITE %04X %02X %06X
	// %02X%02X%02X %02X%02X%02X %04X %i %i
	// %02X\n",ad,v,pbr|pc,ram[0x916],ram[0x915],ram[0x914],ram[0x1C],ram[0x1B],ram[0x1A],dp,p.x,p.m,dbr>>16); }
	// if ((ad>=0x2F && ad<0x30)) { snemdebug ("WRITE %04X %02X %06X
	// %02X%02X%02X %02X%02X%02X %04X %i %i
	// %02X\n",ad,v,pbr|pc,ram[0x916],ram[0x915],ram[0x914],ram[0x1C],ram[0x1B],ram[0x1A],dp,p.x,p.m,dbr>>16); }
	// if (ad>=0x7F139A && ad<=0x7F139B) { snemdebug ("WRITE %04X %02X %06X
	// %02X%02X%02X %02X%02X%02X %04X %i %i
	// %02X\n",ad,v,pbr|pc,ram[0x916],ram[0x915],ram[0x914],ram[0x1C],ram[0x1B],ram[0x1A],dp,p.x,p.m,dbr>>16); }
	// if (ad==0x7E2000) { snemdebug ("WRITE %04X %02X %06X   %04X %04X %04X
	// %04X %02X\n",ad,v,pbr|pc,dp,a.w,x.w,y.w,dbr>>16); }
	// if (ad==1) { snemdebug("Write to 1 %02X %06X\n",v,pbr|pc); }
	// if ((ad&0xFFFF)>=0x59 && (ad&0xFFFF)<=0x5C) { snemdebug ("WRITE %04X
	// %02X %06X\n",ad,v,pbr|pc); }
	// if (((ad&0xFFFF)>=0x1A05 && (ad&0xFFFF)<=0x1A0F) ||
	// (ad&0xFFFF)==0x679 || (ad&0xFFFF)==0x682 || (ad&0xFFFF)==0x654 ||
	// (ad&0xFFFF)==0x602 || (ad&0xFFFF)==0x11E || (ad&0xFFFF)==0x11D ||
	// (ad&0xFFFF)==0x109 || (ad&0xFFFF)==0x108) { snemdebug ("WRITE %04X %02X
	// %06X\n",ad,v,pbr|pc); }
	// if (((a>>16)&0x7F)<0x7E && (a&0x8000)) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	// if (a>=0x1E0 && a<=0x2E0) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	// if ((ad&0xFFFE)==0x7E) { snemdebug("Write %06X %02X %06X A
	// %04X\n",ad,v,pbr|pc,a.w); }
	// if (ad<=0x7EAEFF && ad>=0x7EAC00) { snemdebug("Write %06X %02X %06X A
	// %04X DBR %02X X %04X Y %04X\n",ad,v,pbr|pc,a.w,dbr,x.w,y.w); }
	// if (ad>=0x7E2500 && ad<=0x7E47E0) { snemdebug("Write %06X %02X %06X A
	// %04X DBR %02X X %04X Y %04X\n",ad,v,pbr|pc,a.w,dbr,x.w,y.w); }
	// if (ad>=0x7E7368 && ad<=0x7E9468) { snemdebug("Write %06X %02X %06X A
	// %04X DBR %02X X %04X Y %04X\n",ad,v,pbr|pc,a.w,dbr,x.w,y.w); }
	// if (a==0x93) { snemdebug("Write %06X %02X %06X\n",a,v,pbr|pc); }
	// if ((a&0xFFFFFF)==0x7E2568) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	// if (a==0x1DDE || a==0x1DDF) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	// if (a<0x7E8000 && a>=0x7E0000) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	// if (a<0x7F8000 && a>=0x7F0D50) { snemdebug("Write %06X %02X
	// %06X\n",a,v,pbr|pc); }
	cycles -= accessspeed[(ad >> 13) & 0x7FF];
	clockspc(accessspeed[(ad >> 13) & 0x7FF]);
	if (memwrite[(ad >> 13) & 0x7FF])
		memlookup[(ad >> 13) & 0x7FF][(ad)&0x1FFF] = v;
	else
		writememl(ad, v);
}

//#define readmem(a) readmeml(a)
//#define readmem(a) \
//(memread[((a)>>24)&255][((a)>>13)&7])?memlookup[((a)>>24)&255][((a)>>13)&7][(a)&0x1FFF]:readmeml(a)
#define readmemw(a) (readmem(a)) | ((readmem((a) + 1)) << 8)
//#define writemem(a,v) if (memread[((a)>>24)&255][((a)>>13)&7]) \
//memlookup[((a)>>24)&255][((a)>>13)&7][(a)&0x1FFF]=v; else writememl(a,v)
#define writememw(a, v)                                                        \
	writemem(a, (v)&0xFF);                                                     \
	writemem((a) + 1, (v) >> 8)
#define writememw2(a, v)                                                       \
	writemem((a) + 1, (v) >> 8);                                               \
	writemem(a, (v)&0xFF)

/* Video */
int nmi, vbl, joyscan;
int nmienable;
uint8_t* vramb;
uint16_t* vram;
int ppumask;

int yirq, xirq, irqenable, irq;
int lines;

int skipz, setzf;

int pal;

void initppu();
void resetppu();
uint8_t readppu(uint16_t addr);
void writeppu(uint16_t addr, uint8_t val);
void drawline(int line);

void dumpchar();
void dumpbg2();
void dumpvram();

/* DMA registers */
uint16_t dmadest[8], dmasrc[8], dmalen[8];
uint32_t hdmaaddr[8], hdmaaddr2[8];
uint8_t dmabank[8], dmaibank[8], dmactrl[8], hdmastat[8], hdmadat[8];
int hdmacount[8];
uint8_t hdmaena;

/* I/O */
void readjoy();

uint8_t readjoyold(uint16_t addr);
void writejoyold(uint16_t addr, uint8_t val);

uint8_t readio(uint16_t addr);
void writeio(uint16_t addr, uint8_t val);
