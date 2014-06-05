/*
 * File: driver.c
 *
 * Description: Provides funtionality to communicate with
 *		the smon device.
 */
//#define SMON_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "driver.h"

#define DEVICE	"/dev/smon0"

static int smon_dev = 0;

void local_print_event (struct smon_event *event, int i);
void local_print_evset (struct smon_evset *evset, int i);

int smon_load_driver (void)
{
	smon_dev = open(DEVICE, 0);
	if (smon_dev == -1) {
		smon_dev = 0;
		PDEBUG("smon: error: loading device %s\n", DEVICE);
		return -1;
	}
	return 0;
}

int smon_unload_driver (void)
{
	if (smon_dev) {
		close(smon_dev);
		smon_dev = 0;
	}
	return 0;
}

int smon_get_dev_fd (void)
{
	return smon_dev;
}

int smon_set_event (struct smon_event *event)
{
	struct smon_ioctl ioc;
	int ret;

	if (!smon_dev)
		return -1;
	ioc.event = event;
	ret = ioctl(smon_dev, SMON_IOCSEVT, (void*)&ioc);
	return ret;
}

int smon_list_events (void)
{
	struct smon_ioctl ioc;
	struct smon_event event;
	int i = 0;

	if (!smon_dev)
		return -1;

	bzero ((void*)&event, sizeof(struct smon_event));
	ioc.event = &event;
	ioc.id = i;

	printf ("\n -> Available Events:\n");
	while (!ioctl(smon_dev, SMON_IOCGEVT, (void*)&ioc))
	{
		local_print_event (ioc.event, i);
		ioc.id = ++i;
	}
	printf (" [END] %d events available\n\n", i);

	return i;
}

int smon_check_events (struct smon_evset *evset)
{
	int i, evt;

	for (i = 0; i < MAX_GP_CTRS; i++)
	{
		evt = evset->evids[i];
		if ( (evt >= 0) && (ioctl(smon_dev, SMON_IOCCEVT, (unsigned int)evt)))
			return -SMENOEVT;
	}
	return 0;
}

int smon_set_evset (struct smon_evset *evset)
{
	struct smon_ioctl ioc;
	int ret;

	if (!smon_dev)
		return -1;
	ioc.evset = evset;
	ret = ioctl(smon_dev, SMON_IOCSEVS, (void*)&ioc);
	return ret;
}

int smon_list_evsets (void)
{
	struct smon_ioctl ioc;
	struct smon_evset evset;
	int i = 0;

	if (!smon_dev)
		return -1;

	bzero ((void*)&evset, sizeof(struct smon_evset));
	ioc.evset = &evset;
	ioc.id = i;

	printf ("\n -> Available Event-Sets:\n");
	while (!ioctl(smon_dev, SMON_IOCGEVS, (void*)&ioc))
	{
		local_print_evset (ioc.evset, i);
		ioc.id = ++i;
	}
	printf (" [END] %d event-sets available\n\n", i);

	return i;
}

int smon_check_evsets (struct smon_envir *envir)
{
	int i, esid;

	if (!smon_dev)
		return -1;

	for (i = 0; i < envir->n_esids; i++) {
		esid = envir->esids[i];
		if (ioctl(smon_dev, SMON_IOCCEVS, (unsigned long)esid))
			return -SMENOEVS;
	}
	return 0;
}

int smon_set_task (pid_t pid, struct smon_envir *envir)
{
	struct smon_ioctl ioc;
	int ret;

	if (!smon_dev)
		return -1;

	ioc.pid = pid;
	ioc.envir = envir;
	ret = ioctl(smon_dev, SMON_IOCSTSK, (void*)&ioc);
	return ret;
}

int smon_unset_task (pid_t pid)
{
	struct smon_ioctl ioc;
	int ret;

	if (!smon_dev)
		return -1;

	ioc.pid = pid;
	ret = ioctl(smon_dev, SMON_IOCUTSK, (void*)&ioc);
	return ret;
}

int smon_read_samples (unsigned int items, unsigned int size)
{
	struct smon_ioctl ioc;
	int ret;

	if (!smon_dev)
		return -1;

	ioc.items = items;
	ioc.size = size;
	ret = ioctl(smon_dev, SMON_IOCREAD, (void*)&ioc);

	return ret;
}

void *smon_mmap (void *addr, size_t length, int prot, int flags, off_t offset)
{
	if (!smon_dev)
		return NULL;
	return mmap(addr, length, prot, flags, smon_dev, offset);
}


/*
 * Local Functions
 */
void local_print_event (struct smon_event *event, int i)
{
	printf ("    [%d]  %s :  \t\tevsel=0x%.2x,umask=0x%.2x,mode=%d -> perfevtsel=0x%.8x\n", i, event->tag,
		(unsigned int)(event->perfevtsel & 0x000000ff), (unsigned int)((event->perfevtsel & 0x0000ff00)>>8),
		(unsigned int)((event->perfevtsel & 0x000f0000)>>16), (unsigned int)event->perfevtsel);
}

void local_print_evset (struct smon_evset *evset, int i)
{
	int k;
	printf ("    [%d]  %s :  \t\tfixed=0x%.3x, events=", i, evset->tag, (unsigned int)(evset->fixed_ctrl & 0x0fff));
	for (k = 0; k < MAX_GP_CTRS-1; k++)
		printf("%d,", evset->evids[k]);
	printf("%d\n", evset->evids[k]);
}





