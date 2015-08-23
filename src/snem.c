#include <stdlib.h>
#include <allegro.h>

#include "snem.h"
#include "util.h"

int intthisline;

// TODO a lot of these should probably be 'extern'
uint32_t pc2, pc3, pc4;
uint32_t pc7, pc8, pc9, pc10, pc11, pc12, pc13, pc14, pc15;
int framenum;
int oldnmi = 0;
int output = 1;
int timetolive = 0;
int times = 0;
int ins = 0;

int fps, frames, changefps;
int spcclck, spctotal;
double spcclck2, spcclck3;
int infocus, romloaded;

// Predefinitions of functions that are declared later here
void wakeupmainthread();

void oncesec()
{
	fps = frames;
	frames = 0;
	changefps = 1;
}

int drawcount = 0;
void hz60()
{
	wakeupmainthread();
	if (infocus && romloaded)
		drawcount++;
}

// int e0bc=0;
void execframe()
{
	nmi = vbl = 0;
	framenum++;
	if (framenum == 50) {
		spcclck = spctotal;
		spcclck2 = spctotal2;
		spcclck3 = spctotal3;
		spctotal = 0;
		spctotal2 = 0.0f;
		spctotal3 = 0.0f;
	}
	for (lines = 0; lines < ((pal) ? 312 : 262); lines++) {
		// snemlog("%i %02X:%04X %i %i
		// %i\n",lines,pbr>>16,pc,irqenable,xirq,yirq);
		if ((irqenable == 2 /* || irqenable==1 */) && (lines == yirq)) {
			irq = 1; /* snemlog("Raise IRQ line %i %02X\n",lines,lines); */
		}
		if (lines < 225)
			drawline(lines);
		cycles += 1364;
		intthisline = 0;
		while (cycles > 0) {
			// if (ins==20) snemdebug("1 %06X\n",pbr|pc);
			/*                        pc7=pc8;
									pc8=pc9;
									pc9=pc10;
									pc10=pc11;
									pc11=pc12;
									pc12=pc13;
									pc13=pc14;
									pc14=pc15;
									pc15=pc2;
									pc2=pc3;
									pc3=pc4;
									pc4=pbr|pc;
			// if (pc==0xFB2A) { snemdebug("FB2A
			X=%04X\n",x.w); }
									if (pc==0xFD5F || pc==0xFD94)
									{
											snemdebug("%04X %06X %06X %06X %06X
			%06X %06X %06X %06X %06X %06X %06X
			%06X\n",pc,pc2,pc3,pc4,pc7,pc8,pc9,pc10,pc11,pc12,pc13,pc14,pc15);
											output=1;
											timetolive=20;
									}
									if (pc==0xF58D) snemdebug("F58D A=%04X
			Y=%04X\n",a.w,y.w);
			// if (pc==0xFD66) { timetolive=15; }
			// if (pc==0xFD91) { output=0; }
				if (pc==0xFADB) { output=1; }
				if (pc==0xFAE6) { output=0; } */
			opcode = readmem(pbr | pc);
			pc++;
			opcodes[opcode][cpumode]();
			if ((((irqenable == 3) && (lines == yirq)) || (irqenable == 1)) &&
				!intthisline) {
				if (((1364 - cycles) >> 2) >= xirq) {
					irq = 1;
					intthisline = 1;
					// snemlog("Raise IRQ
					// horiz %i
					// %i\n",lines,irqenable);
				}
			}
			/*                        if (pc==0x9665)
									{
											dumpregs();
											exit(-1);
									} */
			/*                        if (pc==0x9F64) { output=1; }
									if (pc==0x9F67)
									{
											dumpregs();
											snemdebug("Hit E166 %06X %06X
			   %06X\n",pc2,pc3,pc4);
											snemdebug("%08X %08X %02X
			   %02X\n",memlookup[4],memlookup[0x404],readmem(0x9F64),readmem(0x809F64));
											exit(-1);
									} */
			// if ((pbr|pc)==0xC40142) { output=1;
			// timetolive=300; }
			/*                        if (x.w==0xE0BC && !e0bc) { snemdebug("X=E0BC
			at %06X\n",pbr|pc); }
									e0bc=(x.w==0xE0BC); */
			// if (pc==0x93E2) { output=1; }
			// if (pc==0x93F3) { timetolive=150; }
			// if (output) { snemdebug("%06X : %04X %04X
			// %04X\n",pbr|pc,a.w,x.w,y.w); }
			// snemdebug("End of op\n");
			// if (output) { snemdebug("%06X ",pc); }
			// if (pc==0xBBE9) { output=1; }
			// if (pc==0xBC18) { output=0; }
			// if (pc==0xB898) { ins=0; output=1; }
			// if ((pbr|pc)==0x89262) { snemdebug("Hit 89262
			// %04X\n",s.w); }
			ins++;
			// if (ins==16640000) { output=1;
			// timetolive=5000; }
			// if (ins==6050000 && times==1) output=1;
			if (oldnmi != nmi && nmienable && nmi)
				nmi65c816();
			else if (irq && (!p.i || inwai))
				irq65c816();
			oldnmi = nmi;
			if (output) {
				snemdebug("%06lu %02lu A=%04X X=%04X Y=%04X S=%04X %02X %i %i %04X "
					   "%i %02X %i %i\n",
					   pc | pbr, dbr >> 16, a.w, x.w, y.w, s.w, opcode, p.m,
					   cpumode, dp, lines, lines, xirq, yirq);
			}
			if (timetolive) {
				timetolive--;
				if (!timetolive) {
					output = 0;
				}
			}
			// if (pc==0x9DCD) { snemdebug("99BA %04X %04X
			// %04X %i\n",a.w,x.w,y.w,ins); }
			// if (pc==0xAA7C) { snemdebug("AA7C %04X %04X
			// %04X %i\n",a.w,x.w,y.w,ins); }
			// if (ins==286800) { output=1; }
			// if (ins==290200) { output=0; }
		}
		if (lines == 0xE0)
			nmi = 1;
		if (lines == 0xE0) {
			vbl = joyscan = 1;
			readjoy();
			// snemlog("Enter VBL\n");
		}
		if (lines == 0xE3)
			joyscan = 0;
		if (lines == 200 && key[KEY_ESC])
			break;
	}
	frames++;
}

