#include <string.h>
#include <doslib.h>
#include <iocslib.h>
#include <rcddef.h>

#define PCM8CHK						\
({	register signed int _d0 asm ("d0");		\
	register signed int _d1 asm ("d1");		\
	_d1 = 0x50434d38;				\
	_d0 = 0x67;					\
	asm volatile("trap #15"				\
		:"=d"(_d1)				\
		:"0"(_d0),"0"(_d1)			\
		:"d0");					\
	_d0;						\
})

#define PCM8OUT(CH,DAT,LEN,ADDRS)			\
({	register unsigned short int _d0 asm ("d0");	\
	register unsigned       int _d1 asm ("d1");	\
	register unsigned       int _d2 asm ("d2");	\
	register unsigned     char *_a1 asm ("a1");	\
	_d0 = (CH);					\
	_d1 = (DAT);					\
	_d2 = (LEN);					\
	_a1 = (ADDRS);					\
	asm volatile("trap #2"				\
		:"=d"(_d0)				\
		:"d"(_d0),"d"(_d1),"d"(_d2),"a"(_a1)	\
		:"d0");					\
	_d0;						\
})

#define PCM8OFF(CH)					\
({	register unsigned short int _d0 asm ("d0");	\
	register unsigned       int _d2 asm ("d2");	\
	_d0 = (CH);					\
	_d2 = 0x0;					\
	asm volatile("trap #2"				\
		:"=d"(_d0)				\
		:"d"(_d0),"d"(_d2)			\
		:"d0");					\
	_d0;						\
})

#define PCM8CHG(CH,DAT)					\
({	register unsigned short int _d0 asm ("d0");	\
	register unsigned       int _d1 asm ("d1");	\
	_d0 = (CH);					\
	_d1 = (DAT);					\
	asm volatile("trap #2"				\
		:"=d"(_d0)				\
		:"d"(_d0),"d"(_d1)			\
		:"d0");					\
	_d0;						\
})

#define PCM8MSK(DAT)					\
({	register unsigned int _d0 asm ("d0");		\
	register unsigned int _d1 asm ("d1");		\
	_d0 = 0x01fb;					\
	_d1 = (DAT);					\
	asm volatile("trap #2"				\
		:"=d"(_d1)				\
		:"0"(_d0),"0"(_d1)			\
		:"d0");					\
	_d0;						\
})

#define strcopy(DSTR,SSTR)				\
({	char	*tmp_sstr = (SSTR);			\
	char	*tmp_dstr = (DSTR);			\
	while (*tmp_dstr++ = *tmp_sstr++)		\
	;						\
	(DSTR);						\
})

/* rcdcheck.o に含まれる */
extern void	rcd_check( void );			/* RCD 常駐チェックルーチン   */
extern struct	RCD_HEAD	*rcd;			/* RCD パラメータへのポインタ */
extern char	rcd_version[];				/* RCD のバージョン "n.nn"    */
/* keepchk.o に含まれる */
extern unsigned int	keepchk(unsigned int,unsigned int);	/*常駐チェックルーチン*/

struct pcmlist{
unsigned char	*address;
unsigned int	size; };

static void		INT_MAIN();
static void		INT_LOAD();
static void		OUT_PCM(unsigned short);
static void		LOAD_PCM(unsigned char,char *);
static unsigned int	CtoI(unsigned char);
/*static void		FREE_MEM();*/
static char		*SEARCH_FILE(char *,char *);
static void		SAVE_SYSWORK();
static void		LOAD_SYSWORK();

struct pcmlist	PCM_BUF[256];
unsigned long	IOCS_WORK_BUF[512];
char		FILE_NAME_BUF[64][21];
char		FULLNAME[256];
unsigned char	KANKYO[256];
unsigned char	FILE_NUM_BUF[64];
unsigned short	ORIGINAL_PROG_BUF[8];
unsigned short	ORIGINAL_PROG_BUF2[6];

short		FLAG_RCPCM;
short		FLAG_FREE_MEM;
short		FLAG_COMMENT;
short		FLAG_2STEP_LOAD;
unsigned char	FILE_NUM;
unsigned char	TEMP_FILE_NUM;
unsigned char	*KENV;
unsigned int	KEEP_DATA;
unsigned int	OLD_MASK_DATA;
extern int	_HEND;
extern int	_PSP;
unsigned int	PSP;
unsigned short	*FLAG_INDOS;
unsigned char	*DOS_COMMAND;
unsigned int	OLD_PAN;
unsigned char	*SONG_ADRS;
unsigned char	MPU;
short		NONAME;

