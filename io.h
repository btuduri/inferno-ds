
/*
 * Interrupt controller
 */

#define	MaxIRQbit	24		/* Maximum IRQ */

#define	VBLANKbit	0		/* Vertical blank */
#define	HBLANKbit	1		/* Horizontal blank */
#define	VCOUNTbit	2		/* Vertical count */
#define	TIMER0bit		3		/* Timer 0 */
#define	TIMER1bit		4		/* Timer 1 */
#define	TIMER2bit		5		/* Timer 2 */
#define	TIMER3bit		6		/* Timer 3 */
#define	UARTbit		7		/* Serial/Comms */
#define	GDMA0		8		/* DMA 0 */
#define	GDMA1		9		/* DMA 1 */
#define	GDMA2		10		/* DMA 2 */
#define	GDMA3		11		/* DMA 3 */
#define	KEYbit		12		/* Keypad */
#define	CARTbit		13		/* CART */
#define	ARM7bit		16		/* ARM7 IPC */
#define	FSENDbit		17		/* SEND FIFO empty */
#define	FRECVbit		18		/* RECV FIFO non empty */
#define	CARDbit		19		
#define	CARDLINEbit	20
#define	GEOMFIFObit	21		/* geometry FIFO */
#define	LIDbit		22
#define	SPIbit		23		/* SPI */
#define	WIFIbit		24		/* WIFI */

#define INTREG	((IntReg *)(SFRZERO + 0x208))
typedef struct IntReg IntReg;
struct IntReg {
	ulong	ime;	// Interrupt Master Enable
	ulong	pad1;
	ulong	ier;	// Interrupt Enable
	ulong	ipr;	// Interrupt Pending
};

/*
 * Fifo controller
 */

#define	FIFObase	0x04000184
#define	FIFOend		0x04100000
#define FIFOREG	((FifoReg*)FIFObase)
typedef struct FifoReg FifoReg;
struct FifoReg {
	ushort	ctl;
	ulong	send;
	uchar	pad1[FIFOend - FIFObase - 2*sizeof(ulong)];
	ulong	recv;
};

enum
{
	FifoTempty =	1<<0,
	FifoTfull =	1<<1,
	FifoTirq =	1<<2,
	FifoTflush =	1<<3,
	FifoRempty =	1<<8,
	FifoRfull =	1<<9,
	FifoRirq =	1<<10,
	Fifoerror =	1<<14,
	Fifoenable =	1<<15,
};

/*
 * Fifo message types
 */

enum
{
	Fcmdwidth =	4,
	Fcmdmask =	(1<<Fcmdwidth)-1,

	/* from arm9 to arm7 */
	F9brightness =	0,
	F9poweroff,
	F9reboot,
	F9leds,
	F9getrtc,
	F9setrtc,

	/* from arm7 to arm9 */
	F7keyup,
	F7keydown,
	F7mousedown,
	F7mouseup,
	F7print,
};

/* 
 * Fifo commands are encoded as follows:
 * |3 bits FIFO_TYPE | 29 bits type data (subcommands, data, ...) |
 */

#define FIFO_FIRMWARE (0 << 29)
#define FIFO_BUTTONS  (1 << 29)
#define FIFO_TOUCH    (2 << 29)
#define FIFO_MIC      (3 << 29)
#define FIFO_WIFI     (4 << 29)
#define FIFO_SOUND    (5 << 29)
#define FIFO_POWER    (6 << 29)
#define FIFO_TIME     (7 << 29)
#define FIFO_GET_TYPE(x) (((x)) & 0xe0000000)
#define FIFO_GET_TYPE_DATA(x) ((x) & 0x1fffffff)

#define FIFO_HIGH_BITS  (1<<16)
#define FIFO_LOW_BITS   (1<<17)

/* 
 * Fifo commands for firmware dumper.
 * |3 bits FIFO_FIRMWARE | 5 bits FIFO_CMD_FIRMWARE_x | 24 bits command data |
 */
