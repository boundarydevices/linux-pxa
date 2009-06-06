//
//  linux/include/asm-arm/BigMacro.h
//
//  Author:     Troy Kisky
//  Created:    Jun 30, 2002
//  Copyright:  Boundary Devices
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
	.nolist

//find set bit pair >= curbit
//out: __nBit
	.ifdef __ARMASM
	GBLA __nBit
	GBLA __nMask
	GBLA __nVal
;	DCD __nV1

.macro NextSetBitUp	val,curBit
	LCLA __nV1
	.set	__nBit,(\curBit)
	.set	__nV1,(\val)
	WHILE ( (__nBit < 30) :LAND: ( (__nV1 :AND: (0x03:SHL:__nBit))=0) )
	.set	__nBit,(__nBit)+2
	WEND
.endm

//find set bit pair <= curbit
//out: __nBit
.macro NextSetBitDown	val,curBit
	LCLA __nV1
	.set	__nBit,(\curBit)
	.set	__nV1,(\val)
	WHILE ( (__nBit <> 0) :LAND: ( (__nV1:AND:(0xc0:SHL:__nBit))=0) )
	.set	__nBit,(__nBit)-2
	WEND
.endm
	.else

.macro NextSetBitUp	val,curBit
	.set	__nBit,(\curBit)
	.set	__nV1,(\val)
	.if ((__nV1) & (0x03<<(__nBit)))
	.else
		.if ((__nBit)-30)
			NextSetBitUp __nV1,((__nBit)+2)
		.endif
	.endif
	.set	__nV1,0
.endm

//find set bit pair <= curbit
//out: __nBit
.macro NextSetBitDown	val,curBit
	.set	__nBit,(\curBit)
	.set	__nV1,(\val)
	.if ((__nV1)&(0xc0<<(__nBit)))
	.else
		.if (__nBit)
			NextSetBitDown __nV1,((__nBit)-2)
		.endif
	.endif
	.set	__nV1,0
.endm
	.endif

//OUT: __nMask
.macro NextSetMask	val
	NextSetBitDown	\val,24
	.if (__nBit>=20)
		NextSetBitUp	\val,__nBit
		.set __nMask,(0xff<<((__nBit)-16))
		.set __nMask,(((__nMask)>>16)+(((__nMask)<<16)&0xffff0000))
	.else
		.set __nMask,(0xff<<(__nBit))
	.endif

.endm

.macro Big2CC inst,dest,val
	.set __nVal,(\val)
	.if (__nVal)<>0
		NextSetMask __nVal
		\inst	\dest,\dest,#(__nVal)&(__nMask)
		Big2CC \inst,\dest,(__nVal)&~(__nMask)
	.endif
.endm

.macro BigAnd2CC cc,dest,val
	.set __nVal,(\val)
	.if (~__nVal)<>0
		NextSetMask __nVal
		.if (((__nVal)&~(__nMask))<>0)
			Big2CC bic\cc,\dest,~__nVal
		.else
			and\cc	\dest,\dest,#(__nVal)&(__nMask)
		.endif
	.endif
.endm

///////////////////////////////////////////////////////
.macro	BigMovCC  cc,dest, val
	.set __nVal,(\val)
	NextSetMask ~__nVal
	.if (((~(__nVal))&~(__nMask)) > 0x255)
		NextSetMask __nVal
		mov\cc	\dest,#(__nVal)&(__nMask)
		.if (((__nVal)&0xffff) ^ (((__nVal)>>16)&0xffff))<>0
			Big2CC orr\cc,\dest,(__nVal)&~(__nMask)
		.else
			.set __nVal,(__nVal)&~(__nMask)
			.if (__nVal)<>0
				NextSetMask __nVal
				orr\cc	\dest,\dest,#(__nVal)&(__nMask)
				.set __nVal,(__nVal)&~(__nMask)
				.if (__nVal)<>0
					orr\cc	\dest,\dest,\dest,LSR #16
				.endif
			.endif
		.endif
	.else
		mvn\cc	\dest,#(~(__nVal))&(__nMask)	//complement of complement is original
		Big2CC bic\cc,\dest,(~(__nVal))&~(__nMask)
	.endif
.endm


.macro	BigAddCC cc,dest,src,val
	.set __nVal,(\val)
	.if (__nVal)<>0
		NextSetMask -__nVal
		.if (((-(__nVal))&~(__nMask)) > 0x255)
			NextSetMask __nVal
			add\cc	\dest,\src,#(__nVal)&(__nMask)
			Big2CC add\cc,\dest,(__nVal)&~(__nMask)
		.else
			sub\cc	\dest,\src,#(-(__nVal))&(__nMask)
			Big2CC sub\cc,\dest,(-(__nVal))&~(__nMask)
		.endif
	.else
		mov\cc	\dest,\src
	.endif
.endm

.macro	BigSubCC cc,dest,src,val
	.set __nVal,(\val)
	BigAddCC \cc,\dest,\src,-__nVal
.endm

.macro BigCC inst,cc,dest,src,val
	.set __nVal,(\val)
	.if (__nVal)<>0
		NextSetMask __nVal
		\inst\cc	\dest,\src,#(__nVal)&(__nMask)
		Big2CC \inst\cc,\dest,(__nVal)&~(__nMask)
	.else
		mov\cc	\dest,\src
	.endif
.endm


