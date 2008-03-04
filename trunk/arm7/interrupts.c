/*---------------------------------------------------------------------------------
	$Id: interrupts.c,v 1.10 2007/02/06 12:00:32 wntrmute Exp $
	Copyright (C) 2005
		Dave Murphy (WinterMute)
	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.
	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:
	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.
	$Log: interrupts.c,v $
	Revision 1.10  2007/02/06 12:00:32  wntrmute
	use uint32 for irqen
	
	Revision 1.9  2006/06/16 07:35:14  wntrmute
	more moves to REG_ values
	implement SetYtrigger helper function
	set bits in REG_DISPSTAT for IRQ_VCOUNT
	
	Revision 1.8  2006/02/21 00:12:35  wntrmute
	clear interrupts and enable REG_IME in irqInit
	
	Revision 1.7  2005/12/12 13:03:29  wntrmute
	set/clear LCD interrupts in irqen/Disable
	
	Revision 1.6  2005/11/07 01:11:08  wntrmute
	more doxygen
	moved IntrMain declaration to C file
	
	Revision 1.5  2005/09/20 05:00:24  wntrmute
	use new register definitions
	
	Revision 1.4  2005/09/04 16:28:04  wntrmute
	place irqTable in ewram
	Revision 1.3  2005/09/03 17:09:35  wntrmute
	added interworking aware interrupt dispatcher
---------------------------------------------------------------------------------*/
#include <u.h>
#include "../mem.h"
#include "nds.h"

typedef struct IntTable IntTable;
IntTable irqTable[MAX_INTERRUPTS];

void irqset(int mask, IntFn handler) {
	int i;

	for(i=0; i<MAX_INTERRUPTS; i++) 
		if(mask == (1<<i)) 
			break;
			
	if (i == MAX_INTERRUPTS)
		return;

	irqTable[i].handler	= handler;
	irqTable[i].mask	= mask;

	irqen(mask);
}

static void
irqhandler(void){
	int i;
	int ibits, oime;

	oime = REG_IME;
	REG_IME = 0;
	ibits = REG_IF;
	for(i=0; i<MAX_INTERRUPTS; i++){
		if (ibits & (1<<i) && irqTable[i].handler){
			irqTable[i].handler();
		}
		ibits &= ~(1<<i);
	}

	// ack. unhandled ints
	if (ibits != 0){
		REG_IF = ibits;
		*(ulong*)IRQCHECK7 = ibits;
	}
	REG_IME = oime;
}

void irqInit(void) {
	int i;
	// Set all interrupts to dummy functions.
	for(i = 0; i < MAX_INTERRUPTS; i++){
		irqTable[i].handler = 0;
		irqTable[i].mask = (1<<i);
	}
	
	*(ulong*)INTHAND7 = (ulong)irqhandler;
	REG_IE = 0;				// disable all interrupts
	REG_IF = ~0;			// clear all pending interrupts
//	REG_IME = 1;			// enable global interrupt
}

void irqClear(int mask) {
	int i;
	for (i=0; i<MAX_INTERRUPTS; i++)
		if (irqTable[i].mask == (1<<i))
			break;
			
	if (i == MAX_INTERRUPTS)
		return;

	irqTable[i].mask = 0;
	irqTable[i].handler = 0;
	
	irqDisable(mask);
}

void irqInitHandler(IntFn handler) {
	REG_IME = 0;
	REG_IF = ~0;
	REG_IE = 0;
	*(ulong*)INTHAND7 = (ulong)handler;
//	REG_IME = 1;
}

void irqen(uint32 irq) {
	if (irq & IRQ_VBLANK)
		REG_DISPSTAT |= DISP_VBLANK_IRQ;
	if (irq & IRQ_HBLANK)
		REG_DISPSTAT |= DISP_HBLANK_IRQ ;
	if (irq & IRQ_VCOUNT)
		REG_DISPSTAT |= DISP_YTRIGGER_IRQ;
	REG_IE |= irq;
//	REG_IME = 1;
}

void irqDisable(uint32 irq) {
	if (irq & IRQ_VBLANK)
		REG_DISPSTAT &= ~DISP_VBLANK_IRQ;
	if (irq & IRQ_HBLANK)
		REG_DISPSTAT &= ~DISP_HBLANK_IRQ;
	if (irq & IRQ_VCOUNT)
		REG_DISPSTAT &= ~DISP_YTRIGGER_IRQ;
	REG_IE &= ~irq;
}