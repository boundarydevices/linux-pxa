#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <video/davincifb.h>
#include <asm/div64.h>
#include <asm/io.h>
/*
 ******************************************************************************
 */
#define HALF_A_SECOND		(NSEC_PER_SEC >> 1)
#define FOURTH_OF_A_SECOND	(NSEC_PER_SEC >> 2)

ulong get_timer_delta(struct timespec *p)
{
	int sec,nsec;
	struct timespec	t;
	getnstimeofday(&t);

	sec = t.tv_sec - p->tv_sec;
	nsec = t.tv_nsec - p->tv_nsec;
	return sec * NSEC_PER_SEC + nsec;
}

/*
 ******************************************************************************
 */
static struct lcd_panel_info_t const lcd_panels_[] = {
{
	name: "dmt640x350_85",
	pixclock: 31500000,
	xres: 640,		//832 = 640+64+96+32
	yres: 350,		//445 = 350+3+60+32
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 64,
	left_margin: 96,
	right_margin: 32,
	vsync_len: 3,
	upper_margin: 60,
	lower_margin: 32,
	active: 1,
	crt: 1
}, {
	name: "dmt640x400_85",
	pixclock: 31500000,
	xres: 640,		//832 = 640+64+96+32
	yres: 400,		//445 = 400+3+41+1
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 64,
	left_margin: 96,
	right_margin: 32,
	vsync_len: 3,
	upper_margin: 41,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt720x400_85",
	pixclock: 35500000,
	xres: 720,		//936 = 720+72+108+36
	yres: 400,		//446 = 400+3+42+1
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 72,
	left_margin: 108,
	right_margin: 36,
	vsync_len: 3,
	upper_margin: 42,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt640x480_60",
	pixclock: 25175000,
	xres: 640,		//800 = 640+96+48+16
	yres: 480,		//525 = 480+2+33+10
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 96,
	left_margin: 48,
	right_margin: 16,
	vsync_len: 2,
	upper_margin: 33,
	lower_margin: 10,
	active: 1,
	crt: 1
}, {
	name: "dmt640x480_72",
	pixclock: 31500000,
	xres: 640,		//832 = 640+40+128+24
	yres: 480,		//520 = 480+3+28+9
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 40,
	left_margin: 128,
	right_margin: 24,
	vsync_len: 3,
	upper_margin: 28,
	lower_margin: 9,
	active: 1,
	crt: 1
}, {
	name: "dmt640x480_75",
	pixclock: 31500000,
	xres: 640,		//840 = 640+64+120+16
	yres: 480,		//500 = 480+3+16+1
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 64,
	left_margin: 120,
	right_margin: 16,
	vsync_len: 3,
	upper_margin: 16,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt640x480_85",
	pixclock: 36000000,
	xres: 640,		//832 = 640+56+80+56
	yres: 480,		//509 = 480+3+25+1
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 56,
	left_margin: 80,
	right_margin: 56,
	vsync_len: 3,
	upper_margin: 25,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt800x600_56",
	pixclock: 36000000,
	xres: 800,		//1024 = 800+72+128+24
	yres: 600,		//625 = 600+2+22+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 72,
	left_margin: 128,
	right_margin: 24,
	vsync_len: 2,
	upper_margin: 22,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt800x600_60",
	pixclock: 40000000,
	xres: 800,		//1056 = 800+128+88+40
	yres: 600,		//628 = 600+4+23+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 128,
	left_margin: 88,
	right_margin: 40,
	vsync_len: 4,
	upper_margin: 23,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt800x600_72",
	pixclock: 50000000,
	xres: 800,		//1040 = 800+120+64+56
	yres: 600,		//666 = 600+6+23+37
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 120,
	left_margin: 64,
	right_margin: 56,
	vsync_len: 6,
	upper_margin: 23,
	lower_margin: 37,
	active: 1,
	crt: 1
}, {
	name: "dmt800x600_75",
	pixclock: 49500000,
	xres: 800,		//1056 = 800+80+160+16
	yres: 600,		//625 = 600+3+21+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 80,
	left_margin: 160,
	right_margin: 16,
	vsync_len: 3,
	upper_margin: 21,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt800x600_85",
	pixclock: 56250000,
	xres: 800,		//1048 = 800+64+152+32
	yres: 600,		//631 = 600+3+27+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 64,
	left_margin: 152,
	right_margin: 32,
	vsync_len: 3,
	upper_margin: 27,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt848x480_60",
	pixclock: 33750000,
	xres: 848,
	yres: 480,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 112,
	left_margin: 112,
	right_margin: 16,
	vsync_len: 8,
	upper_margin: 23,
	lower_margin: 6,
	active: 1,
	crt: 1
}, {
	name: "dmt1024x768_60",
	pixclock: 65000000,
	xres: 1024,		//1344 = 1024+136+160+24
	yres: 768,		//806 = 768+6+29+3
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 136,
	left_margin: 160,
	right_margin: 24,
	vsync_len: 6,
	upper_margin: 29,
	lower_margin: 3,
	active: 1,
	crt: 1
}, {
	name: "dmt1024x768_70",
	pixclock: 75000000,
	xres: 1024,		//1328 = 1024+136+144+24
	yres: 768,		//806 = 768+6+29+3
	pclk_redg: 1,
	hsyn_acth: 0,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 136,
	left_margin: 144,
	right_margin: 24,
	vsync_len: 6,
	upper_margin: 29,
	lower_margin: 3,
	active: 1,
	crt: 1
}, {
	name: "dmt1024x768_75",
	pixclock: 78750000,
	xres: 1024,		//1312 = 1024+96+176+16
	yres: 768,		//800 = 768+3+28+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 96,
	left_margin: 176,
	right_margin: 16,
	vsync_len: 3,
	upper_margin: 28,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1024x768_85",
	pixclock: 94500000,
	xres: 1024,		//1376 = 1024+96+208+48
	yres: 768,		//808 = 768+3+36+1
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 96,
	left_margin: 208,
	right_margin: 48,
	vsync_len: 3,
	upper_margin: 36,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1152x864_75",
	pixclock: 108000000,
	xres: 1152,
	yres: 864,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 128,
	left_margin: 256,
	right_margin: 64,
	vsync_len: 3,
	upper_margin: 32,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
#if 0
//The should be same as CVT reduced
	name: "dmt1280x768_60r",
	pixclock: 68250000, //ROUND_QM(1440*790*60)
	xres: 1280,		//1440 = 1280+32+80+48
	yres: 768,		//790 = 768+7+12+3
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 0,
	oepol_actl: 0,
	hsync_len: 32,
	left_margin: 80,
	right_margin: 48,
	vsync_len: 7,
	upper_margin: 12,
	lower_margin: 3,
	active: 1,
	crt: 1
//cvt compliant 1280x768@60 //1664=1280+128+192+64 //798=768+7+20+3
//lcdp "vesa:1280x768@60" or
//lcdp "v:79672320,1280,768,1,0,1,0,128,192,64,7,20,3,1,1"
}, {
#endif
	name: "dmt1280x960_60",
	pixclock: 108000000,
	xres: 1280,
	yres: 960,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 112,
	left_margin: 312,
	right_margin: 96,
	vsync_len: 3,
	upper_margin: 36,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1280x960_85",
	pixclock: 148500000,
	xres: 1280,
	yres: 960,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 160,
	left_margin: 224,
	right_margin: 64,
	vsync_len: 3,
	upper_margin: 47,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1280x1024_60",
	pixclock: 108000000,
	xres: 1280,
	yres: 1024,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 112,
	left_margin: 248,
	right_margin: 48,
	vsync_len: 3,
	upper_margin: 38,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1280x1024_75",
	pixclock: 135000000,
	xres: 1280,
	yres: 1024,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 144,
	left_margin: 248,
	right_margin: 16,
	vsync_len: 3,
	upper_margin: 38,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1280x1024_85",
	pixclock: 157500000,
	xres: 1280,
	yres: 1024,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 160,
	left_margin: 224,
	right_margin: 64,
	vsync_len: 3,
	upper_margin: 44,
	lower_margin: 1,
	active: 1,
	crt: 1
}, {
	name: "dmt1360x768_60",
	pixclock: 85500000,
	xres: 1360,
	yres: 768,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 112,
	left_margin: 256,
	right_margin: 64,
	vsync_len: 6,
	upper_margin: 18,
	lower_margin: 3,
	active: 1,
	crt: 1
}, {
	name: "dmt1600x1200_60",
	pixclock: 162000000,
	xres: 1600,
	yres: 1200,
	pclk_redg: 1,
	hsyn_acth: 1,
	vsyn_acth: 1,
	oepol_actl: 0,
	hsync_len: 192,
	left_margin: 304,
	right_margin: 64,
	vsync_len: 3,
	upper_margin: 46,
	lower_margin: 1,
	active: 1,
	crt: 1
}
};
#define num_lcd_panels (sizeof(lcd_panels_)/sizeof(lcd_panels_[0]))