.macro BigAndCC cc,dest,src,val
	.set __nVal,(\val)
	.if (~__nVal)<>0
		NextSetMask __nVal
		.if (((__nVal)&~(__nMask))<>0)
			BigCC bic,\cc,\dest,\src,~__nVal
		.else
			and\cc	\dest,\src,#(__nVal)&(__nMask)
		.endif
	.else
		mov\cc	\dest,\src
	.endif
.endm

/////////////////////////////////////
//dest, value
.macro BigAdd2 dest,val
	Big2CC add,\dest,\val
.endm
.macro BigAdd2Eq dest,val
	Big2CC addeq,\dest,\val
.endm
.macro BigAdd2Ne dest,val
	Big2CC addne,\dest,\val
.endm
.macro BigAdd2Cs dest,val
	Big2CC addcs,\dest,\val
.endm

.macro BigSub2 dest,val
	Big2CC sub,\dest,\val
.endm
.macro BigSub2Eq dest,val
	Big2CC subeq,\dest,\val
.endm
.macro BigSub2Ne dest,val
	Big2CC subne,\dest,\val
.endm

.macro BigOrr2 dest,val
	Big2CC orr,\dest,\val
.endm
.macro BigOrr2Eq dest,val
	Big2CC orreq,\dest,\val
.endm
.macro BigOrr2Ne dest,val
	Big2CC orrne,\dest,\val
.endm
.macro BigOrr2Lo dest,val
	Big2CC orrlo,\dest,\val
.endm
.macro BigOrr2Hs dest,val
	Big2CC orrhs,\dest,\val
.endm

.macro BigEor2 dest,val
	Big2CC eor,\dest,\val
.endm
.macro BigEor2Eq dest,val
	Big2CC eoreq,\dest,\val
.endm
.macro BigEor2Ne dest,val
	Big2CC eorne,\dest,\val
.endm
.macro BigEor2Cs dest,val
	Big2CC eorcs,\dest,\val
.endm
.macro BigEor2Cc dest,val
	Big2CC eorcc,\dest,\val
.endm

.macro BigBic2 dest,val
	Big2CC bic,\dest,\val
.endm
.macro BigBic2Eq dest,val
	Big2CC biceq,\dest,\val
.endm
.macro BigBic2Ne dest,val
	Big2CC bicne,\dest,\val
.endm

.macro BigAnd2 dest,val
	BigAnd2CC al,\dest,\val
.endm
.macro BigAnd2Eq dest,val
	BigAnd2CC eq,\dest,\val
.endm
.macro BigAnd2Ne dest,val
	BigAnd2CC ne,\dest,\val
.endm
/////////////////////////////////////

.macro BigMov dest,val
	BigMovCC  al,\dest,\val
.endm
.macro BigMovEq dest,val
	BigMovCC  eq,\dest,\val
.endm
.macro BigMovNe dest,val
	BigMovCC  ne,\dest,\val
.endm
// dest,src,value
.macro BigAdd dest,src,val
	BigAddCC  al,\dest,\src,\val
.endm
.macro BigAddEq dest,src,val
	BigAddCC  eq,\dest,\src,\val
.endm
.macro BigAddNe dest,src,val
	BigAddCC  ne,\dest,\src,\val
.endm

.macro BigSub dest,src,val
	BigSubCC  al,\dest,\src,\val
.endm
.macro BigSubEq dest,src,val
	BigSubCC  eq,\dest,\src,\val
.endm
.macro BigSubNe dest,src,val
	BigSubCC  ne,\dest,\src,\val
.endm

.macro BigOrr dest,src,val
	BigCC  orr,al,\dest,\src,\val
.endm
.macro BigOrrEq dest,src,val
	BigCC  orr,eq,\dest,\src,\val
.endm
.macro BigOrrNe dest,src,val
	BigCC  orr,ne,\dest,\src,\val
.endm

.macro BigEor dest,src,val
	BigCC  eor,al,\dest,\src,\val
.endm
.macro BigEorEq dest,src,val
	BigCC  eor,eq,\dest,\src,\val
.endm
.macro BigEorNe dest,src,val
	BigCC  eor,ne,\dest,\src,\val
.endm
.macro BigEorCs dest,src,val
	BigCC  eor,cs,\dest,\src,\val
.endm
.macro BigEorCc dest,src,val
	BigCC  eor,cc,\dest,\src,\val
.endm

.macro BigBic dest,src,val
	BigCC  bic,al,\dest,\src,\val
.endm
.macro BigBicEq dest,src,val
	BigCC  bic,eq,\dest,\src,\val
.endm
.macro BigBicNe dest,src,val
	BigCC  bic,ne,\dest,\src,\val
.endm

.macro BigAnd dest,src,val
	BigAndCC  al,\dest,\src,\val
.endm
.macro BigAndEq dest,src,val
	BigAndCC  eq,\dest,\src,\val
.endm
.macro BigAndNe dest,src,val
	BigAndCC  ne,\dest,\src,\val
.endm

// *******************************************************************************************
.macro	STARTUPTEXT
	GBLS AreaName
	AREA |.astart|,ALIGN=2,CODE
AreaName SETS "|.astart|"
.endm

.macro	DUP count,val
	LCLA	cnt
cnt	SETA $count
	WHILE ( cnt<>0)
		DCD	$val
cnt	SETA cnt-1
	WEND
.endm
	.list
