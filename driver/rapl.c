#include <linux/module.h>
#include <linux/kernel.h>

//#define SMON_DEBUG
#include "smon.h"

/*
 * RAPL Addresses
 */
#undef  MSR_RAPL_POWER_UNIT
#define MSR_RAPL_POWER_UNIT		0x606

/* RAPL PKG */
#undef  MSR_PKG_RAPL_POWER_LIMIT
#define MSR_PKG_RAPL_POWER_LIMIT	0x610

#undef  MSR_PKG_ENERGY_STATUS
#define MSR_PKG_ENERGY_STATUS		0x611

#undef  MSR_RAPL_PERF_STATUS
#define MSR_RAPL_PERF_STATUS		0x613

#undef  MSR_PKG_POWER_INFO
#define MSR_PKG_POWER_INFO		0x614

/* RAPL PP0 - cores */
#undef  MSR_PP0_POWER_LIMIT
#define MSR_PP0_POWER_LIMIT		0x638

#undef  MSR_PP0_ENERGY_STATUS
#define MSR_PP0_ENERGY_STATUS		0x639

#undef  MSR_PP0_POLICY
#define MSR_PP0_POLICY			0x63A

#undef  MSR_PP0_PERF_STATUS
#define MSR_PP0_PERF_STATUS		0x63B

/* RAPL PP1 - graphics (Intel client - A architectures) */
#undef  MSR_PP1_POWER_LIMIT
#define MSR_PP1_POWER_LIMIT		0x640

#undef  MSR_PP1_ENERGY_STATUS
#define MSR_PP1_ENERGY_STATUS		0x641

#undef  MSR_PP1_POLICY
#define MSR_PP1_POLICY			0x642

/* RAPL DRAM (Intel server - D architectures) */
#undef  MSR_DRAM_POWER_LIMIT
#define MSR_DRAM_POWER_LIMIT		0x618

#undef  MSR_DRAM_ENERGY_STATUS
#define MSR_DRAM_ENERGY_STATUS		0x619

#undef  MSR_DRAM_PERF_STATUS
#define MSR_DRAM_PERF_STATUS		0x61B

#undef  MSR_DRAM_POWER_INFO
#define MSR_DRAM_POWER_INFO		0x61C


inline void smon_rapl_get_info (struct smon_sample_info *info)
{
	long long units_info;

	units_info = msr_read ( MSR_RAPL_POWER_UNIT );

	info->power_units	= 1 << (units_info & 0xf);
	info->energy_units	= 1 << ((units_info >> 8) & 0x1f);
	info->time_units	= 1 << ((units_info >> 16) & 0xf);
}

inline void smon_rapl_read (struct smon_rapl *rapl)
{
	rapl->pkg = msr_read ( MSR_PKG_ENERGY_STATUS );
	rapl->pp0 = msr_read ( MSR_PP0_ENERGY_STATUS );
	rapl->pp1 = msr_read ( MSR_PP1_ENERGY_STATUS );
}

inline void smon_rapl_sub (struct smon_rapl *r, struct smon_rapl *a, struct smon_rapl *b)
{
	r->pkg = a->pkg - b->pkg;
	r->pp0 = a->pp0 - b->pp0;
	r->pp1 = a->pp1 - b->pp1;
}