/*
 ******************************************************************************
 */
# define do_divq(n,base) ({				\
	uint64_t tn = (n);				\
	uint32_t tbase = (base);			\
	uint32_t trem;					\
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));	\
	if (((n) >> 32) == 0) {				\
		tn = (uint32_t)(tn) / tbase;		\
	} else						\
		trem = do_div((tn), tbase);		\
	tn;						\
 })

#define OSC_RATE 27000000

#if 1
#define DDR2_MIN	(140000000 * 2)	/* after div2 */
#define	DDR2_MAX	(200000000 * 2)
#else
#define DDR2_MIN	(161000000 * 2)	/* after div2 */
#define	DDR2_MAX	(163000000 * 2)
#endif

//#define MAX_VPBE	75018754
//75 Mhz Max clock input to back end (13.33ns/clock)
#define MAX_VPBE	112000000	//112 Mhz Max clock input to back end

static int check_ddr2(unsigned mrate)
{
	unsigned rate;
	unsigned div = ((mrate - 1) / DDR2_MAX) + 1;
	if (div > 16)
		return 0;
	rate = mrate /div;
//	printf("%s: rate:%u div:%u\n", __func__, rate, div);
	return (rate < DDR2_MIN) ? 0 : div;
}

struct clk_factors {
	u32 mult;
	u32 div;
	u32 enc_mult;
	u32 enc_div;
	u32 error;
};

static int relatively_prime(u32 a, u32 b)
{
	u32 c;
	if (a < b) {
		c = a;
		a = b;
		b = c;
	}
	do {
		if (b == 1)
			return 1;
		c = a % b;
		if (c == 0)
			break;
		a = b;
		b = c;
	} while (1);
	return 0;
}

int new_best(struct clk_factors *best, struct clk_factors *test)
{
	if (best->error > test->error) {
		if (best->enc_div != 1)
			return 1;
		if (test->enc_div == 1)
			return 1;
		if (((best->error * 3)/4) > test->error)
			return 1;
		/*
		 * Best error is larger, but not more than 33% larger
		 * and Best has advantage of a normal pixel clock
		 */
		return 0;
	}
	if (best->error < test->error) {
		if (test->enc_div != 1)
			return 0;
		if (best->enc_div == 1)
			return 0;
		if (((test->error * 3)/4) > best->error)
			return 0;
		/*
		 * Test error is larger, but not more than 33% larger
		 * and Test has advantage of a normal pixel clock
		 */
		return 1;
	}
	if (best->enc_div != test->enc_div) {
		if (best->enc_div == 1)
			return 0;
		if (test->enc_div == 1)
			return 1;
		/* This is not needed */
		if (0) if ((test->enc_div > best->div) &&
				relatively_prime(test->enc_div,test->enc_mult)) {
			return 1;
		}
	}
	return 0;
}
//VENC_DCLKCTL encodes 1-64 clks, but VENC_OSDCLK0 encodes only 1-16 clks,
#define MAX_ENC_DIV 16

