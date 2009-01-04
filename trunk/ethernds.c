#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "arm7/wifi.h"

#include "../port/error.h"
#include "../port/netif.h"
#include "etherif.h"

static int dbg = 0;
#define	DBG	if(dbg)
#define DPRINT	if(dbg)iprint

enum
{
	WNameLen	= 34,
	WNKeys		= 4,
	WKeyLen		= 14,
};

typedef struct Stats Stats;
struct Stats
{
	ulong	collisions;
	ulong	toolongs;
	ulong	tooshorts;
	ulong	aligns;
	ulong	txerrors;
};

typedef struct WKey WKey;
struct WKey
{
	ushort	len;
	char	dat[WKeyLen];
};

typedef struct WFrame	WFrame;
struct WFrame
{
	ushort	sts;
	ushort	rsvd0;
	ushort	rsvd1;
	ushort	qinfo;
	ushort	rsvd2;
	ushort	rsvd3;
	ushort	txctl;
	ushort	framectl;
	ushort	id;
	uchar	addr1[Eaddrlen];
	uchar	addr2[Eaddrlen];
	uchar	addr3[Eaddrlen];
	ushort	seqctl;
	uchar	addr4[Eaddrlen];
	ushort	dlen;
	uchar	dstaddr[Eaddrlen];
	uchar	srcaddr[Eaddrlen];
	ushort	len;
	ushort	dat[3];
	ushort	type;
};

typedef struct Ctlr Ctlr;
struct Ctlr {
	Lock;

	uchar	*base;
	int	type;
	int	rev;
	int	hasmii;
	int	phyad;
	int	bank;	/* currently selected bank */
	Block*	waiting;	/* waiting for space in FIFO */

	int 	attached;
	char	wantname[WNameLen];
	char	nodename[WNameLen];
	
	int	ptype;			// AP mode/type
	int	crypt;			// encryption off/on
	int	txkey;			// transmit key
	WKey	keys[WNKeys];		// default keys
	int	scan;

	Stats;
};

/* 
 * arm9 wifi interfacing code from DSWifi lib by Stephen Stair
 * using gbatek.txt as source of documentation
 *
 * Copyright (C) 2005 Stephen Stair - sgstair@akkit.org
 */


Wifi_Data W;
Wifi_Data* WifiData = (Wifi_Data*)uncached(&W);

typedef void (*WifiPacketHandler)(int, int);
typedef void (*WifiSyncHandler)(void);

WifiPacketHandler packethandler = 0;
WifiSyncHandler synchandler = 0;

#define SPINLOCK_OK		0x0000
#define SPINLOCK_INUSE	0x0001
#define SPINLOCK_ERROR	0x0002
#define Spinlock_Acquire(arg)	SPINLOCK_OK
#define Spinlock_Release(arg)

void Wifi_CopyMacAddr(volatile void * dest, volatile void * src) {
	((u16 *)dest)[0]=((u16 *)src)[0];
	((u16 *)dest)[1]=((u16 *)src)[1];
	((u16 *)dest)[2]=((u16 *)src)[2];
}

int Wifi_CmpMacAddr(volatile void * mac1,volatile  void * mac2) {
	return (((u16 *)mac1)[0]==((u16 *)mac2)[0]) && (((u16 *)mac1)[1]==((u16 *)mac2)[1]) && (((u16 *)mac1)[2]==((u16 *)mac2)[2]);
}

u32 Wifi_TxBufferWordsAvailable(void) {
	s32 size=WifiData->txbufIn-WifiData->txbufOut-1;
	if(size<0) size += WIFI_TXBUFFER_SIZE/2;
	return size;
}
void Wifi_TxBufferWrite(s32 start, s32 len, u16 * data) {
	int writelen;
	while(len>0) {
		writelen=len;
		if(writelen>(WIFI_TXBUFFER_SIZE/2)-start) writelen=(WIFI_TXBUFFER_SIZE/2)-start;
		len-=writelen;
		while(writelen) {
			WifiData->txbufData[start++]=*(data++);
			writelen--;
		}
		start=0;
	}
}

u16 Wifi_RxReadOffset(s32 base, s32 offset) {
	base+=offset;
	if(base>=(WIFI_RXBUFFER_SIZE/2)) base -= (WIFI_RXBUFFER_SIZE/2);
	return WifiData->rxbufData[base];
}

// datalen = size of packet from beginning of 802.11 header to end, but not including CRC.
int Wifi_RawTxFrame(u16 datalen, u16 rate, u16 * data) {
	Wifi_TxHeader txh;
	int sizeneeded;
	int base;
	sizeneeded=((datalen+12+4+3)/4)*2;
	if(sizeneeded>Wifi_TxBufferWordsAvailable()) {WifiData->stats[WSTAT_TXQUEUEDREJECTED]++; return -1; }
	txh.tx_rate=rate;
	txh.tx_length=datalen+4;
	base = WifiData->txbufOut;
	Wifi_TxBufferWrite(base,6,(u16 *)&txh);
	base += 6;
	if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;
	Wifi_TxBufferWrite(base,((datalen+3)/4)*2,data);
	base += ((datalen+3)/4)*2;
	if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;
	WifiData->txbufOut=base;
	WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
	WifiData->stats[WSTAT_TXQUEUEDBYTES]+=sizeneeded;
   if(synchandler) synchandler();
   return 0;
}

void Wifi_RawSetPacketHandler(WifiPacketHandler wphfunc) {
	packethandler=wphfunc;
}
void Wifi_SetSyncHandler(WifiSyncHandler wshfunc) {
   synchandler=wshfunc;
}

