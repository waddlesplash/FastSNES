/* Snem 0.1 by Tom Walker
  65c816 emulation */

#include <stdio.h>
#include <stdlib.h>

#include "snem.h"
#include "util.h"

void updatecpumode();
int inwai = 0;
/* Temporary variables */
unsigned long addr;

/* Addressing modes */
unsigned long absolute()
{
	unsigned long temp = readmemw(pbr | pc);
	pc += 2;
	return temp | dbr;
}

unsigned long absolutex()
{
	unsigned long temp = (readmemw(pbr | pc)) + x.w + dbr;
	pc += 2;
	// if ((temp&0xFFFF)>0x2200 && (temp&0xFFFF)<0x8000) printf("ABSX
	// %04X %06X\n",x.w,temp);
	// if (output) printf("ABSX 0000,%04X - %06X\n",x.w,temp);
	// if (output) printf("Addr %06X\n",temp);
	return temp;
}

unsigned long absolutey()
{
	unsigned long temp = (readmemw(pbr | pc)) + y.w + dbr;
	pc += 2;
	return temp;
}

unsigned long absolutelong()
{
	unsigned long temp = readmemw(pbr | pc);
	pc += 2;
	temp |= (readmem(pbr | pc) << 16);
	pc++;
	return temp;
}

unsigned long absolutelongx()
{
	unsigned long temp = (readmemw(pbr | pc)) + x.w;
	pc += 2;
	temp += (readmem(pbr | pc) << 16);
	pc++;
	// printf("abslx %06X %04X\n",temp,x.w);
	return temp;
}

unsigned long
zeropage() /* It's actually direct page, but I'm used to calling it zero page */
{
	unsigned long temp = readmem(pbr | pc);
	pc++;
	temp += dp;
	if (dp & 0xFF) {
		cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

unsigned long zeropagex()
{
	unsigned long temp = readmem(pbr | pc) + x.w;
	pc++;
	if (p.e)
		temp &= 0xFF;
	temp += dp;
	if (dp & 0xFF) {
		cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

unsigned long zeropagey()
{
	unsigned long temp = readmem(pbr | pc) + y.w;
	pc++;
	if (p.e)
		temp &= 0xFF;
	temp += dp;
	if (dp & 0xFF) {
		cycles -= 6;
		clockspc(6);
	}
	return temp & 0xFFFF;
}

unsigned long stack()
{
	unsigned long temp = readmem(pbr | pc);
	pc++;
	temp += s.w;
	return temp & 0xFFFF;
}

unsigned long indirect()
{
	unsigned long temp = (readmem(pbr | pc) + dp) & 0xFFFF;
	pc++;
	return (readmemw(temp)) + dbr;
}

unsigned long indirectx()
{
	unsigned long temp = (readmem(pbr | pc) + dp + x.w) & 0xFFFF;
	pc++;
	return (readmemw(temp)) + dbr;
}
unsigned long jindirectx() /* JSR (,x) uses PBR instead of DBR, and 2 byte
							  address insted of 1 + dp */
{
	unsigned long temp =
		(readmem(pbr | pc) + (readmem((pbr | pc) + 1) << 8) + x.w) + pbr;
	pc += 2;
	// printf("Temp %06X\n",temp);
	return temp;
}

unsigned long indirecty()
{
	unsigned long temp = (readmem(pbr | pc) + dp) & 0xFFFF;
	pc++;
	return (readmemw(temp)) + y.w + dbr;
}
unsigned long sindirecty()
{
	unsigned long temp = (readmem(pbr | pc) + s.w) & 0xFFFF;
	pc++;
	return (readmemw(temp)) + y.w + dbr;
}

unsigned long indirectl()
{
	unsigned long temp = (readmem(pbr | pc) + dp) & 0xFFFF;
	pc++;
	unsigned long addr = readmemw(temp) | (readmem(temp + 2) << 16);
	// printf("IND %06X\n",addr);
	return addr;
}

unsigned long indirectly()
{
	unsigned long temp = (readmem(pbr | pc) + dp) & 0xFFFF;
	pc++;
	unsigned long addr = (readmemw(temp) | (readmem(temp + 2) << 16)) + y.w;
	if (pc == 0xFDC9)
		printf("INDy %04X %06X\n", temp, addr);
	// if (output) printf("INDy %06X %02X %06X\n",addr,opcode,pbr|pc);
	return addr;
}

/* Flag setting */
#define setzn8(v)                                                              \
	p.z = !(v);                                                                \
	p.n = (v)&0x80
#define setzn16(v)                                                             \
	p.z = !(v);                                                                \
	p.n = (v)&0x8000

/* ADC/SBC macros */
#define ADC8()                                                                 \
	tempw = a.b.l + temp + ((p.c) ? 1 : 0);                                    \
	p.v = (!((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));              \
	a.b.l = tempw & 0xFF;                                                      \
	setzn8(a.b.l);                                                             \
	p.c = tempw & 0x100;

#define ADC16()                                                                \
	templ = a.w + tempw + ((p.c) ? 1 : 0);                                     \
	p.v = (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));             \
	a.w = templ & 0xFFFF;                                                      \
	setzn16(a.w);                                                              \
	p.c = templ & 0x10000;

#define ADCBCD8()                                                              \
	tempw = (a.b.l & 0xF) + (temp & 0xF) + (p.c ? 1 : 0);                      \
	if (tempw > 9) {                                                           \
		tempw += 6;                                                            \
	}                                                                          \
	tempw += ((a.b.l & 0xF0) + (temp & 0xF0));                                 \
	if (tempw > 0x9F) {                                                        \
		tempw += 0x60;                                                         \
	}                                                                          \
	p.v = (!((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));              \
	a.b.l = tempw & 0xFF;                                                      \
	setzn8(a.b.l);                                                             \
	p.c = tempw > 0xFF;                                                        \
	cycles -= 6;                                                               \
	clockspc(6);

#define ADCBCD16()                                                             \
	templ = (a.w & 0xF) + (tempw & 0xF) + (p.c ? 1 : 0);                       \
	if (templ > 9) {                                                           \
		templ += 6;                                                            \
	}                                                                          \
	templ += ((a.w & 0xF0) + (tempw & 0xF0));                                  \
	if (templ > 0x9F) {                                                        \
		templ += 0x60;                                                         \
	}                                                                          \
	templ += ((a.w & 0xF00) + (tempw & 0xF00));                                \
	if (templ > 0x9FF) {                                                       \
		templ += 0x600;                                                        \
	}                                                                          \
	templ += ((a.w & 0xF000) + (tempw & 0xF000));                              \
	if (templ > 0x9FFF) {                                                      \
		templ += 0x6000;                                                       \
	}                                                                          \
	p.v = (!((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));             \
	a.w = templ & 0xFFFF;                                                      \
	setzn16(a.w);                                                              \
	p.c = templ > 0xFFFF;                                                      \
	cycles -= 6;                                                               \
	clockspc(6);

#define SBC8()                                                                 \
	tempw = a.b.l - temp - ((p.c) ? 0 : 1);                                    \
	p.v = (((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));               \
	a.b.l = tempw & 0xFF;                                                      \
	setzn8(a.b.l);                                                             \
	p.c = tempw <= 0xFF;

#define SBC16()                                                                \
	templ = a.w - tempw - ((p.c) ? 0 : 1);                                     \
	p.v = (((a.w ^ tempw) & (a.w ^ templ)) & 0x8000);                          \
	a.w = templ & 0xFFFF;                                                      \
	setzn16(a.w);                                                              \
	p.c = templ <= 0xFFFF;

#define SBCBCD8()                                                              \
	tempw = (a.b.l & 0xF) - (temp & 0xF) - (p.c ? 0 : 1);                      \
	if (tempw > 9) {                                                           \
		tempw -= 6;                                                            \
	}                                                                          \
	tempw += ((a.b.l & 0xF0) - (temp & 0xF0));                                 \
	if (tempw > 0x9F) {                                                        \
		tempw -= 0x60;                                                         \
	}                                                                          \
	p.v = (((a.b.l ^ temp) & 0x80) && ((a.b.l ^ tempw) & 0x80));               \
	a.b.l = tempw & 0xFF;                                                      \
	setzn8(a.b.l);                                                             \
	p.c = tempw <= 0xFF;                                                       \
	cycles -= 6;                                                               \
	clockspc(6);

#define SBCBCD16()                                                             \
	templ = (a.w & 0xF) - (tempw & 0xF) - (p.c ? 0 : 1);                       \
	if (templ > 9) {                                                           \
		templ -= 6;                                                            \
	}                                                                          \
	templ += ((a.w & 0xF0) - (tempw & 0xF0));                                  \
	if (templ > 0x9F) {                                                        \
		templ -= 0x60;                                                         \
	}                                                                          \
	templ += ((a.w & 0xF00) - (tempw & 0xF00));                                \
	if (templ > 0x9FF) {                                                       \
		templ -= 0x600;                                                        \
	}                                                                          \
	templ += ((a.w & 0xF000) - (tempw & 0xF000));                              \
	if (templ > 0x9FFF) {                                                      \
		templ -= 0x6000;                                                       \
	}                                                                          \
	p.v = (((a.w ^ tempw) & 0x8000) && ((a.w ^ templ) & 0x8000));              \
	a.w = templ & 0xFFFF;                                                      \
	setzn16(a.w);                                                              \
	p.c = templ <= 0xFFFF;                                                     \
	cycles -= 6;                                                               \
	clockspc(6);

/* Instructions */
void inca8()
{
	readmem(pbr | pc);
	a.b.l++;
	setzn8(a.b.l);
}
void inca16()
{
	readmem(pbr | pc);
	a.w++;
	setzn16(a.w);
}
void inx8()
{
	readmem(pbr | pc);
	x.b.l++;
	setzn8(x.b.l);
}
void inx16()
{
	readmem(pbr | pc);
	x.w++;
	setzn16(x.w);
}
void iny8()
{
	readmem(pbr | pc);
	y.b.l++;
	setzn8(y.b.l);
}
void iny16()
{
	readmem(pbr | pc);
	y.w++;
	setzn16(y.w);
}

void deca8()
{
	readmem(pbr | pc);
	a.b.l--;
	setzn8(a.b.l);
}
void deca16()
{
	readmem(pbr | pc);
	a.w--;
	setzn16(a.w);
}
void dex8()
{
	readmem(pbr | pc);
	x.b.l--;
	setzn8(x.b.l);
}
void dex16()
{
	readmem(pbr | pc);
	x.w--;
	setzn16(x.w);
}
void dey8()
{
	readmem(pbr | pc);
	y.b.l--;
	setzn8(y.b.l);
}
void dey16()
{
	readmem(pbr | pc);
	y.w--;
	setzn16(y.w);
}

/* INC group */
void incZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	writemem(addr, temp);
}
void incZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(addr, temp);
}

void incZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	writemem(addr, temp);
}
void incZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(addr, temp);
}

void incAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	writemem(addr, temp);
}
void incAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(addr, temp);
}

void incAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn8(temp);
	writemem(addr, temp);
}
void incAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp++;
	setzn16(temp);
	writememw2(addr, temp);
}

/* DEC group */
void decZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	writemem(addr, temp);
	// if (output && addr==4) printf("DEC 4 %02X %i %i\n",temp,p.z,p.n);
}
void decZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(addr, temp);
}

void decZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	writemem(addr, temp);
}
void decZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(addr, temp);
}

void decAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	writemem(addr, temp);
}
void decAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(addr, temp);
}

void decAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn8(temp);
	writemem(addr, temp);
}
void decAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	temp--;
	setzn16(temp);
	writememw2(addr, temp);
}

/* Flag group */
void clc()
{
	readmem(pbr | pc);
	p.c = 0;
}
void cld()
{
	readmem(pbr | pc);
	p.d = 0;
}
void cli()
{
	readmem(pbr | pc);
	p.i = 0;
}
void clv()
{
	readmem(pbr | pc);
	p.v = 0;
}

void sec()
{
	readmem(pbr | pc);
	p.c = 1;
}
void sed()
{
	readmem(pbr | pc);
	p.d = 1;
}
void sei()
{
	readmem(pbr | pc);
	p.i = 1;
}

void xce()
{
	int temp = p.c;
	p.c = p.e;
	p.e = temp;
	readmem(pbr | pc);
	updatecpumode();
}

void sep()
{
	unsigned char temp = readmem(pbr | pc);
	pc++;
	if (temp & 1)
		p.c = 1;
	if (temp & 2)
		p.z = 1;
	if (temp & 4)
		p.i = 1;
	if (temp & 8)
		p.d = 1;
	if (temp & 0x40)
		p.v = 1;
	if (temp & 0x80)
		p.n = 1;
	if (!p.e) {
		if (temp & 0x10)
			p.x = 1;
		if (temp & 0x20)
			p.m = 1;
		updatecpumode();
	}
}

void rep()
{
	unsigned char temp = readmem(pbr | pc);
	pc++;
	if (temp & 1)
		p.c = 0;
	if (temp & 2)
		p.z = 0;
	if (temp & 4)
		p.i = 0;
	if (temp & 8)
		p.d = 0;
	if (temp & 0x40)
		p.v = 0;
	if (temp & 0x80)
		p.n = 0;
	if (!p.e) {
		if (temp & 0x10)
			p.x = 0;
		if (temp & 0x20)
			p.m = 0;
		updatecpumode();
	}
}

/* Transfer group */
void tax8()
{
	readmem(pbr | pc);
	x.b.l = a.b.l;
	setzn8(x.b.l);
}
void tay8()
{
	readmem(pbr | pc);
	y.b.l = a.b.l;
	setzn8(y.b.l);
}
void txa8()
{
	readmem(pbr | pc);
	a.b.l = x.b.l;
	setzn8(a.b.l);
}
void tya8()
{
	readmem(pbr | pc);
	a.b.l = y.b.l;
	setzn8(a.b.l);
}
void tsx8()
{
	readmem(pbr | pc);
	x.b.l = s.b.l;
	setzn8(x.b.l);
}
void txs8()
{
	readmem(pbr | pc);
	s.b.l = x.b.l;
	// setzn8(s.b.l);
}
void txy8()
{
	readmem(pbr | pc);
	y.b.l = x.b.l;
	setzn8(y.b.l);
}
void tyx8()
{
	readmem(pbr | pc);
	x.b.l = y.b.l;
	setzn8(x.b.l);
}