/*int ofst=0x2A24,ofst2=0x1818; */ /* v3.01f */
/*int ofst=0x29CE 		*/ /* v3.01g */
/*int ofst=0x2a20,ofst_2=0x6448;*/ /* v3.01h */
/*int ofst=0x2a20,ofst_2=0x640c;*/ /* v3.01i */
/*int ofst=0x2a66,ofst_2=0x6452;*/ /* v3.01j */
/*int ofst=0x2a68+0x10,ofst_2=0x649e,ofst_3=0xbf8;*/   /* v3.01l */
/*int ofst=0x2a74+0x10,ofst_2=0x64aa,ofst_3=0xc16;*/   /* v3.01m */
  int ofst=0x2b14+0x10,ofst_2=0x6678,ofst_3=0xc16;     /* v3.01q */

				/* RCPCM_ON@@@@@@@@@@@@ */
				/* RCPCM_OFF@@@@@@@@@@@ */
				/* NNVf:NNVf:NNVf:NNVfP */
				/* NNVf|NNVf|NNVf|NNVfP */
				/* LOADNN:FILENAME.PCM  */
				/* TEMPO:nnnn           */
				/* COMMENT_ON           */
				/* COMMENT_OFF          */

void INT_MAIN()
{
__builtin_saveregs(0);

rcd->flg_song=1;

if(FLAG_RCPCM==1){

	if((*(SONG_ADRS+4)==':')&&(*(SONG_ADRS+9)==':')&&(*(SONG_ADRS+14)==':')){
		OUT_PCM(0);
		goto NEXT;
	}

	if((*(SONG_ADRS+4)=='|')&&(*(SONG_ADRS+9)=='|')&&(*(SONG_ADRS+14)=='|')){
		OUT_PCM(4);
		goto NEXT;
	}

	if(FLAG_2STEP_LOAD == 1){				/*２段ロード*/
		FLAG_2STEP_LOAD=0;
		if(FILE_NUM++ == 64){
			B_PRINT((unsigned char *)"/BUFFER OVER/");
			FILE_NUM--;
			goto NEXT;
		}
		strcopy(FILE_NAME_BUF[FILE_NUM],(char *)SONG_ADRS);
		FILE_NUM_BUF[FILE_NUM]=TEMP_FILE_NUM;
		goto NEXT;
	}

	if( (*(SONG_ADRS+6)==':') && (*(unsigned long *)SONG_ADRS == 0x4c4f4144) ){ /* LOADnn: */
		if( *(SONG_ADRS+7)==' '){
			FLAG_2STEP_LOAD=1;
			TEMP_FILE_NUM=(CtoI(*(SONG_ADRS+4))<<4) | CtoI(*(SONG_ADRS+5));
			goto NEXT;
		}
		if(FILE_NUM++ == 64){
			B_PRINT((unsigned char *)"/BUFFER OVER/");
			FILE_NUM--;
			goto NEXT;
		}
		strcopy(FILE_NAME_BUF[FILE_NUM],(char *)(SONG_ADRS+7));
		FILE_NUM_BUF[FILE_NUM]=(CtoI(*(SONG_ADRS+4))<<4) | CtoI(*(SONG_ADRS+5));
		goto NEXT;
	}

	if( (*(unsigned short *)(SONG_ADRS+4) == 0x4f3a) && (*(unsigned long *)(SONG_ADRS+0) == 0x54454d50) ){ /* TEMPO: */
		asm("move.l %0,(%1)"::
			"d"( (CtoI(*(SONG_ADRS+6))<<12) |
			     (CtoI(*(SONG_ADRS+7))<< 8) |
			     (CtoI(*(SONG_ADRS+8))<< 4) |
			     (CtoI(*(SONG_ADRS+9))    ) ),
			"a"( (unsigned long)&rcd->title + ofst_2 )
			);
		goto NEXT;
	}

	if( *(unsigned long *)SONG_ADRS == 0x434f4d4d ){
		if( strncmp((char *)SONG_ADRS,"COMMENT_ON",10) == 0){
			FLAG_COMMENT=0;
			goto NEXT;
		}
		if( strncmp((char *)SONG_ADRS,"COMMENT_OFF",11) == 0){
			FLAG_COMMENT=1;
			goto NEXT;
		}
	}

}

if( *(SONG_ADRS+19)=='@' ){
	if( strncmp((char *)SONG_ADRS,"RCPCM_OFF@@@@@@@@@@@",20) == 0){	/*ＲＣＰＣＭオフ*/
		FLAG_RCPCM=0;
		FLAG_FREE_MEM=1;
		FLAG_COMMENT=0;
		goto NEXT;
	}
	if( strncmp((char *)SONG_ADRS,"RCPCM_ON@@@@@@@@@@@@",20) == 0){	/*ＲＣＰＣＭオン*/
		FLAG_RCPCM=1;
		FLAG_FREE_MEM=1;
		FLAG_COMMENT=0;
		goto NEXT;
	}
}
NEXT:

return;
}