void Wifi_DisableWifi(void) {
	WifiData->reqMode=WIFIMODE_DISABLED;
	WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}
void Wifi_EnableWifi(void) {
	WifiData->reqMode=WIFIMODE_NORMAL;
	WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}
void Wifi_SetPromiscuousMode(int enable) {
	if(enable) WifiData->reqReqFlags |= WFLAG_REQ_PROMISC;
	else WifiData->reqReqFlags &= ~WFLAG_REQ_PROMISC;
}

void Wifi_ScanMode(void) {
	WifiData->reqMode=WIFIMODE_SCAN;
	WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
}
void Wifi_SetChannel(int channel) {
	if(channel<1 || channel>13) return;
	if(WifiData->reqMode==WIFIMODE_NORMAL || WifiData->reqMode==WIFIMODE_SCAN) {
		WifiData->reqChannel=channel;
	}
}

int Wifi_GetNumAP(void) {
	int i,j;
	j=0;
	for(i=0;i<WIFI_MAX_AP;i++) if(WifiData->aplist[i].flags&WFLAG_APDATA_ACTIVE) j++;
	return j;
}

int Wifi_GetAPData(int apnum, Wifi_AccessPoint * apdata) {
        int j;
    
	if(!apdata) return WIFI_RETURN_PARAMERROR;

	
	if(WifiData->aplist[apnum].flags&WFLAG_APDATA_ACTIVE) {
	    while(Spinlock_Acquire(WifiData->aplist[apnum])!=SPINLOCK_OK);
	    {
		// additionally calculate average RSSI here
		WifiData->aplist[apnum].rssi=0;
		for(j=0;j<8;j++) {
		    WifiData->aplist[apnum].rssi+=WifiData->aplist[apnum].rssi_past[j];
		}
		WifiData->aplist[apnum].rssi = WifiData->aplist[apnum].rssi >> 3;
		*apdata = WifiData->aplist[apnum]; // yay for struct copy!
		Spinlock_Release(WifiData->aplist[apnum]);
		return WIFI_RETURN_OK;
	    }
	}

	return WIFI_RETURN_ERROR;
}

int Wifi_FindMatchingAP(int numaps, Wifi_AccessPoint * apdata, Wifi_AccessPoint * match_dest) {
	int ap_match,i,j,n;
	Wifi_AccessPoint ap;
	u16 macaddrzero[3] = {0,0,0}; // check for empty mac addr
	ap_match=-1;
	for(i=0;i<Wifi_GetNumAP();i++){
		Wifi_GetAPData(i,&ap);
		for(j=0;j<numaps;j++) {
			if(apdata[j].ssid_len>32 || ((signed char)apdata[j].ssid_len)<0) continue;
			if(apdata[j].ssid_len>0) { // compare SSIDs
				if(apdata[j].ssid_len!=ap.ssid_len) continue;
				for(n=0;n<apdata[j].ssid_len;n++) {
					if(apdata[j].ssid[n]!=ap.ssid[n]) break;
				}
				if(n!=apdata[j].ssid_len) continue;
			}
			if(!Wifi_CmpMacAddr(apdata[j].macaddr,macaddrzero)) { // compare mac addr
				if(!Wifi_CmpMacAddr(apdata[j].macaddr,ap.macaddr)) continue;
			}
			if(apdata[j].channel!=0) { // compare channels
				if(apdata[j].channel!=ap.channel) continue;
			}
			if(j<ap_match || ap_match==-1) {
				ap_match=j;
				if(match_dest) *match_dest = ap;
			}
			if(ap_match==0) return ap_match;
		}
	}
	return ap_match;
}

int wifi_connect_state = 0; // -1==error, 0==searching, 1==associating, 2==dhcp'ing, 3==done, 4=searching wfc data
Wifi_AccessPoint wifi_connect_point;

int Wifi_DisconnectAP(void) {
	WifiData->reqMode=WIFIMODE_NORMAL;
	WifiData->reqReqFlags &= ~WFLAG_REQ_APCONNECT;
	WifiData->flags9&=~WFLAG_ARM9_NETREADY;

	wifi_connect_state=-1;
	return 0;
}

int Wifi_ConnectAP(Wifi_AccessPoint * apdata, int wepmode, int wepkeyid, u8 * wepkey) {
	int i;
	Wifi_AccessPoint ap;
	wifi_connect_state=-1;
	if(!apdata) return -1;
	if(((signed char)apdata->ssid_len)<0 || apdata->ssid_len>32) return -1;

	Wifi_DisconnectAP();

	wifi_connect_state=0;

	WifiData->wepmode9=wepmode; // copy data
	WifiData->wepkeyid9=wepkeyid;

	if(wepmode != WEPMODE_NONE && wepkey)
	{
		for(i=0;i<20;i++) {
			WifiData->wepkey9[i]=wepkey[i];
		}
	}

	i=Wifi_FindMatchingAP(1,apdata,&ap);
	if(i==0) {

		Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
		Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
		WifiData->ssid9[0]=ap.ssid_len;
		for(i=0;i<32;i++) {
			WifiData->ssid9[i+1]=ap.ssid[i];
		}
		WifiData->apchannel9=ap.channel;
		for(i=0;i<16;i++) WifiData->baserates9[i]=ap.base_rates[i];
		WifiData->reqMode=WIFIMODE_NORMAL;
		WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
		wifi_connect_state=1;
	} else {
		WifiData->reqMode=WIFIMODE_SCAN;		
		wifi_connect_point = *apdata;
	}
	return 0;
}
void Wifi_AutoConnect(void) {
	if(!(WifiData->wfc_enable[0]&0x80)) {
		wifi_connect_state=ASSOCSTATUS_CANNOTCONNECT;
	} else {
		wifi_connect_state=4;
		WifiData->reqMode=WIFIMODE_SCAN;		
	}
}