#define MAX_ENC_MULT 1	//was MAX_ENC_DIV - 1, but doesn't look good on CRTS
static unsigned query_clk_settings(u32 mhz, struct clk_factors *pbest,
	unsigned char encperpix_m, unsigned char encperpix_d)
{
	struct clk_factors best;
	struct clk_factors cur;
	u32 mrate;
	u32 enc_mult_start = 1;
	u32 enc_mult_end = MAX_ENC_MULT;
	u32 enc_div_start = 1;
	u32 enc_div_end = MAX_ENC_DIV;
	best.error = ~0;
	best.mult = 0;
	best.div = 0;
	best.enc_mult = 0;
	best.enc_div = 0;

	if (!encperpix_m)
		encperpix_m = 1;
	if (encperpix_d) {
		if (encperpix_d > 64) {
			printk(KERN_ERR "Invalid encperpix(%u:%u)\n",
					encperpix_m, encperpix_d);
		} else {
			printk(KERN_ERR "forcing encperpix=%u:%u\n",
					encperpix_m, encperpix_d);
			enc_mult_start = encperpix_m;
			enc_mult_end = encperpix_m;
			enc_div_start = encperpix_d;
			enc_div_end = encperpix_d;
		}
	}

	cur.mult = (DDR2_MIN + OSC_RATE -1) / OSC_RATE;
	mrate = cur.mult * OSC_RATE;
	for (; cur.mult <= 32; cur.mult++, mrate += OSC_RATE) {
		if (!check_ddr2(mrate))
			continue;
		cur.div = (mrate + MAX_VPBE - 1)/MAX_VPBE;
		for (; cur.div <= 16; cur.div++) {
			u32 vpbe = mrate / cur.div;
			cur.enc_mult = enc_mult_start;
//			printf("vpbe=%u, mhz=%u\n", vpbe, mhz);
			for (; cur.enc_mult <= enc_mult_end; cur.enc_mult++) {
				u32 rate;
				u64 vm = vpbe;
				vm *= cur.enc_mult;
				cur.enc_div = do_divq(vm, mhz);
				if (cur.enc_div > enc_div_end)
					cur.enc_div = enc_div_end;
				if (cur.enc_div < enc_div_start)
					cur.enc_div = enc_div_start;
				if (cur.enc_div <= cur.enc_mult) {
					cur.enc_div = cur.enc_mult;
					if (cur.enc_mult != enc_mult_start) {
						cur.enc_div++;
						if (cur.enc_div > enc_div_end)
							break;
					}
				}
				do {
					rate = do_divq(vm, cur.enc_div);
					cur.error = (rate > mhz) ?
						(rate - mhz) : (mhz - rate);
if (0) printk(KERN_ERR "mult:%u div:%u enc:%u:%u error:%u\n",
	cur.mult, cur.div, cur.enc_mult, cur.enc_div, cur.error);
					if (new_best(&best, &cur)) {
if (0) printk(KERN_ERR "%s: mult:%u div:%u enc:%u:%u error:%u\n", __func__,
	cur.mult, cur.div, cur.enc_mult, cur.enc_div, cur.error);
						best = cur;
					}
					cur.enc_div++;
				} while ((rate > mhz) &&
						(cur.enc_div <= enc_div_end));
				if (vpbe <= mhz)
					break;
			}
			if (vpbe <= mhz)
				break;
		}
	}
	*pbest = best;
	mrate = 0;
	if (best.mult) {
		u64 m = OSC_RATE;
		m *= (best.mult * best.enc_mult);
		mrate = do_divq(m, (best.div * best.enc_div));
	}
	return mrate;
}

u32 query_pixel_Clock(u32 mhz,
	unsigned char encperpix_m, unsigned char encperpix_d)
{
	struct clk_factors clk;
	return query_clk_settings(mhz, &clk, encperpix_m, encperpix_d);
}

/*
 ******************************************************************************
 */
#define FB_CVT_CELLSIZE			  8
#define FB_CVT_GTF_C			 40
#define FB_CVT_GTF_J			 20
#define FB_CVT_GTF_K			128
#define FB_CVT_GTF_M			600
#define FB_CVT_MIN_VSYNC_BP		550
#define FB_CVT_MIN_VPORCH		  3


#define FB_CVT_RB_MIN_VBLANK		460
#define FB_CVT_RB_HBLANK		160

/* returns ideal duty cycle * 1000 */
static u32 fb_cvt_ideal_duty_cycle(u32 h_period_est)
{
	u32 c_prime = (FB_CVT_GTF_C - FB_CVT_GTF_J) *
		(FB_CVT_GTF_K) + 256 * FB_CVT_GTF_J;
	u32 m_prime = (FB_CVT_GTF_K * FB_CVT_GTF_M);

	return (1000 * c_prime  - ((m_prime * h_period_est)/1000))/256;
}
/*
 ******************************************************************************
 */


#define CONFIG_GP_HSYNC 14
#define CONFIG_GP_VSYNC 11

#define HMASK (1 << (CONFIG_GP_HSYNC & 0x1f))
#define VMASK (1 << (CONFIG_GP_VSYNC & 0x1f))

#define GP_BINTEN 0x08
#define GP_BANK0_OFFSET	0x10
#define GP_BANK_LENGTH	0x28
#define GP_DIR			0x00
#define GP_OUT			0x04
#define GP_SET			0x08
#define GP_CLR			0x0C
#define GP_IN			0x10
#define GP_SET_RISING_EDGE	0x14
#define GP_CLR_RISING_EDGE	0x18
#define GP_SET_FALLING_EDGE	0x1c
#define GP_CLR_FALLING_EDGE	0x20
#define GP_INT_STAT		0x24