void INT_LOAD()
{
__builtin_saveregs(0);
asm("move.w	sr,-(sp)");
	if( (FILE_NUM != 0) && (*FLAG_INDOS != 1) ){
		SAVE_SYSWORK();
		for(FILE_NUM=FILE_NUM;FILE_NUM>0;FILE_NUM--){
			LOAD_PCM(FILE_NUM_BUF[FILE_NUM],FILE_NAME_BUF[FILE_NUM]);
		}
		LOAD_SYSWORK();
	}
	if( (FLAG_FREE_MEM == 1) && (*DOS_COMMAND != 0x49) ){
		short pcm_num;
		for(pcm_num=0;pcm_num<256;pcm_num++){
			if((unsigned long)PCM_BUF[pcm_num].address>0){
				MFREE((int)PCM_BUF[pcm_num].address);
				PCM_BUF[pcm_num].size=0;
				PCM_BUF[pcm_num].address=0;
			}
		}
		FLAG_FREE_MEM=0;
	}
asm("move.w	(sp)+,sr");
rcd->stepcount++;
}

void OUT_PCM(unsigned short ch_offset)
{
unsigned int	pan;
unsigned short	ch_num;

pan=CtoI(*(SONG_ADRS+19));
for(ch_num=0;ch_num<4;ch_num++){
	unsigned char	*song_adrs;
	unsigned int	out_data=0,pcm_num=0;
	song_adrs=SONG_ADRS+5*ch_num;

	pcm_num=(CtoI(*(song_adrs+0))<< 4) | CtoI(*(song_adrs+1));
	out_data=
		(CtoI(*(song_adrs+2))<<16) |
		(CtoI(*(song_adrs+3))<< 8) |
		pan;				/* v|f|p*/ 
	if(pcm_num==0xfff){
		if( (out_data>=0xffff00) && (pan==OLD_PAN) )continue;
		PCM8CHG(0x70+ch_num+ch_offset,out_data);
	}else{
		PCM8OFF(ch_num+ch_offset);
		if(PCM8OUT(ch_num+ch_offset,
				out_data,
					PCM_BUF[pcm_num].size,
						PCM_BUF[pcm_num].address)
							 != 0)B_PRINT((unsigned char *)"/PCM8 ERROR/");
	}
}
OLD_PAN=pan;
if(FLAG_COMMENT==1)rcd->flg_song=0;
}

unsigned int CtoI(unsigned char ascii)
{
if( (ascii >= 0x30) && (ascii <= 0x39) )return(ascii-0x30);
if( (ascii >= 0x41) && (ascii <= 0x46) )return(ascii-0x37);
if( (ascii >= 0x61) && (ascii <= 0x66) )return(ascii-0x57);
return(0xff);
}

