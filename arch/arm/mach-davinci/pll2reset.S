#include <asm/BigMacro.h>

/* PLL2-SYSTEM PLL MMRs */
	.equiv	_PLL_CTL,		0x0100
	.equiv	_PLL_PLLM,		0x0110
	.equiv	_PLL_DIV1,		0x0118
	.equiv	_PLL_DIV2,		0x011c
	.equiv	_PLL_PLLCMD,		0x0138
	.equiv	_PLL_PLLSTAT,		0x013c
	.equiv	CTL_PLLEN,	(1 << 0)
	.equiv	CTL_PLLPWRDN,	(1 << 1)
	.equiv	CTL_PLLRST,	(1 << 3)
	.equiv	CTL_PLLDIS,	(1 << 4)
	.equiv	CTL_PLLENSRC,	(1 << 5)
	.equiv	CTL_CLKMODE,	(1 << 8)	/* internal osc or CLKIN select*/
	.equiv	GOSET,		1
	.equiv	GOSTAT,		1
	.equiv	_PLL_LOCK_COUNT,	0x2000

	.equiv	MM_SDRSTAT,		0x0004
	.equiv	MM_SDBCR,		0x0008
	.equiv	MM_SDRCR,		0x000c
	.equiv	MM_SDTIMR,		0x0010
	.equiv	MM_SDTIMR2,		0x0014
	.equiv	MM_DDRPHYCR,		0x00e4
	.equiv	MM_VTPIOCR,		0x00f0	@ VTP IO Control register
	.equiv	SDRCR_SR,		(1 << 31)
	.equiv	SDRCR_MCLKSTOPEN,	(1 << 30)
	.equiv	SDRSTAT_PHYRDY,		(1 << 2)
	.equiv	DDRPHYCR_DLLPWRDN,	(1 << 4)

	.equiv	M2_PTCMD,		0x120
	.equiv	M2_PTSTAT,		0x128
	.equiv	M2_MDSTAT_,		0x800
	.equiv	M2_MDSTAT_DDR2,	0x834		@13
	.equiv	M2_MDCTL_,		0xa00
	.equiv	M2_MDCTL_DDR2,	0xa34		@13

	.equiv	SDTIM0_VAL_162MHz, 0x28923211
	.equiv	SDTIM1_VAL_162MHz, 0x0016c722
	.equiv	DUMMY_VAL, 0xa55aa55a


	.equiv	C_PSC_FLAG_MASK,	0x1f
	.equiv	C_DDRCTL_VAL,			0x50006405
	.equiv	C_SDREF_VAL,			0x000005c3
	.equiv	DAVINCI_LPSC_DDR2,	13

	.macro	DelayLoop rCnt,val
	BigMov	\rCnt,\val
99:	subs	\rCnt,\rCnt,#1
	bne		99b
	.endm

	.macro WaitForBitLow	rTmp,rBase,offset,mask