#define DAVINCI_GPIO_BASE 0x01C67000


#define DIR_IN 1
#define DIR_OUT 0

static volatile u32 *gpio_get_in_ptr(u32 gpbase, u32 gp)
{
	u32 bank = (gp >> 5);
	volatile u32 *p = (u32 *)(gpbase + GP_BANK0_OFFSET + GP_IN +
			(bank * GP_BANK_LENGTH));
	return p;
}

static volatile u32 *gpio_get_stat_ptr(u32 gpbase, unsigned gp, int edge)
{
	u32 mask = (1 << (gp & 0x1f));
	u32 bank = (gp >> 5);
	volatile u32 *p = (u32 *)(gpbase + GP_BINTEN);
	*p |= 1 << (bank >> 1);	/* enable proper bank */

	p = (u32 *)(gpbase + GP_BANK0_OFFSET + (bank * GP_BANK_LENGTH));
	if (edge & 1)
		p[GP_SET_RISING_EDGE >> 2] = mask;
	else
		p[GP_CLR_RISING_EDGE >> 2] = mask;
	if (edge & 2)
		p[GP_SET_FALLING_EDGE >> 2] = mask;
	else
		p[GP_CLR_FALLING_EDGE >> 2] = mask;
	return &p[GP_INT_STAT >> 2];
}


struct v_aspect_ratio {
	unsigned char vsync_len;
	unsigned char num;
	unsigned char den;
	unsigned char yres_div256;
};
static const struct v_aspect_ratio cvt_vsync_len_to_aspect_ratio[] = {
	{4, 4, 3, 0},		/* 1.333 : 1 */
	{5, 16, 9, 0},		/* 1.777 : 1 */
	{6, 8, 5, 0},		/* 1.6   : 1 */
	{7, 5, 4, 1024/256},	/* 1.25  : 1 */
	{7, 5, 3, 0},		/* 1.666 : 1, 768/256 */
	{0, 0, 0, 0}
};

int get_xres_guess(int yres)
{
	int xres;
	if ((yres % 83) == 0)
		xres = (yres * 147) / 83;
	else if ((yres % 27) == 0)
		xres = (yres * 4) / 3;
	else if ((yres % 9) == 0)
		xres = (yres * 16) / 9;
	else if ((yres % 5) == 0)
		xres = (yres * 8) / 5;
	else if ((yres % 128) == 0)
		xres = (yres * 25) / 16;	/* still a multiple of 8 */
	else
		xres = (yres * 4) / 3;
	return (xres + 4) & ~7;			/* round to multiple of 8 */
}
int get_xres(int yres, int vsync_len)
{
	int xres = -1;
	const struct v_aspect_ratio *p = cvt_vsync_len_to_aspect_ratio;
	while (p->vsync_len) {
		if (vsync_len == p->vsync_len) {
			if ((p->yres_div256 == 0) ||
					((yres >> 8) == p->yres_div256)) {
				xres = (yres * p->num + (p->den << 2)) /
					(p->den << 3);
				return xres << 3;
			}
		}
		p++;
	}
	return get_xres_guess(yres);
}
#define LOG_TRANSITION_PAIRS 2
#define MAX_TRANSITIONS (1 + (2<<LOG_TRANSITION_PAIRS))
#define BASE_TRANSITION 0
struct measurements {
	volatile u32 *phstat;
	volatile u32 *phsync;
	volatile u32 *pvsync;
	struct timespec	start_time;
	struct timespec	end_time[MAX_TRANSITIONS];
	wait_queue_head_t sample_waitq;
	int		bReady;
//	int hsync_cur_inactive;
	int hcnt[MAX_TRANSITIONS];
	int index;	/* increments mod 8, every vsync level change */
	int inc_pending;
	u32 hsync_active;
	u32 vsync;
#ifdef MEASURE_HSYNC_DUTY_CYCLE
	u32 frac;
#endif
};

static void vsync_advance(struct measurements *m)
{
	int index = m->index;
	getnstimeofday(&m->end_time[index]);
	index++;
	if (index < MAX_TRANSITIONS) {
		m->index = index;
		m->hcnt[index] = 0;
	} else {
		if (!m->bReady) {
			m->bReady=1;
			m->vsync = *m->pvsync & VMASK;
		}
		wmb();
		wake_up(&m->sample_waitq);
	}
}
static irqreturn_t hsync_interrupt(int irq, void *id)
{
	struct measurements *m = id;
//	if ((*m->phsync ^ m->hsync_active) & HMASK) {
		/*
		 * hsync is inactive now,
		 * only count active to inactive transitions
		 */
//		m->hsync_cur_inactive = 1;
		if (!m->bReady) {
			m->hcnt[m->index]++;
//			*m->phstat = HMASK;
		}
		if (m->inc_pending) {
			m->inc_pending = 0;
			vsync_advance(m);
		}
//	} else {
//		m->hsync_cur_inactive = 0;
//	}
	return IRQ_HANDLED;
}
static irqreturn_t vsync_interrupt(int irq, void *id)
{
	struct measurements *m = id;
	if (
//			m->hsync_cur_inactive &&
			((*m->phstat & HMASK) == 0) &&
			((*m->phsync ^ m->hsync_active) & HMASK) ) {
		/*
		 * hsync is inactive now and it's interrupt is NOT pending,
		 * only increment index when inactive
		 */
		vsync_advance(m);
	} else
		m->inc_pending = 1;
	return IRQ_HANDLED;
}

