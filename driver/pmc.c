#include <linux/module.h>
#include <linux/kernel.h>

//#define SMON_DEBUG
#include "smon.h"


/* GP Counter Configuration */
#define IA32_PERFEVTSEL(X)		(0x186+(X))

/* Fixed Counter Configuration */
#undef  IA32_FIXED_CTR_CTRL
#define IA32_FIXED_CTR_CTRL		0x38D

/* Global Configurations */
#undef  IA32_PERF_GLOBAL_CTRL
#define IA32_PERF_GLOBAL_CTRL		0x38F

/* Overflow Control (if ctr bit is 1 -> overflow) */
#undef  IA32_PERF_GLOBAL_STATUS
#define IA32_PERF_GLOBAL_STATUS		0x38E

/* Used to Clear CTR Overflows */
#undef  IA32_PERF_GLOBAL_OVF_CTRL
#define IA32_PERF_GLOBAL_OVF_CTRL	0x390

/* Fixed Counters */
#define IA32_FX_CTR(X)			(0x309+(X))

/* GP Counters */
#define IA32_GP_CTR(X)			(0x4C1+(X))


inline void smon_pmc_clr_overflows (void)
{
	msr_write(IA32_PERF_GLOBAL_OVF_CTRL, (long long)0x0);
}

inline void smon_pmc_set (struct smon_evset *evset)
{
	int i;
	struct smon_event *event;

	msr_write(IA32_FIXED_CTR_CTRL, evset->fixed_ctrl);
	for (i = 0; i < MAX_GP_CTRS; i++) {
		event = smon_get_event(evset->evids[i]);
		if (event) {
			PDEBUG("smon_pmc_set: %d: 0x%x\n", i, event->perfevtsel);
			msr_write(IA32_PERFEVTSEL(i), event->perfevtsel);
		}
	}
	/* clear overflows */
	smon_pmc_clr_overflows();
}

inline void smon_pmc_reset (void)
{
	int i;
	/* reset fixed counters */
	for (i = 0; i < MAX_FX_CTRS; i++)
		msr_write(IA32_FX_CTR(i), (long long)0x0);
	/* reset gp counters */
	for (i = 0; i < MAX_GP_CTRS; i++)
		msr_write(IA32_GP_CTR(i), (long long)0x0);
}

inline void smon_pmc_unset (struct smon_evset *evset)
{

}

inline void smon_pmc_start (struct smon_evset *evset)
{
	/* start counting (counters must be already configured) */
	msr_write(IA32_PERF_GLOBAL_CTRL, evset->global_ctrl);
}

inline void smon_pmc_stop (void)
{
	/* stop counting */
	msr_write(IA32_PERF_GLOBAL_CTRL, (long long)0x0);
}

inline void smon_pmc_read (struct smon_pmc *pmc, struct smon_evset *evset)
{
	int i;

	/* Read FX Counters */
	for (i = 0; i < MAX_FX_CTRS; i++)
		if (evset->fixed_ctrl & (long long)(0x3 << (4*i)))
			pmc->fx[i] = msr_read(IA32_FX_CTR(i));
		else
			pmc->fx[i] = 0;

	/* Read GP Counters */
	for (i = 0; i < MAX_GP_CTRS; i++)
		if (evset->evids[i] >= 0)
			pmc->gp[i] = msr_read(IA32_GP_CTR(i));
		else
			pmc->gp[i] = 0;
}

inline void smon_pmc_write (struct smon_pmc *pmc)
{
	int i;
	
	/* Write to FX Counters */
	for (i = 0; i < MAX_FX_CTRS; i++)
		msr_write(IA32_FX_CTR(i), pmc->fx[i]);

	/* Write to GP Counters */
	for (i = 0; i < MAX_GP_CTRS; i++)
		msr_write(IA32_GP_CTR(i), pmc->gp[i]);

	/* Clear Overflows */
	smon_pmc_clr_overflows();
}