int Wifi_AssocStatus(void) {
	switch(wifi_connect_state) {
		case -1: // error
			return ASSOCSTATUS_CANNOTCONNECT;
		case 0: // searching
			{ 
				int i;
				Wifi_AccessPoint ap;
				i=Wifi_FindMatchingAP(1,&wifi_connect_point,&ap);
				if(i==0) {
					Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
					Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
					WifiData->ssid9[0]=ap.ssid_len;
					for(i=0;i<32;i++) {
						WifiData->ssid9[i+1]=ap.ssid[i];
					}
					WifiData->apchannel9=ap.channel;
					for(i=0;i<16;i++) WifiData->baserates9[i]=ap.base_rates[i];
					WifiData->reqMode=WIFIMODE_NORMAL;
					WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
					wifi_connect_state=1;
				}
			}
			return ASSOCSTATUS_SEARCHING;
		case 1: // associating
			switch(WifiData->curMode) {
			case WIFIMODE_DISABLED:
			case WIFIMODE_NORMAL:
			case WIFIMODE_DISASSOCIATE:
				return ASSOCSTATUS_DISCONNECTED;
            case WIFIMODE_SCAN:
                if(WifiData->reqReqFlags&WFLAG_REQ_APCONNECT) return ASSOCSTATUS_AUTHENTICATING;
                return ASSOCSTATUS_DISCONNECTED;
			case WIFIMODE_ASSOCIATE:
				switch(WifiData->authlevel) {
				case WIFI_AUTHLEVEL_DISCONNECTED:
					return ASSOCSTATUS_AUTHENTICATING;
				case WIFI_AUTHLEVEL_AUTHENTICATED:
				case WIFI_AUTHLEVEL_DEASSOCIATED:
					return ASSOCSTATUS_ASSOCIATING;
				case WIFI_AUTHLEVEL_ASSOCIATED:
#ifdef WIFI_USE_TCP_SGIP
					if(wifi_hw) {
						if(!(wifi_hw->ipaddr)) {
							sgIP_DHCP_Start(wifi_hw,wifi_hw->dns[0]==0);
							wifi_connect_state=2;
							return ASSOCSTATUS_ACQUIRINGDHCP;
						}
					}
					sgIP_ARP_SendGratARP(wifi_hw);
#endif
					wifi_connect_state=3;
					WifiData->flags9|=WFLAG_ARM9_NETREADY;
					return ASSOCSTATUS_ASSOCIATED;
				}
				break;
			case WIFIMODE_ASSOCIATED:
#ifdef WIFI_USE_TCP_SGIP
				if(wifi_hw) {
					if(!(wifi_hw->ipaddr)) {
						sgIP_DHCP_Start(wifi_hw,wifi_hw->dns[0]==0);
						wifi_connect_state=2;
						return ASSOCSTATUS_ACQUIRINGDHCP;
					}
				}
				sgIP_ARP_SendGratARP(wifi_hw);
#endif
				wifi_connect_state=3;
				WifiData->flags9|=WFLAG_ARM9_NETREADY;
				return ASSOCSTATUS_ASSOCIATED;
			case WIFIMODE_CANNOTASSOCIATE:
				return ASSOCSTATUS_CANNOTCONNECT;
			}
			return ASSOCSTATUS_DISCONNECTED;
		case 2: // dhcp'ing
#ifdef WIFI_USE_TCP_SGIP
			{
				int i;
				i=sgIP_DHCP_Update();
				if(i!=SGIP_DHCP_STATUS_WORKING) {
					switch(i) {
					case SGIP_DHCP_STATUS_SUCCESS:
						wifi_connect_state=3;
						WifiData->flags9|=WFLAG_ARM9_NETREADY;
						sgIP_ARP_SendGratARP(wifi_hw);
                        sgIP_DNS_Record_Localhost();
						return ASSOCSTATUS_ASSOCIATED;
					default:
					case SGIP_DHCP_STATUS_IDLE:
					case SGIP_DHCP_STATUS_FAILED:
						Wifi_DisconnectAP();
						wifi_connect_state=-1;
						return ASSOCSTATUS_CANNOTCONNECT;

					}
				}
			}
#else		
			// should never get here (dhcp state) without sgIP!
			Wifi_DisconnectAP();
			wifi_connect_state=-1;
			return ASSOCSTATUS_CANNOTCONNECT;
#endif
			return ASSOCSTATUS_ACQUIRINGDHCP;
		case 3: // connected!
			return ASSOCSTATUS_ASSOCIATED;
		case 4: // search nintendo WFC data for a suitable AP
			{
				int n,i;
				Wifi_AccessPoint ap;
				for(n=0;n<3;n++) if(!(WifiData->wfc_enable[n]&0x80)) break;
				n=Wifi_FindMatchingAP(n,(Wifi_AccessPoint*)WifiData->wfc_ap,(Wifi_AccessPoint*)&ap);
				if(n!=-1) {
#ifdef WIFI_USE_TCP_SGIP
					Wifi_SetIP(WifiData->wfc_config[n][0],WifiData->wfc_config[n][1],WifiData->wfc_config[n][2],WifiData->wfc_config[n][3],WifiData->wfc_config[n][4]);
#endif
					WifiData->wepmode9=WifiData->wfc_enable[n]&0x03; // copy data
					WifiData->wepkeyid9=(WifiData->wfc_enable[n]>>4)&7;
					for(i=0;i<16;i++) {
						WifiData->wepkey9[i]=WifiData->wfc_wepkey[n][i];
					}

					Wifi_CopyMacAddr(WifiData->bssid9, ap.bssid);
					Wifi_CopyMacAddr(WifiData->apmac9, ap.bssid);
					WifiData->ssid9[0]=ap.ssid_len;
					for(i=0;i<32;i++) {
						WifiData->ssid9[i+1]=ap.ssid[i];
					}
					WifiData->apchannel9=ap.channel;
					for(i=0;i<16;i++) WifiData->baserates9[i]=ap.base_rates[i];
					WifiData->reqMode=WIFIMODE_NORMAL;
					WifiData->reqReqFlags |= WFLAG_REQ_APCONNECT | WFLAG_REQ_APCOPYVALUES;
					wifi_connect_state=1;
					return ASSOCSTATUS_SEARCHING;

				}

			}
			return ASSOCSTATUS_SEARCHING;
	}
	return ASSOCSTATUS_CANNOTCONNECT;
}

