#include <linux/module.h>
#include <linux/kernel.h>

//#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

struct smon_cpu cpu[NR_CPUS];

void smon_sample_init (void)
{
	int i;

	for (i = 0; i < NR_CPUS; i++) {
		cpu[i].evset = -1;
	}
}

void smon_sample_write (struct smon_ring_buffer *rb, int type, void *sample )
{
	struct smon_sample_header header = { .type = type, };

	if (rb)
		smon_rb_write (rb, &header, sample);
}

/*
 * PMC functionality
 */
void smon_sample_start (struct smon_task *task)
{
	struct smon_evset *evset = smon_get_evset(task->envir->esids[task->evset]);
	int cpuid = smp_processor_id();
	long long tsc;

	if (!evset) {
		PDEBUG("smon_sample_start: error: not able to get evset %d\n", task->evset);
		return;
	}
	/*
	 * If our evset is already configured, don't do it again
	 * - assumes schedmon is the only tool using HPMCs
	 */

	smon_pmc_set(evset);

	/* reload our counter values */
	smon_pmc_write(&task->sample_pmc.pmc);

	/* Timestamps */
	tsc = msr_rdtsc();
	task->tsc_tmp = tsc;
	if (task->sample_pmc.timestamp_start == 0)
		task->sample_pmc.timestamp_start = tsc;

	/* Start Counting */
	smon_pmc_start(evset);
}

void smon_sample_stop (struct smon_task *task)
{
	long long tsc;

	/* Stop Counting */
	smon_pmc_stop();

	/* Update Timestamps */
	tsc = msr_rdtsc();
	task->sample_pmc.timestamp_stop = tsc;
	task->sample_pmc.duration += (tsc - task->tsc_tmp);

	/* Update Sample */
	smon_pmc_read(&task->sample_pmc.pmc, smon_envir_get_evset(task->envir, task->evset));
}

void smon_sample_save (struct smon_task *task)
{
	/* Write Sample to Buffer */
	task->sample_pmc.pid = task->pid;
	task->sample_pmc.evset = task->evset;
	smon_sample_write (task->rb, SAMPLE_TYPE_PMC, (void*)&task->sample_pmc);
	
	/* Reset Sample */
	memset(&task->sample_pmc, 0, sizeof(struct smon_sample_pmc));

	/* Go to Next Evset */
	task->evset = smon_envir_next_evset(task->envir, task->evset);
	PDEBUG("smon_sample_save: next evset = %d\n", task->evset);
}

/*
 * RAPL functionality
 */
void smon_sample_rapl_start (struct smon_sample_rapl *sample)
{
	smon_rapl_read(&sample->rapl);
	sample->timestamp = msr_rdtsc();
}

void smon_sample_rapl_stop (struct smon_sample_rapl *sample)
{
	struct smon_rapl rapl;
	long long tsc;

	smon_rapl_read(&rapl);
	tsc = msr_rdtsc();

	sample->duration = tsc - sample->timestamp;
	smon_rapl_sub (&sample->rapl, &rapl, &sample->rapl);

}

void smon_sample_rapl_save (struct smon_ring_buffer *rb, struct smon_sample_rapl *sample)
{
	/* write sample to buffer */
	smon_sample_write (rb, SAMPLE_TYPE_RAPL, (void*)sample);
}

/*
 * SCHED sampling functionality
 */
inline void smon_sample_sched_in (struct smon_task *task)
{
	task->sample_sched.cpu = smp_processor_id();
	task->sample_sched.timestamp_in = msr_rdtsc();
}

inline void smon_sample_sched_out (struct smon_task *task)
{
	task->sample_sched.timestamp_out = msr_rdtsc();
}

inline void smon_sample_sched_save (struct smon_task * task)
{
	smon_sample_write (task->rb, SAMPLE_TYPE_SCHED, (void*)&task->sample_sched);
}

/*
 * Sample INFO
 */
void smon_sample_get_info (struct smon_sample_info *info, struct smon_envir *envir)
{
	info->evsets = (unsigned int)envir->n_esids;
	info->n_cpus = (unsigned int)num_online_cpus();

	info->pmc_sample_time = envir->sample_time;
	info->rapl_sample_time = envir->sample_time;

	smon_rapl_get_info(info);

	info->tsc_frequency = msr_tsc_frequency();
	info->timestamp = msr_rdtsc();
}

void smon_sample_info_save (struct smon_ring_buffer *rb, struct smon_sample_info *sample)
{
	/* write sample to buffer */
	smon_sample_write (rb, SAMPLE_TYPE_INFO, (void*)sample);
}