void tax16()
{
	readmem(pbr | pc);
	x.w = a.w;
	setzn16(x.w);
}
void tay16()
{
	readmem(pbr | pc);
	y.w = a.w;
	setzn16(y.w);
}
void txa16()
{
	readmem(pbr | pc);
	a.w = x.w;
	setzn16(a.w);
}
void tya16()
{
	readmem(pbr | pc);
	a.w = y.w;
	setzn16(a.w);
}
void tsx16()
{
	readmem(pbr | pc);
	x.w = s.w;
	setzn16(x.w);
}
void txs16()
{
	readmem(pbr | pc);
	s.w = x.w;
	// setzn16(s.w);
}
void txy16()
{
	readmem(pbr | pc);
	y.w = x.w;
	setzn16(y.w);
}
void tyx16()
{
	readmem(pbr | pc);
	x.w = y.w;
	setzn16(x.w);
}

/* LDX group */
void ldxImm8()
{
	x.b.l = readmem(pbr | pc);
	pc++;
	setzn8(x.b.l);
}
void ldxZp8()
{
	addr = zeropage();
	x.b.l = readmem(addr);
	setzn8(x.b.l);
}
void ldxZpy8()
{
	addr = zeropagey();
	x.b.l = readmem(addr);
	setzn8(x.b.l);
}
void ldxAbs8()
{
	addr = absolute();
	x.b.l = readmem(addr);
	setzn8(x.b.l);
}
void ldxAbsy8()
{
	addr = absolutey();
	x.b.l = readmem(addr);
	setzn8(x.b.l);
}

void ldxImm16()
{
	x.w = readmemw(pbr | pc);
	pc += 2;
	setzn16(x.w);
}
void ldxZp16()
{
	addr = zeropage();
	x.w = readmemw(addr);
	setzn16(x.w);
}
void ldxZpy16()
{
	addr = zeropagey();
	x.w = readmemw(addr);
	setzn16(x.w);
}
void ldxAbs16()
{
	addr = absolute();
	x.w = readmemw(addr);
	setzn16(x.w);
}
void ldxAbsy16()
{
	addr = absolutey();
	x.w = readmemw(addr);
	setzn16(x.w);
}

/* LDY group */
void ldyImm8()
{
	y.b.l = readmem(pbr | pc);
	pc++;
	setzn8(y.b.l);
}
void ldyZp8()
{
	addr = zeropage();
	y.b.l = readmem(addr);
	setzn8(y.b.l);
}
void ldyZpx8()
{
	addr = zeropagex();
	y.b.l = readmem(addr);
	setzn8(y.b.l);
}
void ldyAbs8()
{
	addr = absolute();
	y.b.l = readmem(addr);
	setzn8(y.b.l);
}
void ldyAbsx8()
{
	addr = absolutex();
	y.b.l = readmem(addr);
	setzn8(y.b.l);
}

void ldyImm16()
{
	y.w = readmemw(pbr | pc);
	pc += 2;
	setzn16(y.w);
}
void ldyZp16()
{
	addr = zeropage();
	y.w = readmemw(addr);
	setzn16(y.w);
}
void ldyZpx16()
{
	addr = zeropagex();
	y.w = readmemw(addr);
	setzn16(y.w);
}
void ldyAbs16()
{
	addr = absolute();
	y.w = readmemw(addr);
	setzn16(y.w);
}
void ldyAbsx16()
{
	addr = absolutex();
	y.w = readmemw(addr);
	setzn16(y.w);
}