void Wifi_Init(u32 initflags){
	memset(WifiData, 0, sizeof(Wifi_Data));
	WifiData->flags9 = WFLAG_ARM9_ACTIVE | (initflags & WFLAG_ARM9_INITFLAGMASK);
	nbfifoput(F9TWifi|F9WFinit, (ulong)WifiData);
}

int Wifi_CheckInit(void) {
	if(!WifiData) return 0;
	return ((WifiData->flags7 & WFLAG_ARM7_ACTIVE) && (WifiData->flags9 & WFLAG_ARM9_ARM7READY));
}

void Wifi_Update(void) {
	int cnt;
	int base, base2, len, fulllen;
	if(!WifiData) return;

#ifdef WIFI_USE_TCP_SGIP

	if(!(WifiData->flags9&WFLAG_ARM9_ARM7READY)) {
		if(WifiData->flags7 & WFLAG_ARM7_ACTIVE) {
			WifiData->flags9 |=WFLAG_ARM9_ARM7READY;
			// add network interface.
			wifi_hw = sgIP_Hub_AddHardwareInterface(&Wifi_TransmitFunction,&Wifi_Interface_Init);
            sgIP_timems=WifiData->random; //hacky! but it should work just fine :)
		}
	}
	if(WifiData->authlevel!=WIFI_AUTHLEVEL_ASSOCIATED && WifiData->flags9&WFLAG_ARM9_NETUP) {
		WifiData->flags9 &= ~(WFLAG_ARM9_NETUP);
	} else if(WifiData->authlevel==WIFI_AUTHLEVEL_ASSOCIATED && !(WifiData->flags9&WFLAG_ARM9_NETUP)) {
		WifiData->flags9 |= (WFLAG_ARM9_NETUP);
	}

#endif

	// check for received packets, forward to whatever wants them.
	cnt=0;
	while(WifiData->rxbufIn!=WifiData->rxbufOut) {
		base = WifiData->rxbufIn;
		len=Wifi_RxReadOffset(base,4);
		fulllen=((len+3)&(~3))+12;
#ifdef WIFI_USE_TCP_SGIP
		// Do lwIP interfacing for rx here
		if((Wifi_RxReadOffset(base,6)&0x01CF)==0x0008) // if it is a non-null data packet coming from the AP (toDS==0)
		{
			u16 framehdr[6+12+2+4];
			sgIP_memblock * mb;
			int hdrlen;
			base2=base;
			Wifi_RxRawReadPacket(base,22*2,framehdr);

        // ethhdr_print('!',framehdr+8);
			if((framehdr[8]==((u16 *)WifiData->MacAddr)[0] && framehdr[9]==((u16 *)WifiData->MacAddr)[1] && framehdr[10]==((u16 *)WifiData->MacAddr)[2]) ||
				(framehdr[8]==0xFFFF && framehdr[9]==0xFFFF && framehdr[10]==0xFFFF)) {
				// destination matches our mac address, or the broadcast address.
				//if(framehdr[6]&0x4000) { // wep enabled (when receiving WEP packets, the IV is stripped for us! how nice :|
				//	base2+=24; hdrlen=28;  // base2+=[wifi hdr 12byte]+[802 header hdrlen]+[slip hdr 8byte]
				//} else { 
					base2+=22; hdrlen=24;
				//}
          //  SGIP_DEBUG_MESSAGE(("%04X %04X %04X %04X %04X",Wifi_RxReadOffset(base2-8,0),Wifi_RxReadOffset(base2-7,0),Wifi_RxReadOffset(base2-6,0),Wifi_RxReadOffset(base2-5,0),Wifi_RxReadOffset(base2-4,0)));
				// check for LLC/SLIP header...
				if(Wifi_RxReadOffset(base2-4,0)==0xAAAA && Wifi_RxReadOffset(base2-4,1)==0x0003 && Wifi_RxReadOffset(base2-4,2)==0) {
					mb = sgIP_memblock_allocHW(14,len-8-hdrlen);
					if(mb) {
						if(base2>=(WIFI_RXBUFFER_SIZE/2)) base2-=(WIFI_RXBUFFER_SIZE/2);
						Wifi_RxRawReadPacket(base2,(len-8-hdrlen)&(~1),((u16 *)mb->datastart)+7);
						if(len&1) ((u8 *)mb->datastart)[len+14-1-8-hdrlen]=Wifi_RxReadOffset(base2,((len-8-hdrlen)/2))&255;
						Wifi_CopyMacAddr(mb->datastart,framehdr+8); // copy dest
						if(Wifi_RxReadOffset(base,6)&0x0200) { // from DS set?
							Wifi_CopyMacAddr(((u8 *)mb->datastart)+6,framehdr+14); // copy src from adrs3
						} else {
							Wifi_CopyMacAddr(((u8 *)mb->datastart)+6,framehdr+11); // copy src from adrs2
						}
						((u16 *)mb->datastart)[6]=framehdr[(hdrlen/2)+6+3]; // assume LLC exists and is 8 bytes.

						ethhdr_print('R',mb->datastart);

						// Done generating recieved data packet... now distribute it.
						sgIP_Hub_ReceiveHardwarePacket(wifi_hw,mb);

					}
				}
			}
		}

#endif

		// check if we have a handler
		if(packethandler) {
			base2=base+6;
			if(base2>=(WIFI_RXBUFFER_SIZE/2)) base2-=(WIFI_RXBUFFER_SIZE/2);
			(*packethandler)(base2,len);
		}

		base+=fulllen/2;
		if(base>=(WIFI_RXBUFFER_SIZE/2)) base-=(WIFI_RXBUFFER_SIZE/2);
		WifiData->rxbufIn=base;

		if(cnt++>80) break;
	}
}