#define FIFO_FIRMWARE_CMD(c, d) (FIFO_FIRMWARE | ((c & 0x1f) << 24) | (d & 0x00ffffff))
#define FIFO_FIRMWARE_GET_CMD(c) ((c >> 24) & 0x1f)
#define FIFO_FIRMWARE_GET_DATA(d) (d & 0x00ffffff)
#define FIFO_FIRMWARE_DECODE_ADDRESS(a) ((a) + 0x02000000)

enum FIFO_FIRMWARE_CMDS
{
	FIFO_FIRMWARE_CMD_BUFFER_ADDRESS,
	FIFO_FIRMWARE_CMD_READ
};
	
/* 
 * Fifo wifi commands are encoded as follows:
 * |3 bits FIFO_WIFI | 5 bits FIFO_CMD_WIFI_x | 24 bits command data |
 *  
 *  How command data is used depends on the command.
 *  Some commands, like recieve and transmit, send offsets into 0x02 RAM
 *  telling the other side where to read packet data from.
 *  Other commands, like those for setting WEP keys or the essid, further
 *  divide the command data into flags and actual data.
 */
#define FIFO_WIFI_CMD(c, d) (FIFO_WIFI | ((c & 0x1f) << 24) | (d & 0x00ffffff))
#define FIFO_WIFI_GET_CMD(c) ((c >> 24) & 0x1f)
#define FIFO_WIFI_GET_DATA(d) (d & 0x00ffffff)
#define FIFO_WIFI_DECODE_ADDRESS(a) ((a) + 0x02000000)

enum FIFO_WIFI_CMDS
{
	FIFO_WIFI_CMD_UP,
	FIFO_WIFI_CMD_DOWN,
	FIFO_WIFI_CMD_MAC_QUERY,
	FIFO_WIFI_CMD_TX,
	FIFO_WIFI_CMD_TX_COMPLETE,
	FIFO_WIFI_CMD_RX,
	FIFO_WIFI_CMD_RX_COMPLETE,
	FIFO_WIFI_CMD_STATS_QUERY,
	FIFO_WIFI_CMD_SET_ESSID,
	FIFO_WIFI_CMD_SET_CHANNEL,
	FIFO_WIFI_CMD_SET_WEPKEY,
	FIFO_WIFI_CMD_SET_WEPKEYID,
	FIFO_WIFI_CMD_SET_WEPMODE,
	FIFO_WIFI_CMD_AP_QUERY,
	FIFO_WIFI_CMD_SCAN,
	FIFO_WIFI_CMD_SET_AP_MODE,
	FIFO_WIFI_CMD_GET_AP_MODE,
};

/* 
 * Sound driver commands
 * |3 bits FIFO_SOUND | 1bit unused | 28 bits command data |
 */
#define FIFO_SOUND_CHANNELS	(1<<24)
#define FIFO_SOUND_DMA_ADDRESS	(2<<24)
#define FIFO_SOUND_DMA_SIZE	(3<<24)
#define FIFO_SOUND_FORMAT	(4<<24)
#define FIFO_SOUND_RATE		(5<<24)
#define FIFO_SOUND_TRIGGER	(6<<24)
#define FIFO_SOUND_POWER	(7<<24)

/* 
 * Dma controller
 */

#define DMAREG	((DmaReg*)(SFRZERO + 0x0b0))
typedef struct DmaReg DmaReg;
struct DmaReg {
	ulong	src;
	ulong	dst;
	ulong	ctl;
};

enum
{
	Dmaena		= 1<<31,		/* enable dma channel */
	Dmabusy		= 1<<31,		/* is dma channel busy */
	Dmairq		= 1<<30,		/* request an interrupt on dma */

	Dmastartnow	= 0,
	Dmastartcard	= 5<<27,
	Dmastartfifo	=  7<<27,
	
	Dma16bit		= 0,
	Dma32bit		= 1<<26,
	
	Dmasrcinc	= 0,
	Dmasrcdec	= 1<<23,
	Dmasrcfix	= 1<<24,
	Dmarepeat	= 1<<25,
	
	Dmadstinc	= 0,
	Dmadstdec	= 1<<21,
	Dmadstfix	= 2<<21,
	Dmadstrst	= 3<<21,
	