void LOAD_PCM(unsigned char pcm_num,char *pcm_name)
{
int file_pointer;
struct	FILBUF	info;
unsigned char	i;
unsigned char	flag_rpd;
int pcm_size;

(*rcd->end)();

asm ("andi.w #%11111000_11111111,sr");

strcopy(FULLNAME,pcm_name);
for(i=0;i<255;i++)
{
	if(FULLNAME[i]==0)break;
	if(FULLNAME[i]==' ')
	{
		FULLNAME[i]=0;
		break;
	}
}
if((file_pointer = OPEN((unsigned char *)FULLNAME,0x000)) < 0 )
{	/*ルートにファイルが無い*/
	if(NONAME==1)return;
	if(SEARCH_FILE((char *)KANKYO,FULLNAME)==0)return;
	file_pointer = OPEN((unsigned char *)FULLNAME,0x000);
}
FILES(&info,(unsigned char *)FULLNAME,0x3f);

for(i=0;i<256;i++)
{
	if(FULLNAME[i]==0)break;
}

if(strcmpi(&FULLNAME[i-4],".rpd") == 0){flag_rpd=1;}else{flag_rpd=0;}

BLOCK:
if(flag_rpd==1)
{
	if(READ(file_pointer,(unsigned char *)&pcm_size,4)==0)goto READEND;
	PCM_BUF[pcm_num].size=pcm_size;
}else{
	PCM_BUF[pcm_num].size = info.filelen;
}

if((unsigned long)PCM_BUF[pcm_num].address>0)
{
	MFREE((int)PCM_BUF[pcm_num].address);
	PCM_BUF[pcm_num].address=0;
}
if( (unsigned long)(PCM_BUF[pcm_num].address = (unsigned char *)MALLOC2(2,PCM_BUF[pcm_num].size)) > 0x81000000  )
{	/*メモリが足りません。*/
	CLOSE(file_pointer);
	PCM_BUF[pcm_num].address=0;
	return;
}
asm volatile("move.l %0,(%1)"::"d"(PSP-0x10),"a"(PCM_BUF[pcm_num].address-0x10+0x04));
READ(file_pointer,PCM_BUF[pcm_num].address,PCM_BUF[pcm_num].size);
if(flag_rpd==1)
{
	pcm_num++;
	goto BLOCK;
}
READEND:

CLOSE(file_pointer);

(*rcd->begin)();

}

char *SEARCH_FILE(char *path_name,char *search_name)
{
struct FILBUF	info;
char temp_file_name[64];
strcopy(temp_file_name,path_name);
if(FILES(&info,(unsigned char *)strcat(temp_file_name,"\\*.*"),0x3f)>=0){
	if(info.name[0] != '.'){
		if(strcmp((char *)info.name,search_name)==0){
			strcopy(FULLNAME,path_name);
			strcat(FULLNAME,"\\");
			strcat(FULLNAME,(char *)info.name);
			return(FULLNAME);
			}
		if((info.atr && 0x10) == 1){
			strcopy(temp_file_name,path_name);
			strcat(temp_file_name,"\\");
			strcat(temp_file_name,(char *)info.name);
			if((unsigned long)SEARCH_FILE(temp_file_name,search_name)>0)return(FULLNAME);
		}
	}
	while(NFILES(&info)>=0){
		if(info.name[0] != '.'){
			if(strcmp((char *)info.name,search_name)==0){
				strcopy(FULLNAME,path_name);
				strcat(FULLNAME,"\\");
				strcat(FULLNAME,(char *)info.name);
				return(FULLNAME);
			}
			if((info.atr && 0x10) == 1){
				strcopy(temp_file_name,path_name);
				strcat(temp_file_name,"\\");
				strcat(temp_file_name,(char *)info.name);
				if((unsigned long)SEARCH_FILE(temp_file_name,search_name)>0)return(FULLNAME);
			}
		}
	}
}
return(0);
}

