#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

/* Array containing all the configured event-sets */
static struct smon_evset **evsets	= NULL;
static unsigned int n_evsets		= 0;

int smon_evset_init (void)
{
	PDEBUG("smon_evset_init()\n");
	evsets = (struct smon_evset**) kmalloc(MAX_EVSETS*sizeof(struct smon_evset*), GFP_KERNEL);
	if (!evsets)
		return -SMENOMEM;
	memset (evsets, 0, MAX_EVSETS*sizeof(struct smon_evset*));
	return 0;
}

int smon_evset_exit (void)
{
	int i;

	PDEBUG("smon_evset_exit()\n");
	for (i = 0; i < n_evsets; i++)
	{
		kfree(evsets[i]);
		evsets[i] = NULL;
	}
	kfree(evsets);
	evsets = NULL;
	n_evsets = 0;
	return 0;
}

int smon_set_evset (struct smon_evset *evset)
{
	struct smon_evset *new;
	int i;

	if (n_evsets == MAX_EVSETS)
		return -SMEMAXES;

	new = (struct smon_evset*) kmalloc(sizeof(struct smon_evset), GFP_KERNEL);
	if (!new)
		return -SMENOMEM;

	memcpy (new, evset, sizeof(struct smon_evset));

	for (i = 0; i < MAX_GP_CTRS; i++ ) {
		if (new->evids[i] < 0)
			continue;
		if (!smon_get_event(new->evids[i]))
			goto wrong_event;
	}

	evsets[n_evsets++] = new;
	return 0;

wrong_event:
	kfree (new);
	return -SMENOEVT;
}

inline struct smon_evset* smon_get_evset (unsigned int id)
{
	if (id >= n_evsets)
		return NULL;
	return evsets[id];
}

int smon_check_evset (unsigned int id)
{
	if (id < n_evsets)
		return 0;
	return -SMENOEVS;
}

int smon_del_evset (unsigned int id)
{
	return 0;
}