/* #define MEASURE_HSYNC_DUTY_CYCLE */
/* no need to do this, use 8% rounded down */
#ifdef MEASURE_HSYNC_DUTY_CYCLE
int count_while_high(volatile u32 *phsync, struct timespec* start_time)
{
	u32 check = 0x10000;
	u32 high = 0;
	do {
		if (check==0) {
			ulong elapsed = get_timer_delta(start_time);
			if (elapsed > HALF_A_SECOND)
				return -1;
			check = 0x10000;
		}
		check--;
		if (!(*phsync & HMASK))
			break;
		high++;
		if (!(*phsync & HMASK))
			break;
		high++;
		if (!(*phsync & HMASK))
			break;
		high++;
		if (!(*phsync & HMASK))
			break;
		high++;
	} while (1);
	return high;
}
int count_while_low(volatile u32 *phsync, struct timespec* start_time)
{
	u32 check = 0x10000;
	u32 low = 0;
	do {
		if (check==0) {
			ulong elapsed = get_timer_delta(start_time);
			if (elapsed > HALF_A_SECOND)
				return -1;
			check = 0x10000;
		}
		check--;
		if (*phsync & HMASK)
			break;
		low++;
		if (*phsync & HMASK)
			break;
		low++;
		if (*phsync & HMASK)
			break;
		low++;
		if (*phsync & HMASK)
			break;
		low++;
	} while (1);
	return low;
}
u32 measure_hsync_duty_cycle(struct measurements *m)
{
	u32 frac;
	u32 hsync_low_cnt = 1;
	u32 hsync_high_cnt = 1;
	u32 check = 0x10000;
	volatile u32 *phsync = m->phsync;
	u32 hsync_active = m->hsync_active;
	if (hsync_active) {
		count_while_low(phsync, &m->start_time);
		count_while_high(phsync, &m->start_time);
	} else {
		count_while_high(phsync, &m->start_time);
		count_while_low(phsync, &m->start_time);
	}
	/* hsync just went inactive */

	/* count for 1/4 second */
	do {
		if (check==0) {
			ulong elapsed = get_timer_delta(&m->start_time);
			check = 0x10000;
			/* sample for 1/4 second */
			if (elapsed > FOURTH_OF_A_SECOND)
				break;
		}
		check--;
		if (*phsync & HMASK)
			hsync_high_cnt++;
		else
			hsync_low_cnt++;
		if (*phsync & HMASK)
			hsync_high_cnt++;
		else
			hsync_low_cnt++;
		if (*phsync & HMASK)
			hsync_high_cnt++;
		else
			hsync_low_cnt++;
		if (*phsync & HMASK)
			hsync_high_cnt++;
		else
			hsync_low_cnt++;
	} while (1);

	/* count till hsync inactive edge */
	if (hsync_active) {
		hsync_low_cnt += count_while_low(phsync, &m->start_time);
		hsync_high_cnt += count_while_high(phsync, &m->start_time);
	} else {
		hsync_high_cnt += count_while_high(phsync, &m->start_time);
		hsync_low_cnt += count_while_low(phsync, &m->start_time);
	}
	{
		u32 duty;
		u32 duty_int;
		u64 t = (hsync_active) ? hsync_high_cnt : hsync_low_cnt;
		int power2 = 0;
		u32 total = hsync_high_cnt + hsync_low_cnt;
		while ((t & (1ULL << 63)) == 0) {
			power2++;
			t <<= 1;
		}
		t = t / total;
		if (power2 > 32) {
			t >>= (power2 - 32);
			power2 = 32;
		}
		frac = (u32)t;
		t *= 100;
		duty_int = (u32)(t >> power2);
		t &= ((1ULL << power2) - 1);
		t *= 10000;
		duty = (u32)(t >> power2);
		printk(KERN_ERR "hsync_high_cnt=%i hsync_low_cnt=%i "
				"duty=%i.%04i%% frac=%08x\n",
				hsync_high_cnt, hsync_low_cnt,
				duty_int, duty, frac);
	}
	return frac;
}
#endif

int get_hsync_polarity(struct measurements *m)
{
	u32 vsync;
	u32 check = 0x10000;
	u32 hsync_low_cnt = 0;
	u32 hsync_high_cnt = 0;
	u32 vsync_cnt = 0;
	volatile u32 *phsync = m->phsync;
	volatile u32 *pvsync = m->pvsync;

	/* calc HSYNC duty cycle */

	/* count hsync until vsync changes twice */
	vsync = *pvsync & VMASK;
	do {
		if (check==0) {
			ulong elapsed = get_timer_delta(&m->start_time);
			if (elapsed > FOURTH_OF_A_SECOND)
				break;
			check = 0x10000;
		}
		check--;
		if (*phsync & HMASK)
			hsync_high_cnt++;
		else
			hsync_low_cnt++;
		if ((*pvsync ^ vsync) & VMASK) {
			vsync ^= VMASK;
			vsync_cnt++;
			if (vsync_cnt >= 2)
				break;
		}
	} while (1);
	if (0) printk(KERN_ERR "hsync_high_cnt=%i hsync_low_cnt=%i\n",
			hsync_high_cnt, hsync_low_cnt);
	if ((hsync_high_cnt == 0) || (hsync_low_cnt == 0))
		return -1;
	return (hsync_high_cnt < hsync_low_cnt) ? 1 : 0;
}

static inline u32 cvt_len(u32 len, u32 real_clk, u32 ideal_clk)
{
#define FRAC_BITS 1
	u64 tmp = len << FRAC_BITS;
	tmp *= real_clk;
	tmp /= ideal_clk;
	tmp += 1 << (FRAC_BITS-1);
	tmp >>= FRAC_BITS;
	return (u32)tmp;
}

static u32 calc_pixclock(u32 htotal, u32 hperiod)
{
/*
 * ((htotal pixels/line) / (hperiod ns/line)) * (1000000000 ns/sec) = pixels/sec
 */
	u64 tmp;
	tmp = htotal;
	tmp *= 1000000000;
	tmp += hperiod >> 1;	/* round */
	return do_divq(tmp, hperiod);
}

