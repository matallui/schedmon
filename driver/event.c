#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

//#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

/* Array containing all the configured events */
static struct smon_event **events	= NULL;
static unsigned int n_events		= 0;

int smon_event_init (void)
{
	PDEBUG("smon_event_init()\n");
	events = (struct smon_event**) kmalloc(MAX_EVENTS*sizeof(struct smon_event*), GFP_KERNEL);
	if (!events)
		return -SMENOMEM;
	memset (events, 0, MAX_EVENTS*sizeof(struct smon_event*));
	return 0;
}

int smon_event_exit (void)
{
	int i;
	//struct smon_event *ptr;

	PDEBUG("smon_event_exit()\n");
	for (i = 0; i < n_events; i++)
	{
		kfree(events[i]);
		events[i] = NULL;
	}
	kfree(events);
	events = NULL;
	n_events = 0;
	return 0;
}

int smon_set_event (struct smon_event *event)
{
	struct smon_event *new;

	if (n_events == MAX_EVENTS)
		return -SMEMAXEV;

	new = (struct smon_event*) kmalloc(sizeof(struct smon_event), GFP_KERNEL);
	if (!new)
		return -SMENOMEM;

	memcpy (new, event, sizeof(struct smon_event));

	events[n_events++] = new;

	return 0;
}

inline struct smon_event* smon_get_event (unsigned int id)
{
	if (id >= n_events || id < 0)
		return NULL;
	return events[id];
}

int smon_check_event (unsigned int id)
{
	if (id < n_events && id >= 0)
		return 0;
	return -SMENOEVT;
}

int smon_del_event (unsigned int id)
{
	return 0;
}