	Dmatxwords	= (Dmaena|Dma32bit|Dmastartnow),
	Dmatxhwords	= (Dmaena|Dma16bit|Dmastartnow),
	Dmafifo		= (Dmaena|Dma32bit|Dmadstfix|Dmastartfifo)
};

/* 
 * Timers control
 */

#define TIMERREG	((TimerReg*)(SFRZERO + 0x100))
typedef struct TimerReg TimerReg;
struct TimerReg {
	ushort data;	/* match */
	ushort ctl;
};

/* usage: TIMERREG->data = TIMER_BASE(div) / Hz, where div is TmrdivN */ 
#define TIMER_BASE(d)    (-(0x02000000 >> ((6+2*(d-1)) * (d>0))))

/*
 * TIMERREG ctl bits
 */
enum
{
	Tmrena		= (1<<7),	// enables the timer.
	Tmrirq		= (1<<6),	// request an Interrupt on overflow.
	Tmrcas		= (1<<2),	// cause the timer to count when the timer below overflows (unavailable for timer 0).
	Tmrdiv1 	= (0),		// set timer freq to 33.514 Mhz.
	Tmrdiv64 	= (1),		// set timer freq to (33.514 / 64) Mhz.
	Tmrdiv256 	= (2),		// set timer freq to (33.514 / 256) Mhz.
	Tmrdiv1024 	= (3),		// set timer freq to (33.514 / 1024) Mhz.
};

/* 
 * Lcd control (59.73 hz)
 */
#define LCDREG		((LcdReg*)LCD)
#define SUBLCDREG		((LcdReg*)SUBLCD)
typedef struct LcdReg LcdReg;
struct LcdReg {
	ulong lccr;	/* control */
	ulong lcsr;	/* status */
};

/*
 * LCDREG status bits
 */
enum
{
	DispInVblank	= 0,		/* display currently in a vertical blank. */
	DispInHblank	= 1,		/* display currently in a horizontal blank. */
	DispInVcount	= 2,		/* current scanline and %DISP_Y match. */
	DispIrqVblank	= 3,		/* Irq on vblank */
	DispIrqHblank	= 4,		/* Irq on hblank */
	DispIrqVcount	= 5,		/* Irq on Vcount*/
};

/*
 * VRAM & WRAM bank control
 */
#define VRAMREG	((VramReg*)VRAM)
typedef struct VramReg VramReg;
struct VramReg {
	uchar acr;
	uchar bcr;
	uchar ccr;
	uchar dcr;
	uchar ecr;
	uchar fcr;
	uchar gcr;
	uchar wcr; /* wram control */
	uchar hcr;
	uchar icr;
};

#define POWERREG ((PowerReg*)POWER)
typedef struct PowerReg PowerReg;
struct PowerReg {
	ushort pcr;
};

enum ARM7_power
{
	POWER_SOUND,	//	Controls the power for the sound controller.
	POWER_WIFI,	//	Controls the power for the wifi device.
};

/*
 * POWERREG bits
 */
enum ARM9_power
{
	POWER_LCD =			1<<0,
	POWER_2D_A =		1<<1,
	POWER_MATRIX = 		1<<2,
	POWER_3D_CORE = 		1<<3,
	POWER_2D_B =		1<<9,
	POWER_SWAP_LCDS =	1<<15,
};

#define SPIREG ((SpiReg*)SPI)
typedef struct SpiReg SpiReg;
struct SpiReg {
	ushort ctl;
	ushort data;
};

#define VIDMEMHI	((ushort*)VRAMTOP)
#define VIDMEMLO	((ushort*)VRAMZERO)

/* NDS file header info */
#define NDSHeader ((NDShdr*)0x027FFE00)
typedef struct NDShdr NDShdr;
struct NDShdr{
	char gtitle[12];
	char gcode[4];
	char mcode[2];
	uchar unitcode;	
	uchar devtype;		
	uchar devcapa;
	uchar rserv1[10];
	uchar romver;
	uchar rserv2;
	ulong romoff9;
	ulong entry9;
	ulong ram9;
	ulong size9;
	ulong romoff7;
	ulong entry7;
	ulong ram7;
	ulong fntoff;
	ulong fntsz;
	ulong fatoff;
	ulong fatsz;
	ulong ovloff9;
	ulong ovlsz9;
	ulong ovloff7;
	ulong ovlsz7;
	ulong romctl1;
	ulong romctl2;
	ulong iconoff;
	ushort sacrc;
	ushort romctl3;
	ulong arm9;
	ulong arm7;
	ulong magic1;
	ulong magic2;
	ulong appeoff;
	ulong romhdrsz;
	uchar rserv3[24];
	ulong rserv4[3];
	ulong rserv5[1];
	ushort logocrc;
	ushort hdrcrc;
};