int calc_gtf_settings(struct lcd_panel_info_t *panel, u32 v_total, u32 hperiod)
{
	u64 tmp;
	u32 ideal_duty_cycle;	//div 10**8 = frac
	u32 hsync;
	u32 hblank;
	u32 htotal;
	panel->lower_margin = 1;
/* min time of vsync + back porch (nanosec) */
#define MIN_VSYNC_PLUS_BP 550000

/* C' and M' are part of the Blanking Duty Cycle computation */

#define C_PRIME           30
#define M_PRIME           300
	panel->name = "calc_gtf";
	panel->upper_margin = ((MIN_VSYNC_PLUS_BP + (hperiod >> 1)) / hperiod)
		- panel->vsync_len;
	panel->yres = v_total - panel->lower_margin - panel->upper_margin
		- panel->vsync_len;

// divided by 10**8 gives fraction
	ideal_duty_cycle = (C_PRIME * 1000000) - ((M_PRIME * hperiod));
	panel->xres = get_xres_guess(panel->yres);

	tmp = ideal_duty_cycle;
	tmp *= panel->xres;
	tmp += (100000000 - ideal_duty_cycle) << 3;	/* round */
	hblank = do_divq(tmp, (100000000 - ideal_duty_cycle) << 4);
	hblank <<= 4;

	htotal =  panel->xres + hblank;
	hsync = (htotal * 2 + (25*4)) / (25*8);	//8%
	hsync <<= 3;

	panel->left_margin = hblank >> 1;
	panel->right_margin = (hblank >> 1) - hsync;
	panel->hsync_len = hsync;
	panel->pixclock = calc_pixclock(htotal, hperiod);
	return 0;
}

static int scan_for_dmt_entry(struct lcd_panel_info_t *panel, u32 v_total,
		u32 hperiod)
{
	const struct lcd_panel_info_t *p = lcd_panels_;
	const struct lcd_panel_info_t *best = NULL;
	u32 best_error = ~0;
	int i;
	u32 htotal;
	for (i = 0; i < num_lcd_panels; i++, p++ ) {
		u32 htot, vtot;
		u32 cur_hperiod;
		u32 error;
		u64 tmp;
		if (p->name[0] != 'd') continue;
		if (p->name[1] != 'm') continue;
		if (p->name[2] != 't') continue;
		if (p->hsyn_acth != panel->hsyn_acth) continue;
		if (p->vsyn_acth != panel->vsyn_acth) continue;
		if (p->vsync_len != panel->vsync_len) continue;
		vtot = p->upper_margin + p->yres + p->lower_margin
			+ p->vsync_len;
		if (vtot != v_total) continue;
		htot = p->left_margin + p->xres + p->right_margin
			+ p->hsync_len;
		tmp = htot;
		tmp *= 1000000000;
		/* nanoseconds/line */
		cur_hperiod = do_divq(tmp, p->pixclock);
		error = (cur_hperiod >= hperiod) ? (cur_hperiod - hperiod) :
				(hperiod - cur_hperiod);
		if (best_error > error) {
			best = p;
			best_error = error;
		}
	}
	if (!best) {
		return -1;
	}
	panel->name = best->name;
	panel->pclk_redg = best->pclk_redg;
	panel->oepol_actl = best->oepol_actl;
	panel->active = best->active;
	panel->crt = best->crt;

	panel->xres = best->xres;
	panel->hsync_len = best->hsync_len;
	panel->left_margin = best->left_margin;
	panel->right_margin = best->right_margin;

	panel->yres = best->yres;
	panel->upper_margin = best->upper_margin;
	panel->lower_margin = best->lower_margin;
	htotal = panel->xres + panel->hsync_len +
			panel->left_margin + panel->right_margin;
	panel->pixclock = calc_pixclock(htotal, hperiod);
	if (0) printk(KERN_ERR "found %s, error=%i ns, hperiod=%i ns\n",
			best->name, best_error, hperiod);
	return 0;
}

void calc_cvt_settings(struct lcd_panel_info_t *panel, u32 v_total, u32 hperiod)
{
	u32 v_period = hperiod * v_total;
	u32 reduced = 0;
	panel->lower_margin = 3;
	if (panel->hsyn_acth && !panel->vsyn_acth) {
		reduced = 1;
		v_period -= FB_CVT_RB_MIN_VBLANK * 1000;
		panel->yres = v_period / hperiod;
		panel->name = "calc_cvtr";
	} else {
		v_period -= FB_CVT_MIN_VSYNC_BP * 1000;
		panel->yres = v_period / hperiod;
		panel->yres -= FB_CVT_MIN_VPORCH;
		panel->name = "calc_cvt";
	}

	panel->upper_margin = v_total - panel->vsync_len - panel->lower_margin
		- panel->yres;
	panel->xres = get_xres(panel->yres, panel->vsync_len);
	{
		u32 htotal;
		u32 hblank  = FB_CVT_RB_HBLANK;
		panel->hsync_len = 32;
		if (!reduced) {
			u32 ideal_duty_cycle = fb_cvt_ideal_duty_cycle(hperiod);
			u32 active_pixels = panel->xres;

			if (ideal_duty_cycle < 20000)
				ideal_duty_cycle = 20000;
			hblank = (active_pixels * ideal_duty_cycle)/
				(100000 - ideal_duty_cycle);
			hblank &= ~((2 * FB_CVT_CELLSIZE) - 1);
		}
		htotal =  panel->xres + hblank;
		panel->pixclock = calc_pixclock(htotal, hperiod);
		panel->left_margin = hblank >> 1;
		if (!reduced) {
			u32 t;
			u64 tmp = hperiod;
			tmp *= 2;	/* (2/25) 8% width */
			/* nanosecond width of sync pulse */
			tmp = do_divq(tmp, 25);
			tmp *= panel->pixclock;
			t = do_divq(tmp, 1000000000);	/* # of clocks */
			t &= ~(FB_CVT_CELLSIZE - 1);
			panel->hsync_len = t;
		}
		panel->right_margin = hblank - panel->left_margin
			- panel->hsync_len;

//panel now has CVT settings
	}
#define ROUND_QM(a) (((a)+125000)/250000)*250000
	panel->pixclock = ROUND_QM(panel->pixclock);
}