/* LDA group */
void ldaImm8()
{
	a.b.l = readmem(pbr | pc);
	pc++;
	setzn8(a.b.l);
}
void ldaZp8()
{
	addr = zeropage();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaZpx8()
{
	addr = zeropagex();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaSp8()
{
	addr = stack();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaSIndirecty8()
{
	addr = sindirecty();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaAbs8()
{
	addr = absolute();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaAbsx8()
{
	addr = absolutex();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaAbsy8()
{
	addr = absolutey();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaLong8()
{
	addr = absolutelong();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaLongx8()
{
	addr = absolutelongx();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaIndirect8()
{
	addr = indirect();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaIndirectx8()
{
	addr = indirectx();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaIndirecty8()
{
	addr = indirecty();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaIndirectLong8()
{
	addr = indirectl();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}
void ldaIndirectLongy8()
{
	addr = indirectly();
	a.b.l = readmem(addr);
	setzn8(a.b.l);
}

void ldaImm16()
{
	a.w = readmemw(pbr | pc);
	pc += 2;
	setzn16(a.w);
}
void ldaZp16()
{
	addr = zeropage();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaZpx16()
{
	addr = zeropagex();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaSp16()
{
	addr = stack();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaSIndirecty16()
{
	addr = sindirecty();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaAbs16()
{
	addr = absolute();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaAbsx16()
{
	addr = absolutex();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaAbsy16()
{
	addr = absolutey();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaLong16()
{
	addr = absolutelong();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaLongx16()
{
	addr = absolutelongx();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaIndirect16()
{
	addr = indirect();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaIndirectx16()
{
	addr = indirectx();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaIndirecty16()
{
	addr = indirecty();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaIndirectLong16()
{
	addr = indirectl();
	a.w = readmemw(addr);
	setzn16(a.w);
}
void ldaIndirectLongy16()
{
	addr = indirectly();
	a.w = readmemw(addr);
	if (pc == 0xFDC9)
		printf("LDA %06X %04X\n", addr, a.w);
	setzn16(a.w);
}

/* STA group */
void staZp8()
{
	addr = zeropage();
	writemem(addr, a.b.l);
}
void staZpx8()
{
	addr = zeropagex();
	writemem(addr, a.b.l);
}
void staAbs8()
{
	addr = absolute();
	writemem(addr, a.b.l);
}
void staAbsx8()
{
	addr = absolutex();
	writemem(addr, a.b.l);
}
void staAbsy8()
{
	addr = absolutey();
	writemem(addr, a.b.l);
}
void staLong8()
{
	addr = absolutelong();
	writemem(addr, a.b.l);
}
void staLongx8()
{
	addr = absolutelongx();
	writemem(addr, a.b.l);
}
void staIndirect8()
{
	addr = indirect();
	writemem(addr, a.b.l);
}
void staIndirectx8()
{
	addr = indirectx();
	writemem(addr, a.b.l);
}
void staIndirecty8()
{
	addr = indirecty();
	writemem(addr, a.b.l);
}
void staIndirectLong8()
{
	addr = indirectl();
	writemem(addr, a.b.l);
}
void staIndirectLongy8()
{
	addr = indirectly();
	writemem(addr, a.b.l);
}
void staSp8()
{
	addr = stack();
	writemem(addr, a.b.l);
}
void staSIndirecty8()
{
	addr = sindirecty();
	writemem(addr, a.b.l);
}

void staZp16()
{
	addr = zeropage();
	writememw(addr, a.w);
}
void staZpx16()
{
	addr = zeropagex();
	writememw(addr, a.w);
}
void staAbs16()
{
	addr = absolute();
	writememw(addr, a.w);
}
void staAbsx16()
{
	addr = absolutex();
	writememw(addr, a.w);
}
void staAbsy16()
{
	addr = absolutey();
	writememw(addr, a.w);
}
void staLong16()
{
	addr = absolutelong();
	writememw(addr, a.w);
}
void staLongx16()
{
	addr = absolutelongx();
	writememw(addr, a.w);
	// printf("Written %06X %04X %04X\n",addr,a.w,readmemw(addr));
}
void staIndirect16()
{
	addr = indirect();
	writememw(addr, a.w);
}
void staIndirectx16()
{
	addr = indirectx();
	writememw(addr, a.w);
}
void staIndirecty16()
{
	addr = indirecty();
	writememw(addr, a.w);
}
void staIndirectLong16()
{
	addr = indirectl();
	writememw(addr, a.w);
}
void staIndirectLongy16()
{
	addr = indirectly();
	writememw(addr, a.w);
}
void staSp16()
{
	addr = stack();
	writememw(addr, a.w);
}
void staSIndirecty16()
{
	addr = sindirecty();
	writememw(addr, a.w);
}

/* STX group */
void stxZp8()
{
	addr = zeropage();
	writemem(addr, x.b.l);
}
void stxZpy8()
{
	addr = zeropagey();
	writemem(addr, x.b.l);
}
void stxAbs8()
{
	addr = absolute();
	writemem(addr, x.b.l);
}

void stxZp16()
{
	addr = zeropage();
	writememw(addr, x.w);
}
void stxZpy16()
{
	addr = zeropagey();
	writememw(addr, x.w);
}
void stxAbs16()
{
	addr = absolute();
	writememw(addr, x.w);
}

/* STY group */
void styZp8()
{
	addr = zeropage();
	writemem(addr, y.b.l);
}
void styZpx8()
{
	addr = zeropagex();
	writemem(addr, y.b.l);
}
void styAbs8()
{
	addr = absolute();
	writemem(addr, y.b.l);
}

void styZp16()
{
	addr = zeropage();
	writememw(addr, y.w);
}
void styZpx16()
{
	addr = zeropagex();
	writememw(addr, y.w);
}
void styAbs16()
{
	addr = absolute();
	writememw(addr, y.w);
}

/* STZ group */
void stzZp8()
{
	addr = zeropage();
	writemem(addr, 0);
}
void stzZpx8()
{
	addr = zeropagex();
	writemem(addr, 0);
}
void stzAbs8()
{
	addr = absolute();
	writemem(addr, 0);
}
void stzAbsx8()
{
	addr = absolutex();
	writemem(addr, 0);
}

void stzZp16()
{
	addr = zeropage();
	writememw(addr, 0);
}
void stzZpx16()
{
	addr = zeropagex();
	writememw(addr, 0);
}
void stzAbs16()
{
	addr = absolute();
	writememw(addr, 0);
}
void stzAbsx16()
{
	addr = absolutex();
	writememw(addr, 0);
}

/* ADC group */
void adcImm8()
{
	unsigned short tempw;
	unsigned char temp = readmem(pbr | pc);
	pc++;
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcZp8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcZpx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcSp8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = stack();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcAbs8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcAbsx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcAbsy8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutey();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcLong8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutelong();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcLongx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutelongx();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcIndirect8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirect();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcIndirectx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectx();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcIndirecty8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirecty();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcsIndirecty8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = sindirecty();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcIndirectLong8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectl();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}
void adcIndirectLongy8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectly();
	temp = readmem(addr);
	if (p.d) {
		ADCBCD8();
	} else {
		ADC8();
	}
}

void adcImm16()
{
	unsigned long templ;
	unsigned short tempw;
	tempw = readmemw(pbr | pc);
	pc += 2;
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcZp16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = zeropage();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcZpx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = zeropagex();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcSp16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = stack();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcAbs16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolute();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcAbsx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutex();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcAbsy16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutey();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcLong16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutelong();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcLongx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutelongx();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcIndirect16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirect();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcIndirectx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectx();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcIndirecty16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirecty();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcsIndirecty16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = sindirecty();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcIndirectLong16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectl();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}
void adcIndirectLongy16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectly();
	tempw = readmemw(addr);
	if (p.d) {
		ADCBCD16();
	} else {
		ADC16();
	}
}

/* SBC group */
void sbcImm8()
{
	unsigned short tempw;
	unsigned char temp = readmem(pbr | pc);
	pc++;
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcZp8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcZpx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcSp8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = stack();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcAbs8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcAbsx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcAbsy8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutey();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcLong8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutelong();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcLongx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = absolutelongx();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcIndirect8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirect();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcIndirectx8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectx();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcIndirecty8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirecty();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcIndirectLong8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectl();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}
void sbcIndirectLongy8()
{
	unsigned short tempw;
	unsigned char temp;
	addr = indirectly();
	temp = readmem(addr);
	if (p.d) {
		SBCBCD8();
	} else {
		SBC8();
	}
}

void sbcImm16()
{
	unsigned long templ;
	unsigned short tempw;
	tempw = readmemw(pbr | pc);
	pc += 2;
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcZp16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = zeropage();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcZpx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = zeropagex();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcSp16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = stack();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcAbs16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolute();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcAbsx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutex();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcAbsy16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutey();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcLong16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutelong();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcLongx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = absolutelongx();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcIndirect16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirect();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcIndirectx16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectx();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcIndirecty16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirecty();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcIndirectLong16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectl();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}
void sbcIndirectLongy16()
{
	unsigned long templ;
	unsigned short tempw;
	addr = indirectly();
	tempw = readmemw(addr);
	if (p.d) {
		SBCBCD16();
	} else {
		SBC16();
	}
}

/* EOR group */
void eorImm8()
{
	a.b.l ^= readmem(pbr | pc);
	pc++;
	setzn8(a.b.l);
}
void eorZp8()
{
	addr = zeropage();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorZpx8()
{
	addr = zeropagex();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorSp8()
{
	addr = stack();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorAbs8()
{
	addr = absolute();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorAbsx8()
{
	addr = absolutex();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorAbsy8()
{
	addr = absolutey();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorLong8()
{
	addr = absolutelong();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorLongx8()
{
	addr = absolutelongx();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorIndirect8()
{
	addr = indirect();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorIndirectx8()
{
	addr = indirectx();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorIndirecty8()
{
	addr = indirecty();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorIndirectLong8()
{
	addr = indirectl();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}
void eorIndirectLongy8()
{
	addr = indirectly();
	a.b.l ^= readmem(addr);
	setzn8(a.b.l);
}

void eorImm16()
{
	a.w ^= readmemw(pbr | pc);
	pc += 2;
	setzn16(a.w);
}
void eorZp16()
{
	addr = zeropage();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorZpx16()
{
	addr = zeropagex();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorSp16()
{
	addr = stack();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorAbs16()
{
	addr = absolute();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorAbsx16()
{
	addr = absolutex();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorAbsy16()
{
	addr = absolutey();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorLong16()
{
	addr = absolutelong();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorLongx16()
{
	addr = absolutelongx();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorIndirect16()
{
	addr = indirect();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorIndirectx16()
{
	addr = indirectx();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorIndirecty16()
{
	addr = indirecty();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorIndirectLong16()
{
	addr = indirectl();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}
void eorIndirectLongy16()
{
	addr = indirectly();
	a.w ^= readmemw(addr);
	setzn16(a.w);
}

/* AND group */
void andImm8()
{
	a.b.l &= readmem(pbr | pc);
	pc++;
	setzn8(a.b.l);
}
void andZp8()
{
	addr = zeropage();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andZpx8()
{
	addr = zeropagex();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andSp8()
{
	addr = stack();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andAbs8()
{
	addr = absolute();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andAbsx8()
{
	addr = absolutex();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andAbsy8()
{
	addr = absolutey();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andLong8()
{
	addr = absolutelong();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andLongx8()
{
	addr = absolutelongx();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andIndirect8()
{
	addr = indirect();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andIndirectx8()
{
	addr = indirectx();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andIndirecty8()
{
	addr = indirecty();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andIndirectLong8()
{
	addr = indirectl();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}
void andIndirectLongy8()
{
	addr = indirectly();
	a.b.l &= readmem(addr);
	setzn8(a.b.l);
}

void andImm16()
{
	a.w &= readmemw(pbr | pc);
	pc += 2;
	setzn16(a.w);
}
void andZp16()
{
	addr = zeropage();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andZpx16()
{
	addr = zeropagex();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andSp16()
{
	addr = stack();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andAbs16()
{
	addr = absolute();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andAbsx16()
{
	addr = absolutex();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andAbsy16()
{
	addr = absolutey();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andLong16()
{
	addr = absolutelong();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andLongx16()
{
	addr = absolutelongx();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andIndirect16()
{
	addr = indirect();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andIndirectx16()
{
	addr = indirectx();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andIndirecty16()
{
	addr = indirecty();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andIndirectLong16()
{
	addr = indirectl();
	a.w &= readmemw(addr);
	setzn16(a.w);
}
void andIndirectLongy16()
{
	addr = indirectly();
	a.w &= readmemw(addr);
	setzn16(a.w);
}

/* ORA group */
void oraImm8()
{
	a.b.l |= readmem(pbr | pc);
	pc++;
	setzn8(a.b.l);
}
void oraZp8()
{
	addr = zeropage();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraZpx8()
{
	addr = zeropagex();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraSp8()
{
	addr = stack();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraAbs8()
{
	addr = absolute();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraAbsx8()
{
	addr = absolutex();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraAbsy8()
{
	addr = absolutey();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraLong8()
{
	addr = absolutelong();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraLongx8()
{
	addr = absolutelongx();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraIndirect8()
{
	addr = indirect();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraIndirectx8()
{
	addr = indirectx();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraIndirecty8()
{
	addr = indirecty();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraIndirectLong8()
{
	addr = indirectl();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}
void oraIndirectLongy8()
{
	addr = indirectly();
	a.b.l |= readmem(addr);
	setzn8(a.b.l);
}

void oraImm16()
{
	a.w |= readmemw(pbr | pc);
	pc += 2;
	setzn16(a.w);
}
void oraZp16()
{
	addr = zeropage();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraZpx16()
{
	addr = zeropagex();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraSp16()
{
	addr = stack();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraAbs16()
{
	addr = absolute();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraAbsx16()
{
	addr = absolutex();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraAbsy16()
{
	addr = absolutey();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraLong16()
{
	addr = absolutelong();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraLongx16()
{
	addr = absolutelongx();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraIndirect16()
{
	addr = indirect();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraIndirectx16()
{
	addr = indirectx();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraIndirecty16()
{
	addr = indirecty();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraIndirectLong16()
{
	addr = indirectl();
	a.w |= readmemw(addr);
	setzn16(a.w);
}
void oraIndirectLongy16()
{
	addr = indirectly();
	a.w |= readmemw(addr);
	setzn16(a.w);
}

/* BIT group */
void bitImm8()
{
	unsigned char temp = readmem(pbr | pc);
	pc++;
	p.z = !(temp & a.b.l);
	// setzf=0;
	// p.v=temp&0x40;
	// p.n=temp&0x80;
}
void bitImm16()
{
	unsigned short temp = readmemw(pbr | pc);
	pc += 2;
	p.z = !(temp & a.w);
	// printf("BIT %04X %04X %i\n",a.w,temp,p.z);
	setzf = 0;
	// p.v=temp&0x4000;
	// p.n=temp&0x8000;
}

void bitZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	p.z = !(temp & a.b.l);
	p.v = temp & 0x40;
	p.n = temp & 0x80;
}
void bitZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	p.z = !(temp & a.w);
	p.v = temp & 0x4000;
	p.n = temp & 0x8000;
}

void bitZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	p.z = !(temp & a.b.l);
	p.v = temp & 0x40;
	p.n = temp & 0x80;
}
void bitZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	p.z = !(temp & a.w);
	p.v = temp & 0x4000;
	p.n = temp & 0x8000;
}

void bitAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	p.z = !(temp & a.b.l);
	p.v = temp & 0x40;
	p.n = temp & 0x80;
}
void bitAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	p.z = !(temp & a.w);
	p.v = temp & 0x4000;
	p.n = temp & 0x8000;
}

void bitAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	p.z = !(temp & a.b.l);
	p.v = temp & 0x40;
	p.n = temp & 0x80;
}
void bitAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	p.z = !(temp & a.w);
	p.v = temp & 0x4000;
	p.n = temp & 0x8000;
}

/* CMP group */
void cmpImm8()
{
	unsigned char temp;
	temp = readmem(pbr | pc);
	pc++;
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpSp8()
{
	unsigned char temp;
	addr = stack();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpAbsy8()
{
	unsigned char temp;
	addr = absolutey();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpLong8()
{
	unsigned char temp;
	addr = absolutelong();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpLongx8()
{
	unsigned char temp;
	addr = absolutelongx();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpIndirect8()
{
	unsigned char temp;
	addr = indirect();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpIndirectx8()
{
	unsigned char temp;
	addr = indirectx();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpIndirecty8()
{
	unsigned char temp;
	addr = indirecty();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpIndirectLong8()
{
	unsigned char temp;
	addr = indirectl();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}
void cmpIndirectLongy8()
{
	unsigned char temp;
	addr = indirectly();
	temp = readmem(addr);
	setzn8(a.b.l - temp);
	p.c = (a.b.l >= temp);
}

void cmpImm16()
{
	unsigned short temp;
	temp = readmemw(pbr | pc);
	pc += 2;
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpSp16()
{
	unsigned short temp;
	addr = stack();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpAbsy16()
{
	unsigned short temp;
	addr = absolutey();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpLong16()
{
	unsigned short temp;
	addr = absolutelong();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpLongx16()
{
	unsigned short temp;
	addr = absolutelongx();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpIndirect16()
{
	unsigned short temp;
	addr = indirect();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpIndirectx16()
{
	unsigned short temp;
	addr = indirectx();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpIndirecty16()
{
	unsigned short temp;
	addr = indirecty();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpIndirectLong16()
{
	unsigned short temp;
	addr = indirectl();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}
void cmpIndirectLongy16()
{
	unsigned short temp;
	addr = indirectly();
	temp = readmemw(addr);
	setzn16(a.w - temp);
	p.c = (a.w >= temp);
}

/* Stack Group */
void phb()
{
	readmem(pbr | pc);
	writemem(s.w, dbr >> 16);
	s.w--;
	// printf("PHB %04X\n",s.w);
}
void phbe()
{
	readmem(pbr | pc);
	writemem(s.w, dbr >> 16);
	s.b.l--;
}

void phk()
{
	readmem(pbr | pc);
	writemem(s.w, pbr >> 16);
	s.w--;
	// printf("PHK %04X\n",s.w);
}
void phke()
{
	readmem(pbr | pc);
	writemem(s.w, pbr >> 16);
	s.b.l--;
}

void pea()
{
	addr = readmemw(pbr | pc);
	pc += 2;
	writemem(s.w, addr >> 8);
	s.w--;
	writemem(s.w, addr & 0xFF);
	s.w--;
	// printf("PEA %04X\n",s.w);
}

void pei()
{
	addr = indirect();
	writemem(s.w, addr >> 8);
	s.w--;
	writemem(s.w, addr & 0xFF);
	s.w--;
	// printf("PEI %04X\n",s.w);
}

void per()
{
	addr = readmemw(pbr | pc);
	pc += 2;
	addr += pc;
	writemem(s.w, addr >> 8);
	s.w--;
	writemem(s.w, addr & 0xFF);
	s.w--;
	// printf("PER %04X\n",s.w);
}

void phd()
{
	writemem(s.w, dp >> 8);
	s.w--;
	writemem(s.w, dp & 0xFF);
	s.w--;
	// printf("PHD %04X\n",s.w);
}
void pld()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	dp = readmem(s.w);
	s.w++;
	dp |= (readmem(s.w) << 8);
	// printf("PLD %04X\n",s.w);
}

void pha8()
{
	readmem(pbr | pc);
	writemem(s.w, a.b.l);
	s.w--;
	// printf("PHA %04X\n",s.w);
}
void pha16()
{
	readmem(pbr | pc);
	writemem(s.w, a.b.h);
	s.w--;
	writemem(s.w, a.b.l);
	s.w--;
	// printf("PHA %04X\n",s.w);
}

void phx8()
{
	readmem(pbr | pc);
	writemem(s.w, x.b.l);
	s.w--;
	// printf("PHX %04X\n",s.w);
}
void phx16()
{
	readmem(pbr | pc);
	writemem(s.w, x.b.h);
	s.w--;
	writemem(s.w, x.b.l);
	s.w--;
	// printf("PHX %04X\n",s.w);
}

void phy8()
{
	readmem(pbr | pc);
	writemem(s.w, y.b.l);
	s.w--;
	// printf("PHY %04X\n",s.w);
}
void phy16()
{
	readmem(pbr | pc);
	writemem(s.w, y.b.h);
	s.w--;
	writemem(s.w, y.b.l);
	s.w--;
	// printf("PHY %04X\n",s.w);
}

void pla8()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	a.b.l = readmem(s.w);
	setzn8(a.b.l);
	// printf("PLA %04X\n",s.w);
}
void pla16()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	a.b.l = readmem(s.w);
	s.w++;
	a.b.h = readmem(s.w);
	setzn16(a.w);
	// printf("PLA %04X\n",s.w);
}

void plx8()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	x.b.l = readmem(s.w);
	setzn8(x.b.l);
	// printf("PLX %04X\n",s.w);
}
void plx16()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	x.b.l = readmem(s.w);
	s.w++;
	x.b.h = readmem(s.w);
	setzn16(x.w);
	// printf("PLX %04X\n",s.w);
}

void ply8()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	y.b.l = readmem(s.w);
	setzn8(y.b.l);
	// printf("PLY %04X\n",s.w);
}
void ply16()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	y.b.l = readmem(s.w);
	s.w++;
	y.b.h = readmem(s.w);
	setzn16(y.w);
	// printf("PLY %04X\n",s.w);
}

void plb()
{
	readmem(pbr | pc);
	s.w++;
	cycles -= 6;
	clockspc(6);
	dbr = readmem(s.w) << 16;
	// printf("PLB %04X\n",s.w);
}
void plbe()
{
	readmem(pbr | pc);
	s.b.l++;
	cycles -= 6;
	clockspc(6);
	dbr = readmem(s.w) << 16;
}

void plp()
{
	unsigned char temp = readmem(s.w + 1);
	s.w++;
	p.c = temp & 1;
	p.z = temp & 2;
	p.i = temp & 4;
	p.d = temp & 8;
	p.x = temp & 0x10;
	p.m = temp & 0x20;
	p.v = temp & 0x40;
	p.n = temp & 0x80;
	cycles -= 12;
	clockspc(12);
	updatecpumode();
	// printf("PLP %04X\n",s.w);
}
void plpe()
{
	unsigned char temp;
	s.b.l++;
	temp = readmem(s.w);
	p.c = temp & 1;
	p.z = temp & 2;
	p.i = temp & 4;
	p.d = temp & 8;
	p.v = temp & 0x40;
	p.n = temp & 0x80;
	cycles -= 12;
	clockspc(12);
}

void php()
{
	unsigned char temp = (p.c) ? 1 : 0;
	if (p.z)
		temp |= 2;
	if (p.i)
		temp |= 4;
	if (p.d)
		temp |= 8;
	if (p.v)
		temp |= 0x40;
	if (p.n)
		temp |= 0x80;
	if (p.x)
		temp |= 0x10;
	if (p.m)
		temp |= 0x20;
	readmem(pbr | pc);
	writemem(s.w, temp);
	s.w--;
	// printf("PHP %04X\n",s.w);
}
void phpe()
{
	unsigned char temp = (p.c) ? 1 : 0;
	if (p.z)
		temp |= 2;
	if (p.i)
		temp |= 4;
	if (p.d)
		temp |= 8;
	if (p.v)
		temp |= 0x40;
	if (p.n)
		temp |= 0x80;
	temp |= 0x30;
	readmem(pbr | pc);
	writemem(s.w, temp);
	s.b.l--;
}

/* CPX group */
void cpxImm8()
{
	unsigned char temp = readmem(pbr | pc);
	pc++;
	setzn8(x.b.l - temp);
	p.c = (x.b.l >= temp);
}
void cpxImm16()
{
	unsigned short temp = readmemw(pbr | pc);
	pc += 2;
	setzn16(x.w - temp);
	p.c = (x.w >= temp);
}

void cpxZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	setzn8(x.b.l - temp);
	p.c = (x.b.l >= temp);
}
void cpxZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	setzn16(x.w - temp);
	p.c = (x.w >= temp);
}

void cpxAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	setzn8(x.b.l - temp);
	p.c = (x.b.l >= temp);
}
void cpxAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	setzn16(x.w - temp);
	p.c = (x.w >= temp);
}

/* CPY group */
void cpyImm8()
{
	unsigned char temp = readmem(pbr | pc);
	pc++;
	setzn8(y.b.l - temp);
	p.c = (y.b.l >= temp);
}
void cpyImm16()
{
	unsigned short temp = readmemw(pbr | pc);
	pc += 2;
	setzn16(y.w - temp);
	p.c = (y.w >= temp);
}

void cpyZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	setzn8(y.b.l - temp);
	p.c = (y.b.l >= temp);
}
void cpyZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	setzn16(y.w - temp);
	p.c = (y.w >= temp);
}

void cpyAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	setzn8(y.b.l - temp);
	p.c = (y.b.l >= temp);
}
void cpyAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	setzn16(y.w - temp);
	p.c = (y.w >= temp);
}

/* Branch group */
void bcc()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (!p.c) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void bcs()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (p.c) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void beq()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (setzf > 0)
		p.z = 0;
	if (setzf < 0)
		p.z = 1;
	setzf = 0;
	if (p.z) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void bne()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	// if (pc==0x8D44) printf("BNE %i %i ",setzf,p.z);
	if (setzf > 0)
		p.z = 1;
	if (setzf < 0)
		p.z = 0;
	setzf = 0;
	// if (pc==0x8D44) printf("%i\n",p.z);
	// if (skipz) //printf("skipz ");
	if (!p.z) // && !skipz)
	{
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
	// if (skipz) //printf("%04X\n",pc);
	skipz = 0;
}
void bpl()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (!p.n) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void bmi()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (p.n) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void bvc()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (!p.v) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}
void bvs()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	if (p.v) {
		pc += temp;
		cycles -= 6;
		clockspc(6);
	}
}

void bra()
{
	signed char temp = (signed char)readmem(pbr | pc);
	pc++;
	pc += temp;
	cycles -= 6;
	clockspc(6);
}
void brl()
{
	unsigned short temp = readmemw(pbr | pc);
	pc += 2;
	pc += temp;
	cycles -= 6;
	clockspc(6);
}

/* Jump group */
void jmp()
{
	addr = readmemw(pbr | pc);
	pc = addr;
}

void jmplong()
{
	addr = readmemw(pbr | pc) | (readmem((pbr | pc) + 2) << 16);
	pc = addr & 0xFFFF;
	pbr = addr & 0xFF0000;
}

void jmpind()
{
	addr = readmemw(pbr | pc);
	pc = readmemw(addr);
}

void jmpindx()
{
	addr = (readmemw(pbr | pc)) + x.w + pbr;
	// //printf("Read %06X\n",addr);
	pc = readmemw(addr);
}

void jmlind()
{
	addr = readmemw(pbr | pc);
	pc = readmemw(addr);
	pbr = readmem(addr + 2) << 16;
}

void jsr()
{
	addr = readmemw(pbr | pc);
	pc++;
	readmem(pbr | pc);
	writemem(s.w, pc >> 8);
	s.w--;
	writemem(s.w, pc & 0xFF);
	s.w--;
	pc = addr;
	// printf("JSR %04X\n",s.w);
}
void jsre()
{
	addr = readmemw(pbr | pc);
	pc++;
	readmem(pbr | pc);
	writemem(s.w, pc >> 8);
	s.b.l--;
	writemem(s.w, pc & 0xFF);
	s.b.l--;
	pc = addr;
}

void jsrIndx()
{
	addr = jindirectx();
	pc--;
	// //printf("Addr %06X\n",addr);
	writemem(s.w, pc >> 8);
	s.w--;
	writemem(s.w, pc & 0xFF);
	s.w--;
	pc = readmemw(addr);
	// printf("JSR %04X\n",s.w);
	// //printf("PC %04X\n",pc);
}
void jsrIndxe()
{
	addr = jindirectx();
	pc--;
	writemem(s.w, pc >> 8);
	s.b.l--;
	writemem(s.w, pc & 0xFF);
	s.b.l--;
	pc = readmemw(addr);
}

void jsl()
{
	unsigned char temp;
	addr = readmemw(pbr | pc);
	pc += 2;
	temp = readmem(pbr | pc);
	writemem(s.w, pbr >> 16);
	s.w--;
	writemem(s.w, pc >> 8);
	s.w--;
	writemem(s.w, pc & 0xFF);
	s.w--;
	pc = addr;
	pbr = temp << 16;
	// printf("JSL %04X\n",s.w);
}

void jsle()
{
	unsigned char temp;
	addr = readmemw(pbr | pc);
	pc += 2;
	temp = readmem(pbr | pc);
	writemem(s.w, pbr >> 16);
	s.b.l--;
	writemem(s.w, pc >> 8);
	s.b.l--;
	writemem(s.w, pc & 0xFF);
	s.b.l--;
	pc = addr;
	pbr = temp << 16;
}

void rtl()
{
	cycles -= 18;
	clockspc(18);
	pc = readmemw(s.w + 1);
	s.w += 2;
	pbr = readmem(s.w + 1) << 16;
	s.w++;
	pc++;
	// printf("RTL %04X\n",s.w);
}
void rtle()
{
	cycles -= 18;
	clockspc(18);
	s.b.l++;
	pc = readmem(s.w);
	s.b.l++;
	pc |= (readmem(s.w) << 8);
	s.b.l++;
	pbr = readmem(s.w) << 16;
}

void rts()
{
	cycles -= 18;
	clockspc(18);
	pc = readmemw(s.w + 1);
	s.w += 2;
	pc++;
	// printf("RTS %04X\n",s.w);
}
void rtse()
{
	cycles -= 18;
	clockspc(18);
	s.b.l++;
	pc = readmem(s.w);
	s.b.l++;
	pc |= (readmem(s.w) << 8);
}

void rti()
{
	unsigned char temp;
	cycles -= 6;
	s.w++;
	clockspc(6);
	temp = readmem(s.w);
	// //printf("%04X -> %02X\n",s.w,temp);
	p.c = temp & 1;
	p.z = temp & 2;
	p.i = temp & 4;
	p.d = temp & 8;
	p.x = temp & 0x10;
	p.m = temp & 0x20;
	p.v = temp & 0x40;
	p.n = temp & 0x80;
	s.w++;
	pc = readmem(s.w); // //printf("%04X -> %02X\n",s.w,pc);
	s.w++;
	pc |= (readmem(s.w) << 8); // //printf("%04X -> %02X\n",s.w,pc>>8);
	s.w++;
	pbr = readmem(s.w) << 16; ////printf("%04X -> %02X\n",s.w,pbr>>16);
	updatecpumode();
	// printf("RTI %04X\n",s.w);
	// output=0;
	// //printf("RTI to %06X\n",pbr|pc);
}

/* Shift group */
void asla8()
{
	readmem(pbr | pc);
	p.c = a.b.l & 0x80;
	a.b.l <<= 1;
	setzn8(a.b.l);
}
void asla16()
{
	readmem(pbr | pc);
	p.c = a.w & 0x8000;
	a.w <<= 1;
	setzn16(a.w);
}

void aslZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x80;
	temp <<= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void aslZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x8000;
	temp <<= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void aslZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x80;
	temp <<= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void aslZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x8000;
	temp <<= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void aslAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x80;
	temp <<= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void aslAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x8000;
	temp <<= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void aslAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x80;
	temp <<= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void aslAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 0x8000;
	temp <<= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void lsra8()
{
	readmem(pbr | pc);
	p.c = a.b.l & 1;
	a.b.l >>= 1;
	setzn8(a.b.l);
}
void lsra16()
{
	readmem(pbr | pc);
	p.c = a.w & 1;
	a.w >>= 1;
	setzn16(a.w);
}

void lsrZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void lsrZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void lsrZpx8()
{
	unsigned char temp;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void lsrZpx16()
{
	unsigned short temp;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void lsrAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void lsrAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void lsrAbsx8()
{
	unsigned char temp;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void lsrAbsx16()
{
	unsigned short temp;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	p.c = temp & 1;
	temp >>= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void rola8()
{
	readmem(pbr | pc);
	addr = p.c;
	p.c = a.b.l & 0x80;
	a.b.l <<= 1;
	if (addr)
		a.b.l |= 1;
	setzn8(a.b.l);
}
void rola16()
{
	readmem(pbr | pc);
	addr = p.c;
	p.c = a.w & 0x8000;
	a.w <<= 1;
	if (addr)
		a.w |= 1;
	setzn16(a.w);
}

void rolZp8()
{
	unsigned char temp;
	int tempc;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x80;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void rolZp16()
{
	unsigned short temp;
	int tempc;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x8000;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void rolZpx8()
{
	unsigned char temp;
	int tempc;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x80;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void rolZpx16()
{
	unsigned short temp;
	int tempc;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x8000;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void rolAbs8()
{
	unsigned char temp;
	int tempc;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x80;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void rolAbs16()
{
	unsigned short temp;
	int tempc;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x8000;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void rolAbsx8()
{
	unsigned char temp;
	int tempc;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x80;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn8(temp);
	writemem(addr, temp);
}
void rolAbsx16()
{
	unsigned short temp;
	int tempc;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 0x8000;
	temp <<= 1;
	if (tempc)
		temp |= 1;
	setzn16(temp);
	writememw2(addr, temp);
}

void rora8()
{
	readmem(pbr | pc);
	addr = p.c;
	p.c = a.b.l & 1;
	a.b.l >>= 1;
	if (addr)
		a.b.l |= 0x80;
	setzn8(a.b.l);
}
void rora16()
{
	readmem(pbr | pc);
	addr = p.c;
	p.c = a.w & 1;
	a.w >>= 1;
	if (addr)
		a.w |= 0x8000;
	setzn16(a.w);
}

void rorZp8()
{
	unsigned char temp;
	int tempc;
	addr = zeropage();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x80;
	setzn8(temp);
	writemem(addr, temp);
}
void rorZp16()
{
	unsigned short temp;
	int tempc;
	addr = zeropage();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x8000;
	setzn16(temp);
	writememw2(addr, temp);
}

void rorZpx8()
{
	unsigned char temp;
	int tempc;
	addr = zeropagex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x80;
	setzn8(temp);
	writemem(addr, temp);
}
void rorZpx16()
{
	unsigned short temp;
	int tempc;
	addr = zeropagex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x8000;
	setzn16(temp);
	writememw2(addr, temp);
}

void rorAbs8()
{
	unsigned char temp;
	int tempc;
	addr = absolute();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x80;
	setzn8(temp);
	writemem(addr, temp);
}
void rorAbs16()
{
	unsigned short temp;
	int tempc;
	addr = absolute();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x8000;
	setzn16(temp);
	writememw2(addr, temp);
}

void rorAbsx8()
{
	unsigned char temp;
	int tempc;
	addr = absolutex();
	temp = readmem(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x80;
	setzn8(temp);
	writemem(addr, temp);
}
void rorAbsx16()
{
	unsigned short temp;
	int tempc;
	addr = absolutex();
	temp = readmemw(addr);
	cycles -= 6;
	clockspc(6);
	tempc = p.c;
	p.c = temp & 1;
	temp >>= 1;
	if (tempc)
		temp |= 0x8000;
	setzn16(temp);
	writememw2(addr, temp);
}

/* Misc group */
void xba()
{
	readmem(pbr | pc);
	a.w = (a.w >> 8) | (a.w << 8);
	setzn8(a.b.l);
}
void nop()
{
	cycles -= 6;
	clockspc(6);
}

void tcd()
{
	readmem(pbr | pc);
	dp = a.w;
	setzn16(dp);
}

void tdc()
{
	readmem(pbr | pc);
	a.w = dp;
	setzn16(a.w);
}

void tcs()
{
	readmem(pbr | pc);
	s.w = a.w;
}

void tsc()
{
	readmem(pbr | pc);
	a.w = s.w;
	setzn16(a.w);
}

void trbZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	p.z = !(a.b.l & temp);
	temp &= ~a.b.l;
	cycles -= 6;
	clockspc(6);
	writemem(addr, temp);
}
void trbZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	p.z = !(a.w & temp);
	temp &= ~a.w;
	cycles -= 6;
	clockspc(6);
	writememw2(addr, temp);
}

void trbAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	p.z = !(a.b.l & temp);
	temp &= ~a.b.l;
	cycles -= 6;
	clockspc(6);
	writemem(addr, temp);
}
void trbAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	p.z = !(a.w & temp);
	temp &= ~a.w;
	cycles -= 6;
	clockspc(6);
	writememw2(addr, temp);
}

void tsbZp8()
{
	unsigned char temp;
	addr = zeropage();
	temp = readmem(addr);
	p.z = !(a.b.l & temp);
	temp |= a.b.l;
	cycles -= 6;
	clockspc(6);
	writemem(addr, temp);
}
void tsbZp16()
{
	unsigned short temp;
	addr = zeropage();
	temp = readmemw(addr);
	p.z = !(a.w & temp);
	temp |= a.w;
	cycles -= 6;
	clockspc(6);
	writememw2(addr, temp);
}

void tsbAbs8()
{
	unsigned char temp;
	addr = absolute();
	temp = readmem(addr);
	p.z = !(a.b.l & temp);
	temp |= a.b.l;
	cycles -= 6;
	clockspc(6);
	writemem(addr, temp);
}
void tsbAbs16()
{
	unsigned short temp;
	addr = absolute();
	temp = readmemw(addr);
	p.z = !(a.w & temp);
	temp |= a.w;
	cycles -= 6;
	clockspc(6);
	writememw2(addr, temp);
}

void wai()
{
	readmem(pbr | pc);
	inwai = 1;
	pc--;
	// printf("WAI %06X\n",pbr|pc);
}

void mvp()
{
	unsigned char temp;
	dbr = (readmem(pbr | pc)) << 16;
	pc++;
	addr = (readmem(pbr | pc)) << 16;
	pc++;
	temp = readmem(addr | x.w);
	writemem(dbr | y.w, temp);
	x.w--;
	y.w--;
	a.w--;
	if (a.w != 0xFFFF)
		pc -= 3;
	cycles -= 12;
	clockspc(12);
}

void mvn()
{
	unsigned char temp;
	dbr = (readmem(pbr | pc)) << 16;
	pc++;
	addr = (readmem(pbr | pc)) << 16;
	pc++;
	temp = readmem(addr | x.w);
	writemem(dbr | y.w, temp);
	x.w++;
	y.w++;
	a.w--;
	if (a.w != 0xFFFF)
		pc -= 3;
	cycles -= 12;
	clockspc(12);
}

void brk()
{
	unsigned char temp = 0;
	writemem(s.w, pbr >> 16);
	s.w--;
	writemem(s.w, pc >> 8);
	s.w--;
	writemem(s.w, pc & 0xFF);
	s.w--;
	if (p.c)
		temp |= 1;
	if (p.z)
		temp |= 2;
	if (p.i)
		temp |= 4;
	if (p.d)
		temp |= 8;
	if (p.x)
		temp |= 0x10;
	if (p.m)
		temp |= 0x20;
	if (p.v)
		temp |= 0x40;
	if (p.n)
		temp |= 0x80;
	writemem(s.w, temp);
	s.w--;
	pc = readmemw(0xFFE6);
	pbr = 0;
	p.i = 1;
	p.d = 0;
}

/* Functions */
void reset65c816()
{
	pbr = dbr = 0;
	s.w = 0x1FF;
	cpumode = 4;
	p.e = 1;
	p.i = 1;
	pc = readmemw(0xFFFC);
	a.w = x.w = y.w = 0;
	p.x = p.m = 1;
	skipz = 0;
	printf("Reset to %04X\n", pc);
	// exit(-1);
}

void dumpregs()
{
	FILE* f = fopen("ram.dmp", "wb");
	fwrite(ram, 0x20000, 1, f);
	fclose(f);
	snemlog("A=%04X X=%04X Y=%04X S=%04X\n", a.w, x.w, y.w, s.w);
	snemlog("PC=%06X DBR=%02X DP=%04X  %i ins\n", pc | pbr, dbr >> 24, dp, ins);
	snemlog("%c %c %c %i\n", (p.e) ? 'E' : ' ', (p.x) ? 'X' : ' ',
			(p.m) ? 'M' : ' ', cpumode);
	dumpvram();
	// printf("89272=%02X\n",readmem(0x89272));
}

void badopcode()
{
	// FILE *f=fopen("rom.dmp","wb");
	snemlog("Bad opcode %02X\n", opcode);
	pc--;
	dumpregs();
	// printf("%02X
	// %06X\n",readmem(0x3F8A82),rom[((0x3F8A82>>16)*0x8000)+(((0x3F8A82>>12)&3)*0x2000)+(0x3F8A82&0x1FFF)]);
	// fwrite(rom,2048*1024,1,f);
	// fclose(f);
	exit(-1);
}

void makeopcodetable()
{
	int c, d;
	for (c = 0; c < 256; c++) {
		for (d = 0; d < 5; d++) {
			opcodes[c][d] = badopcode;
		}
	}
	/* LDA group */
	opcodes[0xA9][0] = opcodes[0xA9][2] = opcodes[0xA9][4] = ldaImm8;
	opcodes[0xA9][1] = opcodes[0xA9][3] = ldaImm16;
	opcodes[0xA5][0] = opcodes[0xA5][2] = opcodes[0xA5][4] = ldaZp8;
	opcodes[0xA5][1] = opcodes[0xA5][3] = ldaZp16;
	opcodes[0xB5][0] = opcodes[0xB5][2] = opcodes[0xB5][4] = ldaZpx8;
	opcodes[0xB5][1] = opcodes[0xB5][3] = ldaZpx16;
	opcodes[0xA3][0] = opcodes[0xA3][2] = opcodes[0xA3][4] = ldaSp8;
	opcodes[0xA3][1] = opcodes[0xA3][3] = ldaSp16;
	opcodes[0xB3][0] = opcodes[0xB3][2] = opcodes[0xB3][4] = ldaSIndirecty8;
	opcodes[0xB3][1] = opcodes[0xB3][3] = ldaSIndirecty16;
	opcodes[0xAD][0] = opcodes[0xAD][2] = opcodes[0xAD][4] = ldaAbs8;
	opcodes[0xAD][1] = opcodes[0xAD][3] = ldaAbs16;
	opcodes[0xBD][0] = opcodes[0xBD][2] = opcodes[0xBD][4] = ldaAbsx8;
	opcodes[0xBD][1] = opcodes[0xBD][3] = ldaAbsx16;
	opcodes[0xB9][0] = opcodes[0xB9][2] = opcodes[0xB9][4] = ldaAbsy8;
	opcodes[0xB9][1] = opcodes[0xB9][3] = ldaAbsy16;
	opcodes[0xAF][0] = opcodes[0xAF][2] = opcodes[0xAF][4] = ldaLong8;
	opcodes[0xAF][1] = opcodes[0xAF][3] = ldaLong16;
	opcodes[0xBF][0] = opcodes[0xBF][2] = opcodes[0xBF][4] = ldaLongx8;
	opcodes[0xBF][1] = opcodes[0xBF][3] = ldaLongx16;
	opcodes[0xB2][0] = opcodes[0xB2][2] = opcodes[0xB2][4] = ldaIndirect8;
	opcodes[0xB2][1] = opcodes[0xB2][3] = ldaIndirect16;
	opcodes[0xA1][0] = opcodes[0xA1][2] = opcodes[0xA1][4] = ldaIndirectx8;
	opcodes[0xA1][1] = opcodes[0xA1][3] = ldaIndirectx16;
	opcodes[0xB1][0] = opcodes[0xB1][2] = opcodes[0xB1][4] = ldaIndirecty8;
	opcodes[0xB1][1] = opcodes[0xB1][3] = ldaIndirecty16;
	opcodes[0xA7][0] = opcodes[0xA7][2] = opcodes[0xA7][4] = ldaIndirectLong8;
	opcodes[0xA7][1] = opcodes[0xA7][3] = ldaIndirectLong16;
	opcodes[0xB7][0] = opcodes[0xB7][2] = opcodes[0xB7][4] = ldaIndirectLongy8;
	opcodes[0xB7][1] = opcodes[0xB7][3] = ldaIndirectLongy16;
	/* LDX group */
	opcodes[0xA2][0] = opcodes[0xA2][1] = opcodes[0xA2][4] = ldxImm8;
	opcodes[0xA2][2] = opcodes[0xA2][3] = ldxImm16;
	opcodes[0xA6][0] = opcodes[0xA6][1] = opcodes[0xA6][4] = ldxZp8;
	opcodes[0xA6][2] = opcodes[0xA6][3] = ldxZp16;
	opcodes[0xB6][0] = opcodes[0xB6][1] = opcodes[0xB6][4] = ldxZpy8;
	opcodes[0xB6][2] = opcodes[0xB6][3] = ldxZpy16;
	opcodes[0xAE][0] = opcodes[0xAE][1] = opcodes[0xAE][4] = ldxAbs8;
	opcodes[0xAE][2] = opcodes[0xAE][3] = ldxAbs16;
	opcodes[0xBE][0] = opcodes[0xBE][1] = opcodes[0xBE][4] = ldxAbsy8;
	opcodes[0xBE][2] = opcodes[0xBE][3] = ldxAbsy16;
	/* LDY group */
	opcodes[0xA0][0] = opcodes[0xA0][1] = opcodes[0xA0][4] = ldyImm8;
	opcodes[0xA0][2] = opcodes[0xA0][3] = ldyImm16;
	opcodes[0xA4][0] = opcodes[0xA4][1] = opcodes[0xA4][4] = ldyZp8;
	opcodes[0xA4][2] = opcodes[0xA4][3] = ldyZp16;
	opcodes[0xB4][0] = opcodes[0xB4][1] = opcodes[0xB4][4] = ldyZpx8;
	opcodes[0xB4][2] = opcodes[0xB4][3] = ldyZpx16;
	opcodes[0xAC][0] = opcodes[0xAC][1] = opcodes[0xAC][4] = ldyAbs8;
	opcodes[0xAC][2] = opcodes[0xAC][3] = ldyAbs16;
	opcodes[0xBC][0] = opcodes[0xBC][1] = opcodes[0xBC][4] = ldyAbsx8;
	opcodes[0xBC][2] = opcodes[0xBC][3] = ldyAbsx16;

	/* STA group */
	opcodes[0x85][0] = opcodes[0x85][2] = opcodes[0x85][4] = staZp8;
	opcodes[0x85][1] = opcodes[0x85][3] = staZp16;
	opcodes[0x95][0] = opcodes[0x95][2] = opcodes[0x95][4] = staZpx8;
	opcodes[0x95][1] = opcodes[0x95][3] = staZpx16;
	opcodes[0x8D][0] = opcodes[0x8D][2] = opcodes[0x8D][4] = staAbs8;
	opcodes[0x8D][1] = opcodes[0x8D][3] = staAbs16;
	opcodes[0x9D][0] = opcodes[0x9D][2] = opcodes[0x9D][4] = staAbsx8;
	opcodes[0x9D][1] = opcodes[0x9D][3] = staAbsx16;
	opcodes[0x99][0] = opcodes[0x99][2] = opcodes[0x99][4] = staAbsy8;
	opcodes[0x99][1] = opcodes[0x99][3] = staAbsy16;
	opcodes[0x8F][0] = opcodes[0x8F][2] = opcodes[0x8F][4] = staLong8;
	opcodes[0x8F][1] = opcodes[0x8F][3] = staLong16;
	opcodes[0x9F][0] = opcodes[0x9F][2] = opcodes[0x9F][4] = staLongx8;
	opcodes[0x9F][1] = opcodes[0x9F][3] = staLongx16;
	opcodes[0x92][0] = opcodes[0x92][2] = opcodes[0x92][4] = staIndirect8;
	opcodes[0x92][1] = opcodes[0x92][3] = staIndirect16;
	opcodes[0x81][0] = opcodes[0x81][2] = opcodes[0x81][4] = staIndirectx8;
	opcodes[0x81][1] = opcodes[0x81][3] = staIndirectx16;
	opcodes[0x91][0] = opcodes[0x91][2] = opcodes[0x91][4] = staIndirecty8;
	opcodes[0x91][1] = opcodes[0x91][3] = staIndirecty16;
	opcodes[0x87][0] = opcodes[0x87][2] = opcodes[0x87][4] = staIndirectLong8;
	opcodes[0x87][1] = opcodes[0x87][3] = staIndirectLong16;
	opcodes[0x97][0] = opcodes[0x97][2] = opcodes[0x97][4] = staIndirectLongy8;
	opcodes[0x97][1] = opcodes[0x97][3] = staIndirectLongy16;
	opcodes[0x83][0] = opcodes[0x83][2] = opcodes[0x83][4] = staSp8;
	opcodes[0x83][1] = opcodes[0x83][3] = staSp16;
	opcodes[0x93][0] = opcodes[0x93][2] = opcodes[0x93][4] = staSIndirecty8;
	opcodes[0x93][1] = opcodes[0x93][3] = staSIndirecty16;
	/* STX group */
	opcodes[0x86][0] = opcodes[0x86][1] = opcodes[0x86][4] = stxZp8;
	opcodes[0x86][2] = opcodes[0x86][3] = stxZp16;
	opcodes[0x96][0] = opcodes[0x96][1] = opcodes[0x96][4] = stxZpy8;
	opcodes[0x96][2] = opcodes[0x96][3] = stxZpy16;
	opcodes[0x8E][0] = opcodes[0x8E][1] = opcodes[0x8E][4] = stxAbs8;
	opcodes[0x8E][2] = opcodes[0x8E][3] = stxAbs16;
	/* STY group */
	opcodes[0x84][0] = opcodes[0x84][1] = opcodes[0x84][4] = styZp8;
	opcodes[0x84][2] = opcodes[0x84][3] = styZp16;
	opcodes[0x94][0] = opcodes[0x94][1] = opcodes[0x94][4] = styZpx8;
	opcodes[0x94][2] = opcodes[0x94][3] = styZpx16;
	opcodes[0x8C][0] = opcodes[0x8C][1] = opcodes[0x8C][4] = styAbs8;
	opcodes[0x8C][2] = opcodes[0x8C][3] = styAbs16;
	/* STZ group */
	opcodes[0x64][0] = opcodes[0x64][2] = opcodes[0x64][4] = stzZp8;
	opcodes[0x64][1] = opcodes[0x64][3] = stzZp16;
	opcodes[0x74][0] = opcodes[0x74][2] = opcodes[0x74][4] = stzZpx8;
	opcodes[0x74][1] = opcodes[0x74][3] = stzZpx16;
	opcodes[0x9C][0] = opcodes[0x9C][2] = opcodes[0x9C][4] = stzAbs8;
	opcodes[0x9C][1] = opcodes[0x9C][3] = stzAbs16;
	opcodes[0x9E][0] = opcodes[0x9E][2] = opcodes[0x9E][4] = stzAbsx8;
	opcodes[0x9E][1] = opcodes[0x9E][3] = stzAbsx16;

	opcodes[0x3A][0] = opcodes[0x3A][2] = opcodes[0x3A][4] = deca8;
	opcodes[0x3A][1] = opcodes[0x3A][3] = deca16;
	opcodes[0xCA][0] = opcodes[0xCA][1] = opcodes[0xCA][4] = dex8;
	opcodes[0xCA][2] = opcodes[0xCA][3] = dex16;
	opcodes[0x88][0] = opcodes[0x88][1] = opcodes[0x88][4] = dey8;
	opcodes[0x88][2] = opcodes[0x88][3] = dey16;
	opcodes[0x1A][0] = opcodes[0x1A][2] = opcodes[0x1A][4] = inca8;
	opcodes[0x1A][1] = opcodes[0x1A][3] = inca16;
	opcodes[0xE8][0] = opcodes[0xE8][1] = opcodes[0xE8][4] = inx8;
	opcodes[0xE8][2] = opcodes[0xE8][3] = inx16;
	opcodes[0xC8][0] = opcodes[0xC8][1] = opcodes[0xC8][4] = iny8;
	opcodes[0xC8][2] = opcodes[0xC8][3] = iny16;

	/* INC group */
	opcodes[0xE6][0] = opcodes[0xE6][2] = opcodes[0xE6][4] = incZp8;
	opcodes[0xE6][1] = opcodes[0xE6][3] = incZp16;
	opcodes[0xF6][0] = opcodes[0xF6][2] = opcodes[0xF6][4] = incZpx8;
	opcodes[0xF6][1] = opcodes[0xF6][3] = incZpx16;
	opcodes[0xEE][0] = opcodes[0xEE][2] = opcodes[0xEE][4] = incAbs8;
	opcodes[0xEE][1] = opcodes[0xEE][3] = incAbs16;
	opcodes[0xFE][0] = opcodes[0xFE][2] = opcodes[0xFE][4] = incAbsx8;
	opcodes[0xFE][1] = opcodes[0xFE][3] = incAbsx16;

	/* DEC group */
	opcodes[0xC6][0] = opcodes[0xC6][2] = opcodes[0xC6][4] = decZp8;
	opcodes[0xC6][1] = opcodes[0xC6][3] = decZp16;
	opcodes[0xD6][0] = opcodes[0xD6][2] = opcodes[0xD6][4] = decZpx8;
	opcodes[0xD6][1] = opcodes[0xD6][3] = decZpx16;
	opcodes[0xCE][0] = opcodes[0xCE][2] = opcodes[0xCE][4] = decAbs8;
	opcodes[0xCE][1] = opcodes[0xCE][3] = decAbs16;
	opcodes[0xDE][0] = opcodes[0xDE][2] = opcodes[0xDE][4] = decAbsx8;
	opcodes[0xDE][1] = opcodes[0xDE][3] = decAbsx16;

	/* AND group */
	opcodes[0x29][0] = opcodes[0x29][2] = opcodes[0x29][4] = andImm8;
	opcodes[0x29][1] = opcodes[0x29][3] = andImm16;
	opcodes[0x25][0] = opcodes[0x25][2] = opcodes[0x25][4] = andZp8;
	opcodes[0x25][1] = opcodes[0x25][3] = andZp16;
	opcodes[0x35][0] = opcodes[0x35][2] = opcodes[0x35][4] = andZpx8;
	opcodes[0x35][1] = opcodes[0x35][3] = andZpx16;
	opcodes[0x23][0] = opcodes[0x23][2] = opcodes[0x23][4] = andSp8;
	opcodes[0x23][1] = opcodes[0x23][3] = andSp16;
	opcodes[0x2D][0] = opcodes[0x2D][2] = opcodes[0x2D][4] = andAbs8;
	opcodes[0x2D][1] = opcodes[0x2D][3] = andAbs16;
	opcodes[0x3D][0] = opcodes[0x3D][2] = opcodes[0x3D][4] = andAbsx8;
	opcodes[0x3D][1] = opcodes[0x3D][3] = andAbsx16;
	opcodes[0x39][0] = opcodes[0x39][2] = opcodes[0x39][4] = andAbsy8;
	opcodes[0x39][1] = opcodes[0x39][3] = andAbsy16;
	opcodes[0x2F][0] = opcodes[0x2F][2] = opcodes[0x2F][4] = andLong8;
	opcodes[0x2F][1] = opcodes[0x2F][3] = andLong16;
	opcodes[0x3F][0] = opcodes[0x3F][2] = opcodes[0x3F][4] = andLongx8;
	opcodes[0x3F][1] = opcodes[0x3F][3] = andLongx16;
	opcodes[0x32][0] = opcodes[0x32][2] = opcodes[0x32][4] = andIndirect8;
	opcodes[0x32][1] = opcodes[0x32][3] = andIndirect16;
	opcodes[0x21][0] = opcodes[0x21][2] = opcodes[0x21][4] = andIndirectx8;
	opcodes[0x21][1] = opcodes[0x21][3] = andIndirectx16;
	opcodes[0x31][0] = opcodes[0x31][2] = opcodes[0x31][4] = andIndirecty8;
	opcodes[0x31][1] = opcodes[0x31][3] = andIndirecty16;
	opcodes[0x27][0] = opcodes[0x27][2] = opcodes[0x27][4] = andIndirectLong8;
	opcodes[0x27][1] = opcodes[0x27][3] = andIndirectLong16;
	opcodes[0x37][0] = opcodes[0x37][2] = opcodes[0x37][4] = andIndirectLongy8;
	opcodes[0x37][1] = opcodes[0x37][3] = andIndirectLongy16;

	/* EOR group */
	opcodes[0x49][0] = opcodes[0x49][2] = opcodes[0x49][4] = eorImm8;
	opcodes[0x49][1] = opcodes[0x49][3] = eorImm16;
	opcodes[0x45][0] = opcodes[0x45][2] = opcodes[0x45][4] = eorZp8;
	opcodes[0x45][1] = opcodes[0x45][3] = eorZp16;
	opcodes[0x55][0] = opcodes[0x55][2] = opcodes[0x55][4] = eorZpx8;
	opcodes[0x55][1] = opcodes[0x55][3] = eorZpx16;
	opcodes[0x43][0] = opcodes[0x43][2] = opcodes[0x43][4] = eorSp8;
	opcodes[0x43][1] = opcodes[0x43][3] = eorSp16;
	opcodes[0x4D][0] = opcodes[0x4D][2] = opcodes[0x4D][4] = eorAbs8;
	opcodes[0x4D][1] = opcodes[0x4D][3] = eorAbs16;
	opcodes[0x5D][0] = opcodes[0x5D][2] = opcodes[0x5D][4] = eorAbsx8;
	opcodes[0x5D][1] = opcodes[0x5D][3] = eorAbsx16;
	opcodes[0x59][0] = opcodes[0x59][2] = opcodes[0x59][4] = eorAbsy8;
	opcodes[0x59][1] = opcodes[0x59][3] = eorAbsy16;
	opcodes[0x4F][0] = opcodes[0x4F][2] = opcodes[0x4F][4] = eorLong8;
	opcodes[0x4F][1] = opcodes[0x4F][3] = eorLong16;
	opcodes[0x5F][0] = opcodes[0x5F][2] = opcodes[0x5F][4] = eorLongx8;
	opcodes[0x5F][1] = opcodes[0x5F][3] = eorLongx16;
	opcodes[0x52][0] = opcodes[0x52][2] = opcodes[0x52][4] = eorIndirect8;
	opcodes[0x52][1] = opcodes[0x52][3] = eorIndirect16;
	opcodes[0x41][0] = opcodes[0x41][2] = opcodes[0x41][4] = eorIndirectx8;
	opcodes[0x41][1] = opcodes[0x41][3] = eorIndirectx16;
	opcodes[0x51][0] = opcodes[0x51][2] = opcodes[0x51][4] = eorIndirecty8;
	opcodes[0x51][1] = opcodes[0x51][3] = eorIndirecty16;
	opcodes[0x47][0] = opcodes[0x47][2] = opcodes[0x47][4] = eorIndirectLong8;
	opcodes[0x47][1] = opcodes[0x47][3] = eorIndirectLong16;
	opcodes[0x57][0] = opcodes[0x57][2] = opcodes[0x57][4] = eorIndirectLongy8;
	opcodes[0x57][1] = opcodes[0x57][3] = eorIndirectLongy16;

	/* ORA group */
	opcodes[0x09][0] = opcodes[0x09][2] = opcodes[0x09][4] = oraImm8;
	opcodes[0x09][1] = opcodes[0x09][3] = oraImm16;
	opcodes[0x05][0] = opcodes[0x05][2] = opcodes[0x05][4] = oraZp8;
	opcodes[0x05][1] = opcodes[0x05][3] = oraZp16;
	opcodes[0x15][0] = opcodes[0x15][2] = opcodes[0x15][4] = oraZpx8;
	opcodes[0x15][1] = opcodes[0x15][3] = oraZpx16;
	opcodes[0x03][0] = opcodes[0x03][2] = opcodes[0x03][4] = oraSp8;
	opcodes[0x03][1] = opcodes[0x03][3] = oraSp16;
	opcodes[0x0D][0] = opcodes[0x0D][2] = opcodes[0x0D][4] = oraAbs8;
	opcodes[0x0D][1] = opcodes[0x0D][3] = oraAbs16;
	opcodes[0x1D][0] = opcodes[0x1D][2] = opcodes[0x1D][4] = oraAbsx8;
	opcodes[0x1D][1] = opcodes[0x1D][3] = oraAbsx16;
	opcodes[0x19][0] = opcodes[0x19][2] = opcodes[0x19][4] = oraAbsy8;
	opcodes[0x19][1] = opcodes[0x19][3] = oraAbsy16;
	opcodes[0x0F][0] = opcodes[0x0F][2] = opcodes[0x0F][4] = oraLong8;
	opcodes[0x0F][1] = opcodes[0x0F][3] = oraLong16;
	opcodes[0x1F][0] = opcodes[0x1F][2] = opcodes[0x1F][4] = oraLongx8;
	opcodes[0x1F][1] = opcodes[0x1F][3] = oraLongx16;
	opcodes[0x12][0] = opcodes[0x12][2] = opcodes[0x12][4] = oraIndirect8;
	opcodes[0x12][1] = opcodes[0x12][3] = oraIndirect16;
	opcodes[0x01][0] = opcodes[0x01][2] = opcodes[0x01][4] = oraIndirectx8;
	opcodes[0x01][1] = opcodes[0x01][3] = oraIndirectx16;
	opcodes[0x11][0] = opcodes[0x11][2] = opcodes[0x11][4] = oraIndirecty8;
	opcodes[0x11][1] = opcodes[0x11][3] = oraIndirecty16;
	opcodes[0x07][0] = opcodes[0x07][2] = opcodes[0x07][4] = oraIndirectLong8;
	opcodes[0x07][1] = opcodes[0x07][3] = oraIndirectLong16;
	opcodes[0x17][0] = opcodes[0x17][2] = opcodes[0x17][4] = oraIndirectLongy8;
	opcodes[0x17][1] = opcodes[0x17][3] = oraIndirectLongy16;

	/* ADC group */
	opcodes[0x69][0] = opcodes[0x69][2] = opcodes[0x69][4] = adcImm8;
	opcodes[0x69][1] = opcodes[0x69][3] = adcImm16;
	opcodes[0x65][0] = opcodes[0x65][2] = opcodes[0x65][4] = adcZp8;
	opcodes[0x65][1] = opcodes[0x65][3] = adcZp16;
	opcodes[0x75][0] = opcodes[0x75][2] = opcodes[0x75][4] = adcZpx8;
	opcodes[0x75][1] = opcodes[0x75][3] = adcZpx16;
	opcodes[0x63][0] = opcodes[0x63][2] = opcodes[0x63][4] = adcSp8;
	opcodes[0x63][1] = opcodes[0x63][3] = adcSp16;
	opcodes[0x6D][0] = opcodes[0x6D][2] = opcodes[0x6D][4] = adcAbs8;
	opcodes[0x6D][1] = opcodes[0x6D][3] = adcAbs16;
	opcodes[0x7D][0] = opcodes[0x7D][2] = opcodes[0x7D][4] = adcAbsx8;
	opcodes[0x7D][1] = opcodes[0x7D][3] = adcAbsx16;
	opcodes[0x79][0] = opcodes[0x79][2] = opcodes[0x79][4] = adcAbsy8;
	opcodes[0x79][1] = opcodes[0x79][3] = adcAbsy16;
	opcodes[0x6F][0] = opcodes[0x6F][2] = opcodes[0x6F][4] = adcLong8;
	opcodes[0x6F][1] = opcodes[0x6F][3] = adcLong16;
	opcodes[0x7F][0] = opcodes[0x7F][2] = opcodes[0x7F][4] = adcLongx8;
	opcodes[0x7F][1] = opcodes[0x7F][3] = adcLongx16;
	opcodes[0x72][0] = opcodes[0x72][2] = opcodes[0x72][4] = adcIndirect8;
	opcodes[0x72][1] = opcodes[0x72][3] = adcIndirect16;
	opcodes[0x61][0] = opcodes[0x61][2] = opcodes[0x61][4] = adcIndirectx8;
	opcodes[0x61][1] = opcodes[0x61][3] = adcIndirectx16;
	opcodes[0x71][0] = opcodes[0x71][2] = opcodes[0x71][4] = adcIndirecty8;
	opcodes[0x71][1] = opcodes[0x71][3] = adcIndirecty16;
	opcodes[0x73][0] = opcodes[0x73][2] = opcodes[0x73][4] = adcsIndirecty8;
	opcodes[0x73][1] = opcodes[0x73][3] = adcsIndirecty16;
	opcodes[0x67][0] = opcodes[0x67][2] = opcodes[0x67][4] = adcIndirectLong8;
	opcodes[0x67][1] = opcodes[0x67][3] = adcIndirectLong16;
	opcodes[0x77][0] = opcodes[0x77][2] = opcodes[0x77][4] = adcIndirectLongy8;
	opcodes[0x77][1] = opcodes[0x77][3] = adcIndirectLongy16;

	/* SBC group */
	opcodes[0xE9][0] = opcodes[0xE9][2] = opcodes[0xE9][4] = sbcImm8;
	opcodes[0xE9][1] = opcodes[0xE9][3] = sbcImm16;
	opcodes[0xE5][0] = opcodes[0xE5][2] = opcodes[0xE5][4] = sbcZp8;
	opcodes[0xE5][1] = opcodes[0xE5][3] = sbcZp16;
	opcodes[0xE3][0] = opcodes[0xE3][2] = opcodes[0xE3][4] = sbcSp8;
	opcodes[0xE3][1] = opcodes[0xE3][3] = sbcSp16;
	opcodes[0xF5][0] = opcodes[0xF5][2] = opcodes[0xF5][4] = sbcZpx8;
	opcodes[0xF5][1] = opcodes[0xF5][3] = sbcZpx16;
	opcodes[0xED][0] = opcodes[0xED][2] = opcodes[0xED][4] = sbcAbs8;
	opcodes[0xED][1] = opcodes[0xED][3] = sbcAbs16;
	opcodes[0xFD][0] = opcodes[0xFD][2] = opcodes[0xFD][4] = sbcAbsx8;
	opcodes[0xFD][1] = opcodes[0xFD][3] = sbcAbsx16;
	opcodes[0xF9][0] = opcodes[0xF9][2] = opcodes[0xF9][4] = sbcAbsy8;
	opcodes[0xF9][1] = opcodes[0xF9][3] = sbcAbsy16;
	opcodes[0xEF][0] = opcodes[0xEF][2] = opcodes[0xEF][4] = sbcLong8;
	opcodes[0xEF][1] = opcodes[0xEF][3] = sbcLong16;
	opcodes[0xFF][0] = opcodes[0xFF][2] = opcodes[0xFF][4] = sbcLongx8;
	opcodes[0xFF][1] = opcodes[0xFF][3] = sbcLongx16;
	opcodes[0xF2][0] = opcodes[0xF2][2] = opcodes[0xF2][4] = sbcIndirect8;
	opcodes[0xF2][1] = opcodes[0xF2][3] = sbcIndirect16;
	opcodes[0xE1][0] = opcodes[0xE1][2] = opcodes[0xE1][4] = sbcIndirectx8;
	opcodes[0xE1][1] = opcodes[0xE1][3] = sbcIndirectx16;
	opcodes[0xF1][0] = opcodes[0xF1][2] = opcodes[0xF1][4] = sbcIndirecty8;
	opcodes[0xF1][1] = opcodes[0xF1][3] = sbcIndirecty16;
	opcodes[0xE7][0] = opcodes[0xE7][2] = opcodes[0xE7][4] = sbcIndirectLong8;
	opcodes[0xE7][1] = opcodes[0xE7][3] = sbcIndirectLong16;
	opcodes[0xF7][0] = opcodes[0xF7][2] = opcodes[0xF7][4] = sbcIndirectLongy8;
	opcodes[0xF7][1] = opcodes[0xF7][3] = sbcIndirectLongy16;

	/* Transfer group */
	opcodes[0xAA][0] = opcodes[0xAA][1] = opcodes[0xAA][4] = tax8;
	opcodes[0xAA][2] = opcodes[0xAA][3] = tax16;
	opcodes[0xA8][0] = opcodes[0xA8][1] = opcodes[0xA8][4] = tay8;
	opcodes[0xA8][2] = opcodes[0xA8][3] = tay16;
	opcodes[0x8A][0] = opcodes[0x8A][2] = opcodes[0x8A][4] = txa8;
	opcodes[0x8A][1] = opcodes[0x8A][3] = txa16;
	opcodes[0x98][0] = opcodes[0x98][2] = opcodes[0x98][4] = tya8;
	opcodes[0x98][1] = opcodes[0x98][3] = tya16;
	opcodes[0x9B][0] = opcodes[0x9B][1] = opcodes[0x9B][4] = txy8;
	opcodes[0x9B][2] = opcodes[0x9B][3] = txy16;
	opcodes[0xBB][0] = opcodes[0xBB][1] = opcodes[0xBB][4] = tyx8;
	opcodes[0xBB][2] = opcodes[0xBB][3] = tyx16;
	opcodes[0xBA][0] = opcodes[0xBA][1] = opcodes[0xBA][4] = tsx8;
	opcodes[0xBA][2] = opcodes[0xBA][3] = tsx16;
	opcodes[0x9A][0] = opcodes[0x9A][1] = opcodes[0x9A][4] = txs8;
	opcodes[0x9A][2] = opcodes[0x9A][3] = txs16;

	/* Flag Group */
	opcodes[0x18][0] = opcodes[0x18][1] = opcodes[0x18][2] = opcodes[0x18][3] =
		opcodes[0x18][4] = clc;
	opcodes[0xD8][0] = opcodes[0xD8][1] = opcodes[0xD8][2] = opcodes[0xD8][3] =
		opcodes[0xD8][4] = cld;
	opcodes[0x58][0] = opcodes[0x58][1] = opcodes[0x58][2] = opcodes[0x58][3] =
		opcodes[0x58][4] = cli;
	opcodes[0xB8][0] = opcodes[0xB8][1] = opcodes[0xB8][2] = opcodes[0xB8][3] =
		opcodes[0xB8][4] = clv;
	opcodes[0x38][0] = opcodes[0x38][1] = opcodes[0x38][2] = opcodes[0x38][3] =
		opcodes[0x38][4] = sec;
	opcodes[0xF8][0] = opcodes[0xF8][1] = opcodes[0xF8][2] = opcodes[0xF8][3] =
		opcodes[0xF8][4] = sed;
	opcodes[0x78][0] = opcodes[0x78][1] = opcodes[0x78][2] = opcodes[0x78][3] =
		opcodes[0x78][4] = sei;
	opcodes[0xFB][0] = opcodes[0xFB][1] = opcodes[0xFB][2] = opcodes[0xFB][3] =
		opcodes[0xFB][4] = xce;
	opcodes[0xE2][0] = opcodes[0xE2][1] = opcodes[0xE2][2] = opcodes[0xE2][3] =
		opcodes[0xE2][4] = sep;
	opcodes[0xC2][0] = opcodes[0xC2][1] = opcodes[0xC2][2] = opcodes[0xC2][3] =
		opcodes[0xC2][4] = rep;

	/* Stack group */
	opcodes[0x8B][0] = opcodes[0x8B][1] = opcodes[0x8B][2] = opcodes[0x8B][3] =
		phb;
	opcodes[0x8B][4] = phbe;
	opcodes[0x4B][0] = opcodes[0x4B][1] = opcodes[0x4B][2] = opcodes[0x4B][3] =
		phk;
	opcodes[0x4B][4] = phke;
	opcodes[0xAB][0] = opcodes[0xAB][1] = opcodes[0xAB][2] = opcodes[0xAB][3] =
		plb;
	opcodes[0xAB][4] = plbe;
	opcodes[0x08][0] = opcodes[0x08][1] = opcodes[0x08][2] = opcodes[0x08][3] =
		php;
	opcodes[0x08][4] = php;
	opcodes[0x28][0] = opcodes[0x28][1] = opcodes[0x28][2] = opcodes[0x28][3] =
		plp;
	opcodes[0x28][4] = plp;
	opcodes[0x48][0] = opcodes[0x48][2] = opcodes[0x48][4] = pha8;
	opcodes[0x48][1] = opcodes[0x48][3] = pha16;
	opcodes[0xDA][0] = opcodes[0xDA][1] = opcodes[0xDA][4] = phx8;
	opcodes[0xDA][2] = opcodes[0xDA][3] = phx16;
	opcodes[0x5A][0] = opcodes[0x5A][1] = opcodes[0x5A][4] = phy8;
	opcodes[0x5A][2] = opcodes[0x5A][3] = phy16;
	opcodes[0x68][0] = opcodes[0x68][2] = opcodes[0x68][4] = pla8;
	opcodes[0x68][1] = opcodes[0x68][3] = pla16;
	opcodes[0xFA][0] = opcodes[0xFA][1] = opcodes[0xFA][4] = plx8;
	opcodes[0xFA][2] = opcodes[0xFA][3] = plx16;
	opcodes[0x7A][0] = opcodes[0x7A][1] = opcodes[0x7A][4] = ply8;
	opcodes[0x7A][2] = opcodes[0x7A][3] = ply16;
	opcodes[0xD4][0] = opcodes[0xD4][1] = opcodes[0xD4][2] = opcodes[0xD4][3] =
		opcodes[0xD4][4] = pei;
	opcodes[0xF4][0] = opcodes[0xF4][1] = opcodes[0xF4][2] = opcodes[0xF4][3] =
		opcodes[0xF4][4] = pea;
	opcodes[0x62][0] = opcodes[0x62][1] = opcodes[0x62][2] = opcodes[0x62][3] =
		opcodes[0x62][4] = per;
	opcodes[0x0B][0] = opcodes[0x0B][1] = opcodes[0x0B][2] = opcodes[0x0B][3] =
		opcodes[0x0B][4] = phd;
	opcodes[0x2B][0] = opcodes[0x2B][1] = opcodes[0x2B][2] = opcodes[0x2B][3] =
		opcodes[0x2B][4] = pld;

	/* CMP group */
	opcodes[0xC9][0] = opcodes[0xC9][2] = opcodes[0xC9][4] = cmpImm8;
	opcodes[0xC9][1] = opcodes[0xC9][3] = cmpImm16;
	opcodes[0xC5][0] = opcodes[0xC5][2] = opcodes[0xC5][4] = cmpZp8;
	opcodes[0xC5][1] = opcodes[0xC5][3] = cmpZp16;
	opcodes[0xC3][0] = opcodes[0xC3][2] = opcodes[0xC3][4] = cmpSp8;
	opcodes[0xC3][1] = opcodes[0xC3][3] = cmpSp16;
	opcodes[0xD5][0] = opcodes[0xD5][2] = opcodes[0xD5][4] = cmpZpx8;
	opcodes[0xD5][1] = opcodes[0xD5][3] = cmpZpx16;
	opcodes[0xCD][0] = opcodes[0xCD][2] = opcodes[0xCD][4] = cmpAbs8;
	opcodes[0xCD][1] = opcodes[0xCD][3] = cmpAbs16;
	opcodes[0xDD][0] = opcodes[0xDD][2] = opcodes[0xDD][4] = cmpAbsx8;
	opcodes[0xDD][1] = opcodes[0xDD][3] = cmpAbsx16;
	opcodes[0xD9][0] = opcodes[0xD9][2] = opcodes[0xD9][4] = cmpAbsy8;
	opcodes[0xD9][1] = opcodes[0xD9][3] = cmpAbsy16;
	opcodes[0xCF][0] = opcodes[0xCF][2] = opcodes[0xCF][4] = cmpLong8;
	opcodes[0xCF][1] = opcodes[0xCF][3] = cmpLong16;
	opcodes[0xDF][0] = opcodes[0xDF][2] = opcodes[0xDF][4] = cmpLongx8;
	opcodes[0xDF][1] = opcodes[0xDF][3] = cmpLongx16;
	opcodes[0xD2][0] = opcodes[0xD2][2] = opcodes[0xD2][4] = cmpIndirect8;
	opcodes[0xD2][1] = opcodes[0xD2][3] = cmpIndirect16;
	opcodes[0xC1][0] = opcodes[0xC1][2] = opcodes[0xC1][4] = cmpIndirectx8;
	opcodes[0xC1][1] = opcodes[0xC1][3] = cmpIndirectx16;
	opcodes[0xD1][0] = opcodes[0xD1][2] = opcodes[0xD1][4] = cmpIndirecty8;
	opcodes[0xD1][1] = opcodes[0xD1][3] = cmpIndirecty16;
	opcodes[0xC7][0] = opcodes[0xC7][2] = opcodes[0xC7][4] = cmpIndirectLong8;
	opcodes[0xC7][1] = opcodes[0xC7][3] = cmpIndirectLong16;
	opcodes[0xD7][0] = opcodes[0xD7][2] = opcodes[0xD7][4] = cmpIndirectLongy8;
	opcodes[0xD7][1] = opcodes[0xD7][3] = cmpIndirectLongy16;

	/* CPX group */
	opcodes[0xE0][0] = opcodes[0xE0][1] = opcodes[0xE0][4] = cpxImm8;
	opcodes[0xE0][2] = opcodes[0xE0][3] = cpxImm16;
	opcodes[0xE4][0] = opcodes[0xE4][1] = opcodes[0xE4][4] = cpxZp8;
	opcodes[0xE4][2] = opcodes[0xE4][3] = cpxZp16;
	opcodes[0xEC][0] = opcodes[0xEC][1] = opcodes[0xEC][4] = cpxAbs8;
	opcodes[0xEC][2] = opcodes[0xEC][3] = cpxAbs16;

	/* CPY group */
	opcodes[0xC0][0] = opcodes[0xC0][1] = opcodes[0xC0][4] = cpyImm8;
	opcodes[0xC0][2] = opcodes[0xC0][3] = cpyImm16;
	opcodes[0xC4][0] = opcodes[0xC4][1] = opcodes[0xC4][4] = cpyZp8;
	opcodes[0xC4][2] = opcodes[0xC4][3] = cpyZp16;
	opcodes[0xCC][0] = opcodes[0xCC][1] = opcodes[0xCC][4] = cpyAbs8;
	opcodes[0xCC][2] = opcodes[0xCC][3] = cpyAbs16;

	/* Branch group */
	opcodes[0x90][0] = opcodes[0x90][1] = opcodes[0x90][2] = opcodes[0x90][3] =
		opcodes[0x90][4] = bcc;
	opcodes[0xB0][0] = opcodes[0xB0][1] = opcodes[0xB0][2] = opcodes[0xB0][3] =
		opcodes[0xB0][4] = bcs;
	opcodes[0xF0][0] = opcodes[0xF0][1] = opcodes[0xF0][2] = opcodes[0xF0][3] =
		opcodes[0xF0][4] = beq;
	opcodes[0xD0][0] = opcodes[0xD0][1] = opcodes[0xD0][2] = opcodes[0xD0][3] =
		opcodes[0xD0][4] = bne;
	opcodes[0x80][0] = opcodes[0x80][1] = opcodes[0x80][2] = opcodes[0x80][3] =
		opcodes[0x80][4] = bra;
	opcodes[0x82][0] = opcodes[0x82][1] = opcodes[0x82][2] = opcodes[0x82][3] =
		opcodes[0x82][4] = brl;
	opcodes[0x10][0] = opcodes[0x10][1] = opcodes[0x10][2] = opcodes[0x10][3] =
		opcodes[0x10][4] = bpl;
	opcodes[0x30][0] = opcodes[0x30][1] = opcodes[0x30][2] = opcodes[0x30][3] =
		opcodes[0x30][4] = bmi;
	opcodes[0x50][0] = opcodes[0x50][1] = opcodes[0x50][2] = opcodes[0x50][3] =
		opcodes[0x50][4] = bvc;
	opcodes[0x70][0] = opcodes[0x70][1] = opcodes[0x70][2] = opcodes[0x70][3] =
		opcodes[0x70][4] = bvs;

	/* Jump group */
	opcodes[0x4C][0] = opcodes[0x4C][1] = opcodes[0x4C][2] = opcodes[0x4C][3] =
		opcodes[0x4C][4] = jmp;
	opcodes[0x5C][0] = opcodes[0x5C][1] = opcodes[0x5C][2] = opcodes[0x5C][3] =
		opcodes[0x5C][4] = jmplong;
	opcodes[0x6C][0] = opcodes[0x6C][1] = opcodes[0x6C][2] = opcodes[0x6C][3] =
		opcodes[0x6C][4] = jmpind;
	opcodes[0x7C][0] = opcodes[0x7C][1] = opcodes[0x7C][2] = opcodes[0x7C][3] =
		opcodes[0x7C][4] = jmpindx;
	opcodes[0xDC][0] = opcodes[0xDC][1] = opcodes[0xDC][2] = opcodes[0xDC][3] =
		opcodes[0xDC][4] = jmlind;
	opcodes[0x20][0] = opcodes[0x20][1] = opcodes[0x20][2] = opcodes[0x20][3] =
		jsr;
	opcodes[0x20][4] = jsre;
	opcodes[0xFC][0] = opcodes[0xFC][1] = opcodes[0xFC][2] = opcodes[0xFC][3] =
		jsrIndx;
	opcodes[0xFC][4] = jsrIndxe;
	opcodes[0x60][0] = opcodes[0x60][1] = opcodes[0x60][2] = opcodes[0x60][3] =
		rts;
	opcodes[0x60][4] = rtse;
	opcodes[0x6B][0] = opcodes[0x6B][1] = opcodes[0x6B][2] = opcodes[0x6B][3] =
		rtl;
	opcodes[0x6B][4] = rtle;
	opcodes[0x40][0] = opcodes[0x40][1] = opcodes[0x40][2] = opcodes[0x40][3] =
		rti;
	opcodes[0x22][0] = opcodes[0x22][1] = opcodes[0x22][2] = opcodes[0x22][3] =
		jsl;
	opcodes[0x22][4] = jsle;

	/* Shift group */
	opcodes[0x0A][0] = opcodes[0x0A][2] = opcodes[0x0A][4] = asla8;
	opcodes[0x0A][1] = opcodes[0x0A][3] = asla16;
	opcodes[0x06][0] = opcodes[0x06][2] = opcodes[0x06][4] = aslZp8;
	opcodes[0x06][1] = opcodes[0x06][3] = aslZp16;
	opcodes[0x16][0] = opcodes[0x16][2] = opcodes[0x16][4] = aslZpx8;
	opcodes[0x16][1] = opcodes[0x16][3] = aslZpx16;
	opcodes[0x0E][0] = opcodes[0x0E][2] = opcodes[0x0E][4] = aslAbs8;
	opcodes[0x0E][1] = opcodes[0x0E][3] = aslAbs16;
	opcodes[0x1E][0] = opcodes[0x1E][2] = opcodes[0x1E][4] = aslAbsx8;
	opcodes[0x1E][1] = opcodes[0x1E][3] = aslAbsx16;

	opcodes[0x4A][0] = opcodes[0x4A][2] = opcodes[0x4A][4] = lsra8;
	opcodes[0x4A][1] = opcodes[0x4A][3] = lsra16;
	opcodes[0x46][0] = opcodes[0x46][2] = opcodes[0x46][4] = lsrZp8;
	opcodes[0x46][1] = opcodes[0x46][3] = lsrZp16;
	opcodes[0x56][0] = opcodes[0x56][2] = opcodes[0x56][4] = lsrZpx8;
	opcodes[0x56][1] = opcodes[0x56][3] = lsrZpx16;
	opcodes[0x4E][0] = opcodes[0x4E][2] = opcodes[0x4E][4] = lsrAbs8;
	opcodes[0x4E][1] = opcodes[0x4E][3] = lsrAbs16;
	opcodes[0x5E][0] = opcodes[0x5E][2] = opcodes[0x5E][4] = lsrAbsx8;
	opcodes[0x5E][1] = opcodes[0x5E][3] = lsrAbsx16;

	opcodes[0x2A][0] = opcodes[0x2A][2] = opcodes[0x2A][4] = rola8;
	opcodes[0x2A][1] = opcodes[0x2A][3] = rola16;
	opcodes[0x26][0] = opcodes[0x26][2] = opcodes[0x26][4] = rolZp8;
	opcodes[0x26][1] = opcodes[0x26][3] = rolZp16;
	opcodes[0x36][0] = opcodes[0x36][2] = opcodes[0x36][4] = rolZpx8;
	opcodes[0x36][1] = opcodes[0x36][3] = rolZpx16;
	opcodes[0x2E][0] = opcodes[0x2E][2] = opcodes[0x2E][4] = rolAbs8;
	opcodes[0x2E][1] = opcodes[0x2E][3] = rolAbs16;
	opcodes[0x3E][0] = opcodes[0x3E][2] = opcodes[0x3E][4] = rolAbsx8;
	opcodes[0x3E][1] = opcodes[0x3E][3] = rolAbsx16;

	opcodes[0x6A][0] = opcodes[0x6A][2] = opcodes[0x6A][4] = rora8;
	opcodes[0x6A][1] = opcodes[0x6A][3] = rora16;
	opcodes[0x66][0] = opcodes[0x66][2] = opcodes[0x66][4] = rorZp8;
	opcodes[0x66][1] = opcodes[0x66][3] = rorZp16;
	opcodes[0x76][0] = opcodes[0x76][2] = opcodes[0x76][4] = rorZpx8;
	opcodes[0x76][1] = opcodes[0x76][3] = rorZpx16;
	opcodes[0x6E][0] = opcodes[0x6E][2] = opcodes[0x6E][4] = rorAbs8;
	opcodes[0x6E][1] = opcodes[0x6E][3] = rorAbs16;
	opcodes[0x7E][0] = opcodes[0x7E][2] = opcodes[0x7E][4] = rorAbsx8;
	opcodes[0x7E][1] = opcodes[0x7E][3] = rorAbsx16;

	/* BIT group */
	opcodes[0x89][0] = opcodes[0x89][2] = opcodes[0x89][4] = bitImm8;
	opcodes[0x89][1] = opcodes[0x89][3] = bitImm16;
	opcodes[0x24][0] = opcodes[0x24][2] = opcodes[0x24][4] = bitZp8;
	opcodes[0x24][1] = opcodes[0x24][3] = bitZp16;
	opcodes[0x34][0] = opcodes[0x34][2] = opcodes[0x34][4] = bitZpx8;
	opcodes[0x34][1] = opcodes[0x34][3] = bitZpx16;
	opcodes[0x2C][0] = opcodes[0x2C][2] = opcodes[0x2C][4] = bitAbs8;
	opcodes[0x2C][1] = opcodes[0x2C][3] = bitAbs16;
	opcodes[0x3C][0] = opcodes[0x3C][2] = opcodes[0x3C][4] = bitAbsx8;
	opcodes[0x3C][1] = opcodes[0x3C][3] = bitAbsx16;

	/* Misc group */
	opcodes[0x00][0] = opcodes[0x00][1] = opcodes[0x00][2] = opcodes[0x00][3] =
		opcodes[0x00][4] = brk;
	opcodes[0xEB][0] = opcodes[0xEB][1] = opcodes[0xEB][2] = opcodes[0xEB][3] =
		opcodes[0xEB][4] = xba;
	opcodes[0xEA][0] = opcodes[0xEA][1] = opcodes[0xEA][2] = opcodes[0xEA][3] =
		opcodes[0xEA][4] = nop;
	opcodes[0x5B][0] = opcodes[0x5B][1] = opcodes[0x5B][2] = opcodes[0x5B][3] =
		opcodes[0x5B][4] = tcd;
	opcodes[0x7B][0] = opcodes[0x7B][1] = opcodes[0x7B][2] = opcodes[0x7B][3] =
		opcodes[0x7B][4] = tdc;
	opcodes[0x1B][0] = opcodes[0x1B][1] = opcodes[0x1B][2] = opcodes[0x1B][3] =
		opcodes[0x1B][4] = tcs;
	opcodes[0x3B][0] = opcodes[0x3B][1] = opcodes[0x3B][2] = opcodes[0x3B][3] =
		opcodes[0x3B][4] = tsc;
	opcodes[0xCB][0] = opcodes[0xCB][1] = opcodes[0xCB][2] = opcodes[0xCB][3] =
		opcodes[0xCB][4] = wai;
	opcodes[0x44][0] = opcodes[0x44][1] = opcodes[0x44][2] = opcodes[0x44][3] =
		opcodes[0x44][4] = mvp;
	opcodes[0x54][0] = opcodes[0x54][1] = opcodes[0x54][2] = opcodes[0x54][3] =
		opcodes[0x54][4] = mvn;
	opcodes[0x04][0] = opcodes[0x04][2] = opcodes[0x04][4] = tsbZp8;
	opcodes[0x04][1] = opcodes[0x04][3] = tsbZp16;
	opcodes[0x0C][0] = opcodes[0x0C][2] = opcodes[0x0C][4] = tsbAbs8;
	opcodes[0x0C][1] = opcodes[0x0C][3] = tsbAbs16;
	opcodes[0x14][0] = opcodes[0x14][2] = opcodes[0x14][4] = trbZp8;
	opcodes[0x14][1] = opcodes[0x14][3] = trbZp16;
	opcodes[0x1C][0] = opcodes[0x1C][2] = opcodes[0x1C][4] = trbAbs8;
	opcodes[0x1C][1] = opcodes[0x1C][3] = trbAbs16;
}

void updatecpumode()
{
	if (p.e) {
		cpumode = 4;
		x.b.h = y.b.h = 0;
	} else {
		cpumode = 0;
		if (!p.m)
			cpumode |= 1;
		if (!p.x)
			cpumode |= 2;
		if (p.x)
			x.b.h = y.b.h = 0;
	}
}

void nmi65c816()
{
	unsigned char temp = 0;
	// printf("NMI %i %i %i\n",p.i,inwai,irqenable);
	readmem(pbr | pc);
	cycles -= 6;
	clockspc(6);
	if (inwai)
		pc++;
	inwai = 0;
	if (!p.e) {
		// //printf("%02X -> %04X\n",pbr>>16,s.w);
		writemem(s.w, pbr >> 16);
		s.w--;
		// //printf("%02X -> %04X\n",pc>>8,s.w);
		writemem(s.w, pc >> 8);
		s.w--;
		// //printf("%02X -> %04X\n",pc&0xFF,s.w);
		writemem(s.w, pc & 0xFF);
		s.w--;
		if (p.c)
			temp |= 1;
		if (p.z)
			temp |= 2;
		if (p.i)
			temp |= 4;
		if (p.d)
			temp |= 8;
		if (p.x)
			temp |= 0x10;
		if (p.m)
			temp |= 0x20;
		if (p.v)
			temp |= 0x40;
		if (p.n)
			temp |= 0x80;
		// //printf("%02X -> %04X\n",temp,s.w);
		writemem(s.w, temp);
		s.w--;
		pc = readmemw(0xFFEA);
		pbr = 0;
		p.i = 1;
		p.d = 0;
		// printf("NMI\n");
	} else {
		snemlog("Emulation mode NMI\n");
		dumpregs();
		exit(-1);
	}
}

void irq65c816()
{
	unsigned char temp = 0;
	// printf("IRQ %i %i %i\n",p.i,inwai,irqenable);
	readmem(pbr | pc);
	cycles -= 6;
	clockspc(6);
	if (inwai && p.i) {
		pc++;
		inwai = 0;
		return;
	}
	if (inwai)
		pc++;
	inwai = 0;
	if (!p.e) {
		writemem(s.w, pbr >> 16);
		s.w--;
		writemem(s.w, pc >> 8);
		s.w--;
		writemem(s.w, pc & 0xFF);
		s.w--;
		if (p.c)
			temp |= 1;
		if (p.z)
			temp |= 2;
		if (p.i)
			temp |= 4;
		if (p.d)
			temp |= 8;
		if (p.x)
			temp |= 0x10;
		if (p.m)
			temp |= 0x20;
		if (p.v)
			temp |= 0x40;
		if (p.n)
			temp |= 0x80;
		writemem(s.w, temp);
		s.w--;
		pc = readmemw(0xFFEE);
		pbr = 0;
		p.i = 1;
		p.d = 0;
		// printf("IRQ\n");
	} else {
		snemlog("Emulation mode IRQ\n");
		dumpregs();
		exit(-1);
	}
}