/* 
 * Input Buttons
 */
#define KEYINPUT	0x04000130
#define KEYREG ((KeyReg*)KEYINPUT)
typedef struct KeyReg KeyReg;
struct KeyReg {
	ushort in;
	ushort inctl;
	ushort rcnt;
	ushort xy;
};

/*
 * KEYREG bits
 */
enum
{
	Numbtns	= 13,
	Btn9msk = (1<<10) - 1,
	Btn7msk = (1<<8)  - 1,

	// relative to KEYINPUT (arm9)
	Abtn	=	0,
	Bbtn	=	1,
	Selbtn	=	2,
	Startbtn=	3,
	Rightbtn=	4,
	Leftbtn	=	5,
	Upbtn	=	6,
	Downbtn	=	7,
	Rbtn	=	8,
	Lbtn	=	9,
	
	Xbtn	=	10,
	Ybtn	=	11,
	Dbgbtn	= 	13,
	Pdown	=	16,
	Lclose	= 	17,
	Maxbtns,

	// relative to XKEYS (arm7 only)
	Xbtn7	= 	0,
	Ybtn7	= 	1,
	Dbgbtn7	= 	3,	// debug btn
	Pdown7	= 	6,	// pen down
	Lclose7	= 	7,	// lid closed
};

enum
{
	Button1 =	1<<0,
	Button2 =	1<<1,
	Button3 =	1<<2,
};

/*
 * Synchronization reg
 */
#define IPCREG ((IpcReg*)0x04000180)
typedef struct IpcReg IpcReg;
struct IpcReg {
	ulong ctl;
};

enum
{
	Ipcdatain	= 0x00F,
	Ipcnotused	= 0x0F0,
	Ipcdataout	= 0xF00,
	Ipcgenirq	= 1<<13,
	Ipcirqena	= 1<<14,
};

/*
 * External memory reg
 */
#define EXMEMREG ((ExmReg*)0x04000204)
typedef struct ExmReg ExmReg;
struct ExmReg {
	ushort ctl;
};

enum
{
	Gbasramcycles	= (1<<1|1<<0),	/* (0-3 = 10, 8, 6, 18 cycles) */
	Gbarom1cycles	= (1<<3|1<<2),	/* (0-3 = 10, 8, 6, 18 cycles) */
	Gbarom2cycles	= 1<<4,		/* (0-1 = 6, 4 cycles) */
	Gbaslotphyout	= (1<<6|1<<5),	/* (0-3 = Low, 4.19MHz, 8.38MHz, 16.76MHz) */
	Arm7hasgba	= 1<<7,		/* (0=ARM9, 1=ARM7) */
	Arm7hasds	= 1<<11,	/* (0=ARM9, 1=ARM7) */
  	Ramaccessmode	= 1<<14,	/* (0=Async/GBA/Reserved, 1=Synchronous) */
	Arm7hasram	= 1<<15,	/* (0=ARM9 Priority, 1=ARM7 Priority) */
};

/*
 * discio interface
 */
enum
{
	Cread =			0x00000001,
	Cwrite =		0x00000002,
	Cslotgba =		0x00000010,
	Cslotnds =		0x00000020,
};

typedef struct Ioifc Ioifc;
struct Ioifc {
	char type[4];			/* name */
	ulong caps;			/* capabilities */
	
	int (* init)(void);
	int (* isin)(void);
	int (* read)(ulong start, ulong n, void* d);
	int (* write)(ulong start, ulong n, const void* d);
	int (* clrstat)(void);
	int (* deinit)(void);
};