/*
int Wifi_TransmitFunction(sgIP_memblock * mb) {
	// convert ethernet frame into wireless frame and output.
	// ethernet header: 6byte dest, 6byte src, 2byte protocol_id
	// assumes individual pbuf len is >=14 bytes, it's pretty likely ;) - also hopes pbuf len is a multiple of 2 :|
	int base,framelen, hdrlen, writelen;
   int copytotal, copyexpect;
	u16 framehdr[6+12+2];
	sgIP_memblock * t;
   framelen=mb->totallength-14+8 + (WifiData->wepmode7?4:0);

   if(!(WifiData->flags9&WFLAG_ARM9_NETUP)) {
	   SGIP_DEBUG_MESSAGE(("Transmit:err_netdown"));
	   sgIP_memblock_free(mb);
	   return 0; //?
   }
	if(framelen+40>Wifi_TxBufferWordsAvailable()*2) { // error, can't send this much!
		SGIP_DEBUG_MESSAGE(("Transmit:err_space"));
      sgIP_memblock_free(mb);
		return 0; //?
	}
	
	ethhdr_print('T',mb->datastart);
	framehdr[0]=0;
	framehdr[1]=0;
	framehdr[2]=0;
	framehdr[3]=0;
	framehdr[4]=0; // rate, will be filled in by the arm7.
	hdrlen=18;
	framehdr[7]=0;

	if(WifiData->curReqFlags&WFLAG_REQ_APADHOC) { // adhoc mode
		framehdr[6]=0x0008;
		Wifi_CopyMacAddr(framehdr+14,WifiData->bssid7);
		Wifi_CopyMacAddr(framehdr+11,WifiData->MacAddr);
		Wifi_CopyMacAddr(framehdr+8,((u8 *)mb->datastart));
	} else {
		framehdr[6]=0x0108;
		Wifi_CopyMacAddr(framehdr+8,WifiData->bssid7);
		Wifi_CopyMacAddr(framehdr+11,WifiData->MacAddr);
		Wifi_CopyMacAddr(framehdr+14,((u8 *)mb->datastart));
	}
	if(WifiData->wepmode7)  { framehdr[6] |=0x4000; hdrlen=20; }
	framehdr[17] = 0;
	framehdr[18] = 0; // wep IV, will be filled in if needed on the arm7 side.
	framehdr[19] = 0;

   framehdr[5]=framelen+hdrlen*2-12+4;
   copyexpect= ((framelen+hdrlen*2-12+4) +12 -4 +1)/2;
   copytotal=0;

	WifiData->stats[WSTAT_TXQUEUEDPACKETS]++;
	WifiData->stats[WSTAT_TXQUEUEDBYTES]+=framelen+hdrlen*2;

	base = WifiData->txbufOut;
	Wifi_TxBufferWrite(base,hdrlen,framehdr);
	base += hdrlen;
   copytotal+=hdrlen;
	if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;

	// add LLC header
	framehdr[0]=0xAAAA;
	framehdr[1]=0x0003;
	framehdr[2]=0x0000;
	framehdr[3]=((u16 *)mb->datastart)[6]; // frame type

	Wifi_TxBufferWrite(base,4,framehdr);
	base += 4;
   copytotal+=4;
	if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;

	t=mb;
	writelen=(mb->thislength-14);
	if(writelen) {
		Wifi_TxBufferWrite(base,(writelen+1)/2,((u16 *)mb->datastart)+7);
		base+=(writelen+1)/2;
      copytotal+=(writelen+1)/2;
		if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;
	}
	while(mb->next) {
		mb=mb->next;
		writelen=mb->thislength;
		Wifi_TxBufferWrite(base,(writelen+1)/2,((u16 *)mb->datastart));
		base+=(writelen+1)/2;
      copytotal+=(writelen+1)/2;
		if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;
	}
   if(WifiData->wepmode7) { // add required extra bytes
      base+=2;
      copytotal+=2;
      if(base>=(WIFI_TXBUFFER_SIZE/2)) base -= WIFI_TXBUFFER_SIZE/2;
   }
	WifiData->txbufOut=base; // update fifo out pos, done sending packet.

	sgIP_memblock_free(t); // free packet, as we're the last stop on this chain.

   if(copytotal!=copyexpect) {
      SGIP_DEBUG_MESSAGE(("Tx exp:%i que:%i",copyexpect,copytotal));
   }
   if(synchandler) synchandler();
	return 0;
}
*/