void SAVE_SYSWORK()
{
int i;
	for(i=0;i<512;i++){
		IOCS_WORK_BUF[i]=((unsigned long *)0x800)[i];
	}
}
void LOAD_SYSWORK()
{
int i;
	for(i=0;i<512;i++){
		((unsigned long *)0x800)[i]=IOCS_WORK_BUF[i];
	}
}
/****************/
/* main program */
/****************/
void main( int argc, char *argv[] )
{
unsigned char rcpcm_title[]="X68k RCPCM: RCPCM play driver v0.34  Copyright 1995,96 ITOKEN\r\n";
unsigned long rcpcm_adrs;
int ssp;
int *adrs;

B_PRINT(rcpcm_title);

PSP=_PSP;
KEEP_DATA = keepchk(PSP-0x10,0);
if( (argc > 1) && ((argv[1][0] == '/') || (argv[1][0] == '-')) && ((argv[1][1] == 'r') || (argv[1][1] == 'R'))){
	if( (KEEP_DATA & 0b1000_0000_0000_0000_0000_0000_0000_0000) == 0){
		B_PRINT((unsigned char *)"RCPCMは常駐していません。\r\n" );
		EXIT2(1);
	}else{
		short	i;
		rcd_check();
		rcpcm_adrs=((KEEP_DATA & 0xffffff) + 0x10) - PSP;

		if( *(unsigned char *)(rcpcm_adrs+(unsigned long)&MPU) == 1){
			asm("	.cpu 68030			");
			asm("	moveq.l	#9,d0			");
			asm("	movec	d0,cacr			");
			asm("@@:				");
			asm("	move.l	(sp)+,d0		");
			asm("	.cpu	68000			");
		}

		ssp=B_SUPER(0);
		{
		int i;
			for(i=0;i<4;i++){
				((unsigned short *)(rcd->title+ofst))[i]=
				((unsigned short *)(rcpcm_adrs+(unsigned long)&ORIGINAL_PROG_BUF[0]))[i];
			}
			for(i=0;i<3;i++){
				((unsigned short *)(rcd->title+ofst_3))[i]=
				((unsigned short *)(rcpcm_adrs+(unsigned long)&ORIGINAL_PROG_BUF2[0]))[i];
			}
		}
		B_SUPER(ssp);

		PCM8MSK(*(unsigned int *)(rcpcm_adrs + (unsigned long)&OLD_MASK_DATA));

		for(i=0;i<256;i++){
			MFREE((int)*(unsigned long *)(rcpcm_adrs+(unsigned long)&(PCM_BUF[i].address)));
		}

		MFREE((KEEP_DATA & 0xffffff) + 0x10);
		B_PRINT((unsigned char *)"RCPCMの常駐を解除しました。\r\n" );
		EXIT2(0);
	}
}
if( (KEEP_DATA & 0b1000_0000_0000_0000_0000_0000_0000_0000) != 0){
	B_PRINT((unsigned char *)"RCPCMは既に常駐しています。\r\n" );
	EXIT2(1);	}

rcd_check();	/* RCD 常駐チェック & ポインタ獲得 */

if( ( int ) rcd == 0 ) {
	B_PRINT((unsigned char *)"RCDが常駐していません。\r\n" );
	EXIT2(1);	}

if(!(PCM8CHK > 0)){
	B_PRINT((unsigned char *)"PCM8が常駐していません。\r\n" );
	EXIT2(1);	}

OLD_MASK_DATA=PCM8MSK(0b100_11011111_00000000);

(*rcd->end)();
FLAG_INDOS = (unsigned short *)INDOSFLG();
DOS_COMMAND = (unsigned char *)(INDOSFLG()+2);
if(GETENV((unsigned char *)"RCPCM_PATH",KENV,KANKYO)<0)
{
	NONAME=1;
}
SONG_ADRS=(unsigned char *)&rcd->song[0];

asm("	.cpu 68030			");
asm("	move.l	d0,-(sp)		");
asm("	moveq.l	#1,d0			");
asm("	move.b	(table-1,pc,d0*2),_MPU	");
asm("	bra	@f			");
asm("table:	.dc.b	0,1		");
asm("@@:				");
asm("	tst.b	_MPU			");
asm("	beq	@f			");
asm("	moveq.l	#9,d0			");
asm("	movec	d0,cacr			");
asm("@@:				");
asm("	move.l	(sp)+,d0		");
asm("	.cpu	68000			");

ssp=B_SUPER(0);
{
int i;
	for(i=0;i<4;i++){
		ORIGINAL_PROG_BUF[i]=((unsigned short *)(rcd->title+ofst))[i];
	}
	*(unsigned short *)(rcd->title+ofst)=0x4eb9;
	*(unsigned long  *)(rcd->title+ofst+2)=(unsigned long)INT_MAIN;
	*(unsigned short *)(rcd->title+ofst+2+4)=0x4e71;
	for(i=0;i<3;i++){
		ORIGINAL_PROG_BUF2[i]=((unsigned short *)(rcd->title+ofst_3))[i];
	}
	*(unsigned short *)(rcd->title+ofst_3)=0x4eb9;
	*(unsigned long *)(rcd->title+ofst_3+2)=(unsigned long)INT_LOAD;
}
B_SUPER(ssp);

B_PRINT((unsigned char *)"常駐します。\r\n" );
KEEPPR(_HEND-PSP-0xf0,0);
/*KEEPPR((unsigned int)rcpcm_title-PSP-0xf0,0);*/
}
