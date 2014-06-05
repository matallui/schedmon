#ifndef _SMON_DRIVER_H_
#define _SMON_DRIVER_H_

#define SMON_DEBUG
#include "../../driver/smon.h"

#define DEFAULT_SAMPLE_TIME	10
#define DEFAULT_BURST		1000
#define DEFAULT_MMAP_PAGES	1024
#define DEFAULT_OFILE		"smon.data"

int smon_load_driver (void);

int smon_unload_driver (void);

int smon_get_dev_fd (void);

int smon_set_event (struct smon_event *event);

int smon_list_events (void);

int smon_check_events (struct smon_evset *evset);

int smon_set_evset (struct smon_evset *evset);

int smon_list_evsets (void);

int smon_check_evsets (struct smon_envir *envir);

int smon_set_task (pid_t pid, struct smon_envir *envir);

int smon_unset_task (pid_t pid);

int smon_read_samples (unsigned int items, unsigned int size);

void *smon_mmap (void *addr, size_t length, int prot, int flags, off_t offset);

#endif