99:	ldr		\rTmp,[\rBase,#\offset]
	tst		\rTmp,#\mask
	bne		99b
	.endm

	.macro WaitForValue		rTmp,rBase,offset,mask,val
99:	ldr		\rTmp,[\rBase,#\offset]
	and		\rTmp,\rTmp,#\mask
	cmp		\rTmp,#\val
	bne		99b
	.endm
/*
	int i;
	@ Select bypass mode
	REGVALUE(PLL2_CTL) &= ~CTL_PLLENSRC;
	REGVALUE(PLL2_CTL) &= ~CTL_PLLEN;
	for (i=0; i < 0x218; i++);
	REGVALUE(PLL2_CTL) &= ~(CTL_PLLRST);
	REGVALUE(PLL2_CTL) &= ~(CTL_PLLDIS);

	REGVALUE(PLL2_PLLM) = mult;
	REGVALUE(PLL2_DIV1) = div1;
	REGVALUE(PLL2_DIV2) = div2;

	REGVALUE(PLL2_CMD) = GOSET;
	while (REGVALUE(PLL2_STAT) & GOSTAT);
	udelay(60);

	REGVALUE(PLL2_CTL) |= CTL_PLLRST;
	udelay(500);
	REGVALUE(PLL2_CTL) |= CTL_PLLEN;
*/
/*
 *	r0 - mult, r1 - div1, r2 - div2(ddr)
 *	r5 - MM, r6 - PLL2, r7 - M2
 */
pll2_change_routine:
@stop ddr2
	ldr	ip, [r5,#MM_SDBCR]	@save 128MB/256MB setting

	ldr	r3, [r5,#MM_SDRCR]
	orr	r3,r3,$SDRCR_SR
	str	r3, [r5,#MM_SDRCR]
	orr	r3,r3,$SDRCR_MCLKSTOPEN
	str	r3, [r5,#MM_SDRCR]

	ldr	r3, [r5,#MM_DDRPHYCR]
	orr	r3,r3,$DDRPHYCR_DLLPWRDN
	str	r3, [r5,#MM_DDRPHYCR]

1:	ldr	r3, [r5,#MM_SDRSTAT]
	tst	r3,$SDRSTAT_PHYRDY
	bne	1b
@disable lpsc vclk
@???

@ Select PLL bypass mode
	ldr	r3, [r6,#_PLL_CTL]
	bic	r3, r3, $CTL_CLKMODE
	str	r3, [r6,#_PLL_CTL]
	bic	r3, r3, $CTL_PLLENSRC
	str	r3, [r6,#_PLL_CTL]
	bic	r3, r3, $CTL_PLLEN
	str	r3, [r6,#_PLL_CTL]
	/* Wait for a few cycles to allow PLLEN Mux to switch properly to bypass Clock */
	DelayLoop r9,0x20

	bic	r3, r3, $CTL_PLLRST
	str	r3, [r6,#_PLL_CTL]
	bic	r3, r3, #CTL_PLLPWRDN
	str	r3, [r6,#_PLL_CTL]
	bic	r3, r3, $CTL_PLLDIS
	str	r3, [r6,#_PLL_CTL]

	orr	r1, r1, $(1 << 15)
	orr	r2, r2, $(1 << 15)
	str	r0, [r6,#_PLL_PLLM]
	str	r1, [r6,#_PLL_DIV1]
	str	r2, [r6,#_PLL_DIV2]

	/* Program the GOSET bit to take new divider values */
	mov	r0, $GOSET
	str	r0, [r6,#_PLL_PLLCMD]

	WaitForBitLow r0,r6,_PLL_PLLSTAT,GOSTAT	@ Wait for Done

	/* Wait for PLL to Reset Properly */
	DelayLoop r0,0x218

	/* Bring PLL out of Reset */
	ldr	r0, [r6,#_PLL_CTL]
	orr	r0, r0, $CTL_PLLRST
	str	r0, [r6,#_PLL_CTL]

	/* Wait for PLL to Lock */
	DelayLoop r0,_PLL_LOCK_COUNT

	/* Enable the PLL */
	ldr	r0, [r6,#_PLL_CTL]
	orr	r0, r0, $CTL_PLLEN
	str	r0, [r6,#_PLL_CTL]

@enable lpsc vclk
@???
	ldr	r3, [r5,#MM_SDRCR]
	bic	r3,r3,$SDRCR_SR
	str	r3, [r5,#MM_SDRCR]
	bic	r3,r3,$SDRCR_MCLKSTOPEN
	str	r3, [r5,#MM_SDRCR]

	ldr	r3, [r5,#MM_DDRPHYCR]
	bic	r3,r3,$DDRPHYCR_DLLPWRDN
	str	r3, [r5,#MM_DDRPHYCR]
1:	ldr	r3, [r5,#MM_SDRSTAT]
	tst	r3,$SDRSTAT_PHYRDY
	beq	1b

	/*------------------------------------------------------*
	 * Issue Soft Reset to DDR Module			*
	 *------------------------------------------------------*/

	/* Shut down the DDR2 LPSC Module */
	ldr	r0, [r7,#M2_MDCTL_DDR2]
	bic	r0, r0, #C_PSC_FLAG_MASK
	orr	r0, r0, $0x03
	str	r0, [r7,#M2_MDCTL_DDR2]

	/* Enable the Power Domain Transition Command */
	ldr	r0, [r7,#M2_PTCMD]
	orr	r0, r0, $0x01
	str	r0, [r7,#M2_PTCMD]

	WaitForBitLow r0,r7,M2_PTSTAT,0x01	@Wait for Transition Complete(PTSTAT)
	WaitForValue r0,r7,M2_MDSTAT_DDR2,0x1f,0x03		@Wait for DDR2 Controller Enable Completion

	/*------------------------------------------------------*
	 * Program DDR2 MMRs for 162MHz Setting			*
	 *------------------------------------------------------*/

	/* Program PHY Control Register */
	BigMov	r0,C_DDRCTL_VAL
	str	r0, [r5,#MM_DDRPHYCR]

	orr	r1, ip, $(1<<15)
	str	r1, [r5,#MM_SDBCR]

	/* Program SDRAM TIM-0 Config Register */
	BigMov	r0, SDTIM0_VAL_162MHz
	str	r0, [r5,#MM_SDTIMR]

	/* Program SDRAM TIM-1 Config Register */
	BigMov	r0, SDTIM1_VAL_162MHz
	str	r0, [r5,#MM_SDTIMR2]

	/* Program the SDRAM Bank Config Control Register */
	bic	r0,r1,#1<<15		@Lock timing registers
	str	r0, [r5,#MM_SDBCR]

	/* Program SDRAM SDREF Config Register */
	BigMov	r0,C_SDREF_VAL
	str	r0, [r5,#MM_SDRCR]

	/*------------------------------------------------------*
	 * Issue Soft Reset to DDR Module			*
	 *------------------------------------------------------*/

	/* Issue a Dummy DDR2 read/write */
	BigMov	r0, DUMMY_VAL
	str	r0, [r8]
	ldr	r0, [r8]

	/* Shut down the DDR2 LPSC Module */
	ldr	r0, [r7,#M2_MDCTL_DDR2]
	bic	r0, r0, #C_PSC_FLAG_MASK
	orr	r0, r0, $0x01
	str	r0, [r7,#M2_MDCTL_DDR2]

	/* Enable the Power Domain Transition Command */
	ldr	r0, [r7,#M2_PTCMD]
	orr	r0, r0, $0x01
	str	r0, [r7,#M2_PTCMD]

	WaitForBitLow r0,r7,M2_PTSTAT,0x01		@Wait for Transition Complete(PTSTAT)
	WaitForValue r0,r7,M2_MDSTAT_DDR2,0x1f,0x01	@Wait for DDR2 Controller Enable Completion

	/*------------------------------------------------------*
	 * Turn DDR2 Controller Clocks On			*
	 *------------------------------------------------------*/
	mov	r0,#DAVINCI_LPSC_DDR2

/* Works on Always On power domain only (no PD argument)
 * In: r0 - (unsigned int id)
 *	r7 - M2_BASE
 */
lpsc_On:
	WaitForBitLow r1,r7,M2_PTSTAT,0x01	@Wait for Transition Complete(PTSTAT)
	add		r2,r7,#M2_MDSTAT_
	ldr		r1,[r2,r0,LSL #2]	@mdstat
	and		r1,r1,#0x1f
	cmp		r1,#3
	moveq	pc,lr		@return if already on and enabled

	add		r2,r7,#M2_MDCTL_
	ldr		r1,[r2,r0,LSL #2]	@mdctl
	orr		r1,r1,#3
	str		r1,[r2,r0,LSL #2]	@mdctl

#if 0
	cmp		r0,#DAVINCI_LPSC_GPIO
@ Special treatment for some modules as for sprue14 p.7.4.2
	ldreq	r1,[r2,r0,LSL #2]	@mdctl
	orreq	r1,r1,#0x200
	streq	r1,[r2,r0,LSL #2]	@mdctl
#endif
	mov		r1,#1
	str		r1,[r7,#M2_PTCMD]

	WaitForBitLow r1,r7,M2_PTSTAT,0x03	@Wait for Transition Complete(PTSTAT)
	add		r2,r7,#M2_MDSTAT_
99:	ldr		r1,[r2,r0,LSL #2]	@mdstat
	and		r1,r1,#0x1f
	cmp		r1,#3
	bne		99b
	mov		pc,lr

pll2_change_routine_end:

/*
 * void copy_reset_pll2_routine(unsigned * dest);
 */
	.globl copy_reset_pll2_routine
copy_reset_pll2_routine:
	adr	r1, pll2_change_routine
	mov	r2, #(pll2_change_routine_end - pll2_change_routine)/4
1:	ldr	r3,[r1],#4
	str	r3,[r0],#4
	subs	r2, r2, #1
	bne	1b
	mov	pc,lr

	.globl reset_pll2
/*
 * void reset_pll2(mult, div1, div2(ddr), rtn, mmbase, pllbase, m2base, ddr2base);
 */
reset_pll2:
	stmdb	sp!,{r4,r5,r6,r7,r8,r9,ip,lr}
	
	ldr	r5, [sp,#8*4]	@MM_BASE
	ldr	r6, [sp,#9*4]	@PLL_BASE
	ldr	r9, [r6,#_PLL_CTL]	@ make sure page translation is cached
	ldr	r7, [sp,#10*4]	@M2_BASE
	ldr	ip, [r7,#M2_PTCMD]	@ make sure page translation is cached
	ldr	r8, [sp,#11*4]	@DDR2_BASE
	ldr	lr, [r8]		@ make sure page translation is cached
	@ make sure page translation is cached
	ldr	r9, [r3,#pll2_change_routine_end - pll2_change_routine -4]
	mov	r9, #0
	mcr	p15, 0, r9, c7, c10, 4		@ drain WB
	adr	lr,2f
	mov	pc,r3
2:
	ldmia	sp!,{r4,r5,r6,r7,r8,r9,ip,pc}