static int get_hsync_level(struct measurements *m)
{
	int ret = get_hsync_polarity(m);
	if (ret < 0)
		return ret;
	m->hsync_active = (ret) ? HMASK : 0;

	getnstimeofday(&m->start_time);
	#ifdef MEASURE_HSYNC_DUTY_CYCLE
	m->frac = measure_hsync_duty_cycle(m);
	#endif
	return ret;
}

#if 0
static int measure_work(struct measurements *m, int gpbase)
{
	int ret = 0;
	int i;
try_again:
	m->index = 0;
	m->hcnt[0] = 0;
	m->inc_pending = 0;
	m->bReady = 0;
	gpio_get_stat_ptr(gpbase, CONFIG_GP_VSYNC, 3);
	gpio_get_stat_ptr(gpbase, CONFIG_GP_HSYNC, 3);
	while (!m->bReady) {
		ret = wait_event_interruptible_timeout(m->sample_waitq,
				m->bReady, HZ);
		if (ret == 0)
			ret = -EINVAL;
		if (ret < 0)
			return ret;
	}
	for (i = 3; i < MAX_TRANSITIONS; i++) {
		if (m->hcnt[i-2] != m->hcnt[i]) {
			for (i = MAX_TRANSITIONS & 1; i < MAX_TRANSITIONS;
					i += 2)
				printk(KERN_ERR "%i %i\n",
						m->hcnt[i], m->hcnt[i+1]);
			goto try_again;
		}
	}
	return ret;
}
#else
static int measure_work(struct measurements *m, int gpbase)
{
	spinlock_t	lock;
	unsigned long	flags;
	volatile u32 *phstat = m->phstat;
	volatile u32 *phsync = m->phsync;
	volatile u32 *pvsync = m->pvsync;
	unsigned vsync;
	int ret = 0;
	int i;
	u32 check;
try_again:
	check = 0x100000;
	spin_lock_init(&lock);
	spin_lock_irqsave(&lock, flags);
	m->index = 0;
	m->hcnt[0] = 0;
	m->inc_pending = 0;
	m->bReady = 0;
	gpio_get_stat_ptr(gpbase, CONFIG_GP_HSYNC, 3);
	vsync = *pvsync & VMASK;
	while (!m->bReady) {
		if (vsync != (*pvsync & VMASK)) {
			vsync_interrupt(0, m);
			vsync ^= VMASK;
		}
		if (*phstat & HMASK) {
			if ((*phsync ^ m->hsync_active) & HMASK) {
				*phstat = HMASK;
				hsync_interrupt(0, m);
			}
		}
		if (check==0) {
			ulong elapsed = get_timer_delta(&m->start_time);
			if (elapsed > FOURTH_OF_A_SECOND) {
				ret = -EINVAL;
				break;
			}
			check = 0x100000;
		}
		check--;
	}
	spin_unlock_irqrestore(&lock, flags);
	if (ret < 0)
		return ret;

	for (i = 3; i < MAX_TRANSITIONS; i++) {
		if (m->hcnt[i-2] != m->hcnt[i]) {
			for (i = MAX_TRANSITIONS & 1; i < MAX_TRANSITIONS;
					i += 2)
				printk(KERN_ERR "%i %i\n",
						m->hcnt[i], m->hcnt[i+1]);
			goto try_again;
		}
	}
	return ret;
}
#endif

static int calc_work(struct vpbe_panel_from_hsync *vpbe_panel,
		struct measurements *m)
{
	u32 shift;
	u32 ideal_clk;
	u32 real_clk;
	u32 v_total;
	u32 hperiod;
	int hsync_cnt1, hsync_cnt2;
	ulong elapsed_ns;
	struct lcd_panel_info_t *panel = &vpbe_panel->panel;

	hsync_cnt1 = m->hcnt[MAX_TRANSITIONS - 2];
	hsync_cnt2 = m->hcnt[MAX_TRANSITIONS - 1];
	elapsed_ns = m->end_time[MAX_TRANSITIONS - 1].tv_sec -
		m->end_time[BASE_TRANSITION].tv_sec;
	elapsed_ns *= NSEC_PER_SEC;
	elapsed_ns += m->end_time[MAX_TRANSITIONS - 1].tv_nsec -
		m->end_time[BASE_TRANSITION].tv_nsec;
	elapsed_ns >>= LOG_TRANSITION_PAIRS;
	if (hsync_cnt1 > hsync_cnt2) {
		int tmp = hsync_cnt1;
		hsync_cnt1 = hsync_cnt2;
		hsync_cnt2 = tmp;
		m->vsync ^= VMASK;
	}
	if (hsync_cnt1==0)
		return -EINVAL;

	if (0) printk(KERN_ERR "hsync_cnt1=%i hsync_cnt2=%i\n",
			hsync_cnt1, hsync_cnt2);
	v_total = hsync_cnt1 + hsync_cnt2;
	panel->pclk_redg = 1;
	panel->oepol_actl = 0;
	panel->active = 1 ;
	panel->rotation = 0 ;
	panel->crt = 1;
	panel->hsyn_acth = (m->hsync_active) ? 1 : 0;
	panel->vsyn_acth = (m->vsync) ? 1 : 0;
	panel->vsync_len =  hsync_cnt1;

	hperiod = (elapsed_ns + (v_total >> 1)) / v_total;

