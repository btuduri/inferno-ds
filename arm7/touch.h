/*---------------------------------------------------------------------------------
	$Id: touch.h,v 1.8 2005/10/03 21:19:34 wntrmute Exp $

	Microphone control for the ARM7

	Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
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

	$Log: touch.h,v $
	Revision 1.8  2005/10/03 21:19:34  wntrmute
	use ratiometric mode
	lock touchscreen on and average several readings
	
	Revision 1.7  2005/09/12 06:51:58  wntrmute
	tidied touch code
	
	Revision 1.6  2005/08/23 17:06:10  wntrmute
	converted all endings to unix

	Revision 1.5  2005/08/03 17:36:23  wntrmute
	moved touch struct to ipc

	Revision 1.4  2005/08/01 23:12:17  wntrmute
	extended touchReadXY to return touchscreen co-ordinates as well as screen co-ordinates

	Revision 1.3  2005/07/29 00:57:40  wntrmute
	updated file headers
	added touchReadXY function
	made header C++ compatible


---------------------------------------------------------------------------------*/



#define Scrwidth	256
#define Scrheight	192


#define Tscgettemp1    0x84
#define TscgetY        0x90
#define Tscgetbattery  0xA4
#define TscgetZ1       0xB4
#define TscgetZ2       0xC4
#define TscgetX        0xD0
#define Tscgetaux      0xE4
#define Tscgettemp2    0xF4
#define Pendown		(1<<6)


void touchReadXY(touchPosition *tp);

uint16 touchRead(uint32 cmd);
uint32 touchReadTemperature(int * t1, int * t2);
int16 readtsc(uint32 cmd, int16 *dmax, u8 *err);