void initsnem()
{
	allocmem();
	initppu();
	initspc();
	makeopcodetable();
	install_keyboard();
	install_timer();
	snemlog("Timer installed - %s\n", timer_driver->ascii_name);
	install_int_ex(oncesec, MSEC_TO_TIMER(1000));
	initdsp();
}

void resetsnem()
{
	resetppu();
	resetspc();
	resetdsp();
	reset65c816();
	if (pal)
		install_int_ex(hz60, BPS_TO_TIMER(50));
	else
		install_int_ex(hz60, BPS_TO_TIMER(60));
}

int windowdisable;

#if !defined(WIN32_GUI)
void wakeupmainthread()
{
	char windowtitle[256];
	if (changefps) {
		changefps = 0;
		sprintf(windowtitle, "NeuSneM: %i fps, %i", fps, spcclck);
		set_window_title(windowtitle);
	}
}
void wakeupsoundthread() {}

int main(int argc, char* argv[])
{
	initsnem();
	loadrom(argv[1]);
	resetsnem();
	while (1) {
		//keypressed();
		while (1) {
			execframe();
			drawcount--;
		}
		if (key[KEY_ESC])
			return 1;
		if (key[KEY_1]) {
			while (key[KEY_1])
				ppumask ^= 1;
		}
		if (key[KEY_2]) {
			while (key[KEY_2])
				ppumask ^= 2;
		}
		if (key[KEY_3]) {
			while (key[KEY_3])
				ppumask ^= 4;
		}
		if (key[KEY_4]) {
			while (key[KEY_4])
				ppumask ^= 8;
		}
		if (key[KEY_5]) {
			while (key[KEY_5])
				ppumask ^= 16;
		}
		if (key[KEY_6]) {
			while (key[KEY_6])
				windowdisable ^= 1;
		}
		if (key[KEY_F1])
		   dumpchar();
		if (key[KEY_F2])
		   dumpbg2();
	}
	//dumpregs();
	//dumpspcregs();
	//dumpvram();
	//dumphdma();
	return 0;
}

END_OF_MAIN()
#endif // !defined(WIN32_GUI)
