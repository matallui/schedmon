#include <linux/module.h>
#include <linux/kernel.h>

//#define SMON_DEBUG
#include "smon.h"

/*
 * MSR Addresses
 */
#undef  MSR_PLATFORM_INFO
#define MSR_PLATFORM_INFO	0xCE	/* read bits 15:8 to get frequency = ratio * 100 MHz */


inline long long msr_read ( int num )
{
	unsigned int hi = 0;
	unsigned int lo = 0;

	PDEBUG("msr_read: num = 0x%.8x\n", num);
	__asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(num));

	return lo | (long long)hi << 32;
}

inline void msr_write ( int num, long long val )
{
	unsigned int hi = val >> 32;
	unsigned int lo = val & 0x00000000FFFFFFFF;

	PDEBUG("msr_write: num = 0x%.8x, val = 0x%.16llx (hi = 0x%.8x, lo = 0x%.8x)\n", num, val, hi, lo);
	__asm__ __volatile__("wrmsr" : : "c"(num), "a"(lo), "d"(hi));
}

inline long long msr_rdtsc (void)
{
	unsigned int d;
	unsigned int a;
	asm("rdtsc;"
	    "mov %%edx, %0;"      // move edx into d
	    "mov %%eax, %1;"      // move eax into a
	    : "=r" (d), "=r" (a)  // output
	    :                     // input
	    : "%edx", "eax"       // clobbered regiters
	    );

	return (long long)d << 32 | a;
}

inline unsigned int msr_tsc_frequency (void)
{
	long long val;

	val = msr_read (MSR_PLATFORM_INFO);

	return (unsigned int)((val>>8) & 0xFF);
}