static const char * ASSOCSTATUS_STRINGS[] = {
	[ASSOCSTATUS_DISCONNECTED] = "disconnected",	// not *trying* to connect
	[ASSOCSTATUS_SEARCHING] = "searching",		// data given does not completely specify an AP, looking for AP that matches the data.
	[ASSOCSTATUS_AUTHENTICATING] = "authing",	// connecting...
	[ASSOCSTATUS_ASSOCIATING] = "associng",		// connecting...
	[ASSOCSTATUS_ACQUIRINGDHCP] = "dhcping",	// connected to AP, but getting IP data from DHCP
	[ASSOCSTATUS_ASSOCIATED] = "connected",		// Connected!
	[ASSOCSTATUS_CANNOTCONNECT] = "error",		// error in connecting...
};

/*
 * devether interfacing
 */

static long
ifstat(Ether* ether, void* a, long n, ulong offset)
{
	Wifi_AccessPoint *ap;
	ulong *stats;
	ushort *debug;
	char *p, *e, *tmp;
	Ctlr *ctlr;
	int i, status;

	DPRINT("ifstat\n");
	
	if (ether->ctlr == nil)
		return 0;

	if(n == 0 || offset != 0)
		return 0;

	ctlr = ether->ctlr;
	tmp = p = smalloc(READSTR);
	if(waserror()) {
		free(tmp);
		nexterror();
	}
	e = tmp+READSTR;

	stats = WifiData->stats;
	debug = WifiData->debug;
	p = seprint(p, e, "tx: %lud (%lud/%lud) Q %lud (%lud)\n",
		stats[WSTAT_TXPACKETS], stats[WSTAT_TXDATABYTES], stats[WSTAT_TXBYTES],
		stats[WSTAT_TXQUEUEDPACKETS], stats[WSTAT_TXQUEUEDBYTES]);
	p = seprint(p, e, "rx: %lud (%lud/%lud) Q %lud (%lud)\n",
		stats[WSTAT_RXPACKETS], stats[WSTAT_RXDATABYTES], stats[WSTAT_RXBYTES],
		stats[WSTAT_RXQUEUEDPACKETS], stats[WSTAT_RXQUEUEDBYTES]);
	p = seprint(p, e, "txreject: %lud rxlost: %lud\n",
		stats[WSTAT_TXQUEUEDREJECTED], stats[WSTAT_RXQUEUEDLOST]);

	p = seprint(p, e, "ie: 0x%ux if: 0x%ux ints: %ud tx: %ux/%ux\n",
		debug[1], debug[2], debug[6], debug[3], debug[4]);

	status = 0; //Wifi_AssocStatus();
	p = seprint(p, e, "mode (0x%ux/0x%ux) auth 0x%ux status (0x%ux) %s\n",
		WifiData->curMode, WifiData->reqMode, WifiData->authlevel,
		status, ASSOCSTATUS_STRINGS[status]);	

	if(0){
		p = seprint(p, e, "hwcnt: ");
		for(i=WSTAT_HW_1B0; i<NUM_WIFI_STATS; i++)
			p = seprint(p, e, "%lux, ", stats[i]);
		p = seprint(p, e, "\n");
	}

	ap = WifiData->aplist;
	// order by signal quality 
	if (ctlr->scan)
	for(i=0; i < WIFI_MAX_AP; i++, ap++){
		if ((void*)ap == nil || !(ap->flags & WFLAG_APDATA_ACTIVE))
			continue;

		p = seprint(p, e, "%d: %s ch=%d (0x%ux)", i, ap->ssid, ap->channel, ap->flags);

		if(0)
		p = seprint(p, e, "sec=%s%s%s m=%s c=%s%s",
			(ap->flags & WFLAG_APDATA_WEP? "wep": ""),
			(ap->flags & WFLAG_APDATA_WPA? "wpa": ""),
			!(ap->flags & (WFLAG_APDATA_WEP|WFLAG_APDATA_WPA))? "": "off",

			(ap->flags & WFLAG_APDATA_ADHOC? "hoc": "man"),
			(ap->flags & WFLAG_APDATA_COMPATIBLE? "c": ""),
			(ap->flags & WFLAG_APDATA_EXTCOMPATIBLE? "e": "")
			);

		if(1)
		p = seprint(p, e, " q=%x", ap->rssi);
			
		if(0)
		p = seprint(p, e, " b=%x%x%x%x%x%x",
			ap->bssid[0], ap->bssid[1], ap->bssid[2], ap->bssid[3], ap->bssid[4], ap->bssid[5]);
		if(0)
		p = seprint(p, e, " m=%x%x%x%x%x%x",
			ap->macaddr[0], ap->macaddr[1], ap->macaddr[2], ap->macaddr[3], ap->macaddr[4], ap->macaddr[5]);
		p = seprint(p, e, "\n");
	}

	n = readstr(offset, a, n, tmp);
	poperror();
	free(tmp);
	
	return n;
}

