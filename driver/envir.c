#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

//#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

struct smon_envir * smon_create_envir (struct smon_envir *envir)
{
	struct smon_envir *new;

	new = (struct smon_envir*) kzalloc(sizeof(struct smon_envir), GFP_KERNEL);
	if (!new)
		return NULL;

	memcpy (new->esids, envir->esids, MUX_EVSETS*sizeof(int));

	new->n_esids = envir->n_esids;

	new->options = envir->options;

	if (envir->sample_time > 0)
		new->sample_time = envir->sample_time;
	else
		new->sample_time = DEFAULT_SAMPLE_TIME;
	new->batch_size = envir->batch_size;
	
	if (envir_option_stat(new))
	{
		new->n_esids = 1;				// only 1 event-set in stat mode
		new->options &= ~ENVOP_RAP;		// disable RAPL
		new->options &= ~ENVOP_CPU;		// disable Migrations
		new->options &= ~ENVOP_FORK;	// disable Forks
		new->options &= ~ENVOP_SCHED;	// disable SCHED
	}

	PDEBUG("smon_create_envir: n_esids = %d\t, esid[0] = %d\n", envir->n_esids, envir->esids[0]);
	atomic_set(&new->count, 1);

	return new;
}

void smon_destroy_envir (struct smon_envir *envir)
{
	if (envir) {
		kfree (envir);
		envir = NULL;
	}
}


struct smon_envir * smon_get_envir (struct smon_envir *envir)
{
	atomic_inc(&envir->count);
	return envir;
}

void smon_release_envir (struct smon_envir *envir)
{
	if (atomic_dec_and_test(&envir->count)) {
		smon_destroy_envir(envir);
		PDEBUG("smon_destroy_envir()\n");
	} else
		PDEBUG("smon_release_envir()\n");
}

inline struct smon_evset * smon_envir_get_evset (struct smon_envir *envir, int id)
{
	struct smon_evset *evset;

	if (id >= envir->n_esids)
		return NULL;

	evset = smon_get_evset(envir->esids[id]);

	return evset;
}

inline int smon_envir_next_evset (struct smon_envir *envir, int id)
{
	return (id+1) % envir->n_esids;
}

inline int envir_option_inherit (struct smon_envir *envir)
{
	return envir->options & ENVOP_INH;
}

inline int envir_option_exec (struct smon_envir *envir)
{
	return envir->options & ENVOP_EXE;
}

inline int envir_option_rapl (struct smon_envir *envir)
{
	return envir->options & ENVOP_RAP;
}

inline int envir_option_cpu (struct smon_envir *envir)
{
	return envir->options & ENVOP_CPU;
}

inline int envir_option_fork (struct smon_envir *envir)
{
	return envir->options & ENVOP_FORK;
}

inline int envir_option_sched (struct smon_envir *envir)
{
	return envir->options & ENVOP_SCHED;
}

inline int envir_option_stat (struct smon_envir *envir)
{
	return envir->options & ENVOP_STAT;
}