	if (0)
		printk(KERN_ERR "looking for hsync=%u, vsync=%u, vsync_len=%u,"
				" v_total=%u, hperiod=%u ns\n",
				panel->hsyn_acth, panel->vsyn_acth,
				panel->vsync_len, v_total, hperiod);
//Check if DMT timings should be used
	do {
		if (((panel->hsyn_acth ^ panel->vsyn_acth) == 0) ||
				(panel->vsync_len < 4)) {
			if (scan_for_dmt_entry(panel, v_total, hperiod)==0)
				break;
			if ((panel->hsyn_acth == 0) && (panel->vsyn_acth == 1)
					&& (panel->vsync_len == 3))
				if (!calc_gtf_settings(panel, v_total, hperiod))
					break;
			printk(KERN_ERR "*** warning!!! **** weird panel,"
					" hsync=%u, vsync=%u, vsync_len=%u,"
					" v_total=%u, hperiod=%u ns\n",
					panel->hsyn_acth, panel->vsyn_acth,
					panel->vsync_len, v_total, hperiod);
		}
		calc_cvt_settings(panel, v_total, hperiod);
	} while (0);

	ideal_clk = panel->pixclock;
	shift = 0;
	while (1) {
		real_clk = query_pixel_Clock(ideal_clk, vpbe_panel->encperpix_m,
				vpbe_panel->encperpix_d);
		if (real_clk == 0)
			return -1;
		if (0) printk(KERN_ERR "ideal_clk=%u, real_clk=%u\n",
				ideal_clk, real_clk);
		if ((ideal_clk > 112000000) &&
				(ideal_clk > ((real_clk * 9) >> 3))) {
			/*
			 * ideal_clk is 12.5% more than real clock,
			 * scale down
			 */
			ideal_clk >>= 1;
			shift++;
			continue;
		}
		break;
	}

	panel->pixclock = ideal_clk;
	if (shift) {
		/*
		 * all settings start as multiples of 8,
		 * so just zeros are shifted out
		 */
		panel->xres >>= shift;
		panel->hsync_len >>= shift;
		panel->left_margin >>= shift;
		panel->right_margin >>= shift;
	}
#if 0
	{
		u32 htotal = panel->xres + panel->hsync_len +
			panel->left_margin + panel->right_margin;
		u32 new_htotal = cvt_len(htotal, real_clk, ideal_clk);
		u32 new_xres = cvt_len(panel->xres, real_clk, ideal_clk);
		panel->xres = new_xres & ~7;	/* Keep multiple of 8*/
		panel->hsync_len = cvt_len(panel->hsync_len, real_clk,
				ideal_clk);
		panel->left_margin = cvt_len(panel->left_margin, real_clk,
				ideal_clk);
		panel->left_margin += (new_xres & 7) >> 1;
		panel->right_margin = new_htotal -
			(panel->xres + panel->hsync_len + panel->left_margin);
		panel->pixclock = calc_pixclock(new_htotal, hperiod);
	}
#endif

#ifdef MEASURE_HSYNC_DUTY_CYCLE
	{
		u32 t;
		u64 tmp = hperiod;
		tmp *= frac;
		tmp >>= 32;	/* nanosecond width of sync pulse */
		tmp *= panel->pixclock;
		tmp /= 500000000;	/* # of clocks */
		t = (u32)((tmp + 1) >> 1);
//		t += FB_CVT_CELLSIZE >> 1;
//		t &= ~(FB_CVT_CELLSIZE - 1);
		printk(KERN_ERR "measured val=%i, using =%i\n",
				t, panel->hsync_len);
	}
#endif
	return real_clk;
}


int calc_settings_from_hsync_vsync(struct vpbe_panel_from_hsync *vpbe_panel)
{
	int ret;
	u32 gpbase;
	struct measurements m;
	int irqh = IRQ_GPIO(CONFIG_GP_HSYNC);
	int irqv = IRQ_GPIO(CONFIG_GP_VSYNC);
	memset(&m, 0, sizeof(m));
	init_waitqueue_head(&m.sample_waitq);
	gpbase = (u32)ioremap(DAVINCI_GPIO_BASE, 4096);
	if (!gpbase) {
		printk(KERN_ERR "%s: ioremap failed\n", __func__);
		return -EINVAL;
	}
	m.phsync = gpio_get_in_ptr(gpbase, CONFIG_GP_HSYNC);
	m.pvsync = gpio_get_in_ptr(gpbase, CONFIG_GP_VSYNC);
	m.phstat = gpio_get_stat_ptr(gpbase, CONFIG_GP_HSYNC, 0);
	ret = get_hsync_level(&m);
	if (ret < 0)
		return ret;
	set_irq_type(irqh, (m.hsync_active) ? IRQ_TYPE_EDGE_FALLING :
		IRQ_TYPE_EDGE_RISING);
	set_irq_type(irqv, IRQ_TYPE_EDGE_BOTH);
	ret = request_irq(irqh, &hsync_interrupt, 0, "hsync", &m);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i gp:%i\n",
				"hsync", irqh, CONFIG_GP_HSYNC);
		goto out2;
	}
	ret = request_irq(irqv, &vsync_interrupt, 0, "vsync", &m);
	if (ret) {
		printk(KERN_ERR "%s: request_irq failed, irq:%i gp:%i\n",
				"vsync", irqh, CONFIG_GP_VSYNC);
		goto out3;
	}
	gpio_get_stat_ptr(gpbase, CONFIG_GP_HSYNC, 0);
	gpio_get_stat_ptr(gpbase, CONFIG_GP_VSYNC, 0);
	/*
	 * this enables/disables rising/falling edge interrupts
	 * and returns a pointer to the interrupt status register
	 */
	ret = measure_work(&m, gpbase);
	free_irq(irqh, &m);
out3:
	free_irq(irqv, &m);
out2:
	iounmap((void *)gpbase);
	if (ret >= 0) {
		ret = calc_work(vpbe_panel, &m);
		vpbe_panel->real_clock = ret;
	}
	return ret;
}