static int
w_option(Ctlr* ctlr, char* buf, long n)
{
	int i, r;
	WKey *key;
	Cmdbuf *cb;

	r = 0;
	cb = parsecmd(buf, n);
	if(cb->nf < 2)
		r = -1;
	else if(cistrcmp(cb->f[0], "power") == 0){
		if(cistrcmp(cb->f[1], "off") == 0)
			Wifi_DisableWifi();
		else if(cistrcmp(cb->f[1], "on") == 0)
			Wifi_EnableWifi();
		else if(cistrcmp(cb->f[1], "up") == 0)
			Wifi_Update();
		else
			r = -1;
	}
	else if(cistrcmp(cb->f[0], "essid") == 0){
		if(cistrcmp(cb->f[1], "default") == 0){
			Wifi_AutoConnect(); // request connect
		}else {
			i = atoi(cb->f[1]);
			if(0 <= i && i < Wifi_GetNumAP())
				Wifi_ConnectAP(&WifiData->aplist[i], 0, ctlr->txkey, (u8*)ctlr->keys[ctlr->txkey].dat);

			Wifi_AssocStatus(); // check status			
			Wifi_Update();
		}
	}
	else if(cistrcmp(cb->f[0], "station") == 0){
		memset(ctlr->nodename, 0, sizeof(ctlr->nodename));
		strncpy(ctlr->nodename, cb->f[1], WNameLen);
	}
	else if(cistrcmp(cb->f[0], "channel") == 0){
		if((i = atoi(cb->f[1])) >= 1 && i <= 16)
			Wifi_SetChannel(i);
		else
			r = -1;
	}
	else if(cistrcmp(cb->f[0], "crypt") == 0){
		if(cistrcmp(cb->f[1], "off") == 0)
			ctlr->crypt = 0;
		else if(cistrcmp(cb->f[1], "on") == 0)
			ctlr->crypt = 1;
		else if((i = atoi(cb->f[1])) >= 0 && i < 3)
			ctlr->crypt = i;
		else
			r = -1;
	}
	else if(strncmp(cb->f[0], "key", 3) == 0){
		if((i = atoi(cb->f[0]+3)) >= 1 && i <= WNKeys){
			ctlr->txkey = i-1;
			key = &ctlr->keys[ctlr->txkey];
			key->len = strlen(cb->f[1]);
			if (key->len > WKeyLen)
				key->len = WKeyLen;
			memset(key->dat, 0, sizeof(key->dat));
			memmove(key->dat, cb->f[1], key->len);
		}
		else
			r = -1;
	}
	else if(cistrcmp(cb->f[0], "txkey") == 0){
		if((i = atoi(cb->f[1])) >= 1 && i <= WNKeys){
			ctlr->txkey = i-1;
		}else
			r = -1;
	}
	else if(cistrcmp(cb->f[0], "scan") == 0){
		if(cistrcmp(cb->f[1], "off") == 0)
			ctlr->scan = 0;
		else if(cistrcmp(cb->f[1], "on") == 0)
			ctlr->scan = 1;
		else if((i = atoi(cb->f[1])) == 0 || i == 1)
			ctlr->scan = i;
		else
			r = -1;
		
		if(ctlr->scan)
			Wifi_ScanMode();
	}
	else if(cistrcmp(cb->f[0], "dbg") == 0){
		dbg = atoi(cb->f[1]);
	}
	else
		r = -2;
	free(cb);
	return r;
}

static long
ctl(Ether* ether, void* buf, long n)
{
	Ctlr *ctlr;
	char *p;

	DPRINT("ctl\n");
	
	if(ether->ctlr == nil)
		error(Enonexist);

	ctlr = ether->ctlr;
	if(ctlr->attached == 0)
		error(Eshutdown);

	ilock(ctlr);
	if(p = strchr(buf, '='))
		*p = ' ';
	if(w_option(ctlr, buf, n)){
		iunlock(ctlr);
		error(Ebadctl);
	}

	iunlock(ctlr);

	return n;
}


static void
promiscuous(void* arg, int on)
{
	USED(arg, on);
	DPRINT("promiscuous\n");
}

static void
attach(Ether *ether)
{
	Ctlr* ctlr;
	
	DPRINT("attach\n");
	if (ether->ctlr == nil)
		return;

	ctlr = (Ctlr*) ether->ctlr;
	if (ctlr->attached == 0){
		ctlr->attached = 1;
	}
}

static char*
dump_pkt(uchar *data, ushort len)
{
	uchar *c;
	static char buff[2024];
	char *c2;

	c = data;
	c2 = buff;
	while ((c - data) < len) {
		if (((*c) >> 4) > 9)
			*(c2++) = ((*c) >> 4) - 10 + 'A';
		else
			*(c2++) = ((*c) >> 4) + '0';

		if (((*c) & 0x0f) > 9)
			*(c2++) = ((*c) & 0x0f) - 10 + 'A';
		else
			*(c2++) = ((*c) & 0x0f) + '0';
		c++;
		if ((c - data) % 2 == 0)
			*(c2++) = ' ';
	}
	*c2 = '\0';
	return buff;
}

static void
txloadpacket(Ether *ether)
{
	Ctlr *ctlr;
	Block *b;
	int lenb;

	ctlr = ether->ctlr;
	b = ctlr->waiting;
	ctlr->waiting = nil;
	if(b == nil)
		return;	// shouldn't happen
/*
	// only transmit one packet at a time
	lenb = BLEN(b);
	
	// wrap up packet information and send it to arm7
	memmove((void *)ctlr->txpktbuf, b->rp, lenb);
	ctlr->txpkt.len = lenb;
	ctlr->txpkt.data = (void *)ctlr->txpktbuf;
	freeb(b);

	if(1)print("dump txpkt[%lud] @ %lux:\n%s",
		(ulong)ctlr->txpkt.len, ctlr->txpkt.data,
		dump_pkt((uchar*)ctlr->txpkt.data, ctlr->txpkt.len));

	// write data to memory before ARM7 gets hands on
	dcflush(&ctlr->txpkt, sizeof(ctlr->txpkt));
*/
}

static void
txstart(Ether *ether)
{
	Ctlr *ctlr;

	ctlr = ether->ctlr;
	if(ctlr->waiting != nil)	/* allocate pending; must wait for that */
		return;
	for(;;){
		if((ctlr->waiting = qget(ether->oq)) == nil)
			break;
		/* ctlr->waiting is a new block to transmit: allocate space */
		txloadpacket(ether);
	}
}

enum {
	WF_Data		= 0x0008,
	WF_Fromds	= 0x0200,
};

static void
rxstart(Ether *ether)
{
	static uchar bcastaddr[Eaddrlen] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	Wifi_RxHeader *rx_hdr;
	WFrame *f;
	Ctlr *ctlr;
	Block* bp;
	Etherpkt* ep;

	if (ether->ctlr == nil)
		return;

	ctlr = ether->ctlr;
/*
	// invalidate cache before we read data written by ARM7
	dcflush(&ctlr->rxpkt, sizeof(ctlr->rxpkt));

	if(1)print("dump rxpkt[%lud] @ %lux:\n%s",
		(ulong)ctlr->rxpkt.len, ctlr->rxpkt.data,
		dump_pkt((uchar*)ctlr->rxpkt.data, ctlr->rxpkt.len));

	rx_hdr = (Wifi_RxHeader*)(uchar*)&ctlr->rxpkt + 2;
	f = (WFrame *)(uchar*)&ctlr->rxpkt + 2;

	if ((f->framectl & 0x01CF) == WF_Data) {
		if (memcmp(ether->ea, f->addr1, Eaddrlen) == 0
			|| memcmp(ether->ea, bcastaddr, Eaddrlen) == 0){

			// hdrlen == 802.11 header length  bytes
			int base2, hdrlen;
			base2 = 22;
			hdrlen = 24;
			// looks like WEP IV and IVC are removed from RX packets

			// check for LLC/SLIP header...
			if (((ushort *) rx_hdr)[base2 - 4 + 0] == 0xAAAA
			    && ((ushort *) rx_hdr)[base2 - 4 + 1] == 0x0003
			    && ((ushort *) rx_hdr)[base2 - 4 + 2] == 0) {
				// mb = sgIP_memblock_allocHW(14,len-8-hdrlen);
				// Wifi_RxRawReadPacket(base2,(len-8-hdrlen)&(~1),((u16 *)mb->datastart)+7);
				int len = rx_hdr->byteLength;
				bp = iallocb(ETHERHDRSIZE + len - 8 + hdrlen + 2);
				if (!bp)
					return;

				ep = (Etherpkt*) bp->wp;
				memmove(ep->d, f->addr1, Eaddrlen);
				if (f->framectl & WF_Fromds)
					memmove(ep->s, f->addr3, Eaddrlen);
				else
					memmove(ep->s, f->addr2, Eaddrlen);
		
				memmove(ep->type,&f->type,2);
				bp->wp = bp->rp+(ETHERHDRSIZE+f->dlen);

				etheriq(ether, bp, 1);
			}
		}

	}
*/
}

static void
transmit(Ether *ether)
{
	Ctlr *ctlr;

	DPRINT("transmit\n");
	ctlr = ether->ctlr;
	ilock(ctlr);
	txstart(ether);
	iunlock(ctlr);
}

static void
interrupt(Ureg*, void *arg)
{
	Ether *ether;
	Ctlr *ctlr;
	ulong type;
	
	DPRINT("interrupt\n");
	ether = arg;
	ctlr = ether->ctlr;
	if (ctlr == nil)
		return;
	
	/* IPCSYNC irq:
	 * used by arm7 to notify the arm9 
	 * that there's wifi activity (tx/rx)
	 */ 
	type = IPCREG->ctl & Ipcdatain;
	ilock(ctlr);
	if (type == I7WFrxdone)
		rxstart(ether);
	else if (type == I7WFtxdone)
		print("txdone\n");
	intrclear(ether->irq, 0);
	iunlock(ctlr);
}

/* set scanning interval */
static void
scanbs(void *a, uint secs)
{
	USED(a, secs);
	DPRINT("scanbs\n");
}

static int
etherndsreset(Ether* ether)
{
	int i;
	char *p;
	uchar ea[Eaddrlen];
	Ctlr *ctlr;

	DPRINT("etherndsreset\n");
	if((ctlr = malloc(sizeof(Ctlr))) == nil)
		return -1;

	ilock(ctlr);
	
	Wifi_Init(WIFIINIT_OPTION_USELED);
	
	memset(ea, 0, sizeof(ea));
	if(memcmp(ether->ea, ea, Eaddrlen) == 0)
		panic("ethernet address not set");

	for(i = 0; i < ether->nopt; i++){
		/*
		 * The max. length of an 'opt' is ISAOPTLEN in dat.h.
		 * It should be > 16 to give reasonable name lengths.
		 */
		if(p = strchr(ether->opt[i], '='))
			*p = ' ';
		w_option(ctlr, ether->opt[i], strlen(ether->opt[i]));
	}

	// link to ether
	ether->ctlr = ctlr;
	ether->attach = attach;
	ether->transmit = transmit;
	ether->interrupt = interrupt;
	ether->ifstat = ifstat;
	ether->ctl = ctl;
	ether->scanbs = scanbs;
	ether->promiscuous = promiscuous;

	ether->arg = ether;

	iunlock(ctlr);
	return 0;
}

void
etherndslink(void)
{
	addethercard("nds",  etherndsreset);
}

