#define SMON_DEBUG

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/types.h>
#include <signal.h>
#include <libgen.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <sched.h>
#include "driver.h"
#include "rb.h"
#include "profile.h"


static pid_t	child_pid;	/* Child PID - for unexpected termination */
FILE		*fp;

unsigned int count_mode;
struct smon_sample_rapl rapl_counts;
struct smon_sample_pmc pmc_counts[MUX_EVSETS];


/*
 * Signal Functionality
 */
void handler_exit (int sig)
{
	/* terminate running child */
	PDEBUG("smon: interrupt: the program was aborted by the user!\n");
	kill(child_pid, SIGINT);
}

/*
 * Printing Functionality
 */
void fprint_sample_info (FILE *fp, struct smon_sample_info *sample)
{
	/* Frequency is in Hz */
	fprintf (fp, "[INFO]\t%lld\t%lu\t%u\t%u\t%u\t%u\t%lld\t%lld\t%lld\n",
		 sample->timestamp, (unsigned long)sample->tsc_frequency*100000000, sample->n_cpus,
		 sample->pmc_sample_time, sample->rapl_sample_time, sample->evsets,
		 sample->energy_units, sample->power_units, sample->time_units);
}

void fprint_sample_pmc (FILE *fp, struct smon_sample_pmc *sample)
{
	struct smon_pmc *pmc = &sample->pmc;

	fprintf (fp, "[PMC]\t%lld\t%lld\t%lld\t%d\t%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
		sample->timestamp_start, sample->timestamp_stop, sample->duration, sample->pid, sample->evset,
		pmc->fx[0], pmc->fx[1], pmc->fx[2], pmc->gp[0], pmc->gp[1], pmc->gp[2], pmc->gp[3]);
}

void fprint_sample_rapl (FILE *fp, struct smon_sample_rapl *sample)
{
	struct smon_rapl *rapl = &sample->rapl;

	fprintf (fp, "[RAPL]\t%lld\t%lld\t%lld\t%lld\t%lld\n",
		sample->timestamp, sample->duration, rapl->pkg, rapl->pp0, rapl->pp1);
}

void fprint_sample_cpu (FILE *fp, struct smon_sample_cpu *sample)
{
	fprintf (fp, "[MIGRATION]\t%lld\t%d\t%d\t%d\n",
		sample->timestamp, sample->pid, sample->old_cpu, sample->new_cpu);
}

void fprint_sample_sched (FILE *fp, struct smon_sample_sched *sample)
{
	fprintf (fp, "[SCHED]\t%lld\t%lld\t%d\t%d\n",
		 sample->timestamp_in, sample->timestamp_out, sample->pid, sample->cpu);
}

void fprint_sample_fork (FILE *fp, struct smon_sample_fork *sample)
{
	fprintf (fp, "[FORK]\t%lld\t%d\t%d\n",
		sample->timestamp, sample->parent, sample->child);
}

/*
 * Count Mode Functionality
 */
void add_counts_pmc (struct smon_sample_pmc *sample_pmc)
{
	int e = sample_pmc->evset;
	int i;

	pmc_counts[e].duration += sample_pmc->duration;
	for (i = 0; i < MAX_FX_CTRS; i++)
		pmc_counts[e].pmc.fx[i] += sample_pmc->pmc.fx[i];
	for (i = 0; i < MAX_GP_CTRS; i++)
		pmc_counts[e].pmc.gp[i] += sample_pmc->pmc.gp[i];
}

void add_counts_rapl (struct smon_sample_rapl *sample_rapl)
{
	rapl_counts.duration += sample_rapl->duration;
	rapl_counts.rapl.pkg += sample_rapl->rapl.pkg;
	rapl_counts.rapl.pp0 += sample_rapl->rapl.pp0;
	rapl_counts.rapl.pp1 += sample_rapl->rapl.pp1;
}

/*
 * Sampling Functionality
 */
int read_samples (struct smon_rb *rb, int nr_samples)
{
	struct smon_sample_header	*sample_header;
	struct smon_sample_info		*sample_info;
	struct smon_sample_pmc		*sample_pmc;
	struct smon_sample_rapl		*sample_rapl;
	struct smon_sample_cpu		*sample_cpu;
	struct smon_sample_fork		*sample_fork;
	struct smon_sample_sched	*sample_sched;

	int n_samples;
	unsigned int bytes;

	int eof = 0;

//	printf("read_samples: >> TAIL\t%lu\n", (unsigned long)smon_rb_cursor(rb) - (unsigned long)rb->addr);

	for (n_samples = 0; (n_samples < nr_samples) && !eof; n_samples++)
	{
//		printf("read_samples: PTR \t%lu\t\t", (unsigned long)smon_rb_cursor(rb) - (unsigned long)rb->addr);

		sample_header = (struct smon_sample_header *)smon_rb_cursor(rb);

		switch (sample_header->type) {

			case SAMPLE_TYPE_PMC:

//				printf("[PMC]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_pmc = (struct smon_sample_pmc *)smon_rb_cursor(rb);
				fprint_sample_pmc(fp, sample_pmc);
				smon_rb_consume(rb, sizeof(struct smon_sample_pmc));
				break;

			case SAMPLE_TYPE_RAPL:

//				printf("[RAPL]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_rapl = (struct smon_sample_rapl *)smon_rb_cursor(rb);
				fprint_sample_rapl(fp, sample_rapl);
				smon_rb_consume(rb, sizeof(struct smon_sample_rapl));
				break;

			case SAMPLE_TYPE_CPU:

//				printf("[MIGRATION]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_cpu = (struct smon_sample_cpu *)smon_rb_cursor(rb);
				fprint_sample_cpu(fp, sample_cpu);
				smon_rb_consume(rb, sizeof(struct smon_sample_cpu));
				break;

			case SAMPLE_TYPE_SCHED:

				//				printf("[SCHED]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_sched = (struct smon_sample_sched *)smon_rb_cursor(rb);
				fprint_sample_sched(fp, sample_sched);
				smon_rb_consume(rb, sizeof(struct smon_sample_sched));
				break;

			case SAMPLE_TYPE_FORK:

//				printf("[FORK]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_fork = (struct smon_sample_fork *)smon_rb_cursor(rb);
				fprint_sample_fork(fp, sample_fork);
				smon_rb_consume(rb, sizeof(struct smon_sample_fork));
				break;

			case SAMPLE_TYPE_INFO:

//				printf("[INFO]\n");
				smon_rb_consume(rb, sizeof(struct smon_sample_header));
				sample_info = (struct smon_sample_info *)(sample_header + 1);
				fprint_sample_info(fp, sample_info);
				smon_rb_consume(rb, sizeof(struct smon_sample_info));
				break;

			case SAMPLE_TYPE_PAD:

//				printf("[next]\n");
				smon_rb_page_next(rb);
				n_samples--;
				break;

			case SAMPLE_TYPE_EOF:

				//printf("[finished]\n");
				eof++;
				break;

			default:

//				PDEBUG("smon: read_samples: error: unknown sample header\n");
				eof++;
				n_samples--;
				return -1;
				break;
		}
	}

	bytes = smon_rb_flush(rb);
//	printf("read_samples: >> PTR \t%lu\n", (unsigned long)smon_rb_cursor(rb) - (unsigned long)rb->addr);
//	printf("read_samples: READ_COUNT\t%u\n", bytes);
	smon_read_samples(n_samples, bytes);

	return 0;
}

void start_sampling (void *addr, struct smon_cmd *cmd)
{
	struct smon_rb rb;
	struct pollfd fds[1];
	int ret, done = 0, err = 0;
	struct smon_envir *envir = (struct smon_envir *)cmd->ptr;

	fds[0].fd = smon_get_dev_fd();
	fds[0].events = POLLIN | POLLHUP;

	smon_rb_init(&rb, addr, cmd->mmap_pages);

	while (!done && !err) {
		ret = poll (fds, 1, -1);
		switch (ret) {
			case 1:
				if (fds[0].revents & POLLIN) {
					err = read_samples (&rb, envir->batch_size);

				} else if (fds[0].revents & POLLHUP) {
					//PDEBUG(">> POLLHUP <<\n");
					err = read_samples (&rb, rb.size);
					done++;
				}

				break;
			case 0:
				/* Time-out */
			case -1:
				/* Error */
				done++;
				PDEBUG("smon: sampling: error on poll (returned %d)\n", ret);
				break;

		}
	}

}


/*
 * Start Profiling
 */
int smon_set_affinity (pid_t pid, unsigned int cpumask)
{
	int err, i;
	cpu_set_t affinity_mask;

	CPU_ZERO ( &affinity_mask );

	if (!cpumask)
		return 0;

	for (i = 0; i < 32; i++)
		if ((cpumask & (1 << i)))
			CPU_SET ( i, &affinity_mask );

	err = sched_setaffinity ( pid, sizeof(affinity_mask), &affinity_mask );
	if (err) {
		PDEBUG ("smon: error: sched_setaffinity failed\n");
		return 1;
	}
	return 0;
}


int start_profile (char *path, char **argv, struct smon_cmd *cmd)
{
	pid_t	pid;
	int	err = 0;
	void	*addr = NULL;
	sem_t	*sem;
	char	sem_name[64];
	char	exec_script[512];
	struct smon_envir *envir = (struct smon_envir *)cmd->ptr;

	/* Initialize Structures for count_mode */
	bzero(&rapl_counts, sizeof(struct smon_sample_rapl));
	bzero(&pmc_counts, sizeof(struct smon_sample_pmc) * MUX_EVSETS);
	count_mode = cmd->count_mode;

	sprintf(sem_name, "smon_sem.XXXXXX");
	mkstemp(sem_name);

	/* Open Output File */
	fp = fopen (cmd->out_file, "w");
	if (fp == NULL) {
		fprintf (stderr, "smon: error: creating output file '%s'\n", cmd->out_file);
		return -1;
	}

	sem = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR, 0);

	if ((pid=fork()) == 0) {

		char *dir, *base;
		int dir_err = 0;

		base = basename(path);
		dir = dirname(path);
		if (chdir(dir) == -1) {
			dir_err++;
			PDEBUG("smon: start_profile: error in chdir(%s)\n", dir);
		}

		/* Set CPU affinity */
		if (cmd->cpumask > 0)
			smon_set_affinity(getpid(), cmd->cpumask);

		/* synchronization between parent and child before exec */
		sem_wait(sem);
		sem_close(sem);

		err += execv (base, argv);
		if (err) {
			fprintf (stderr, "smon: start_profile: error: execv: check your program parameters.\n");
			exit(-1);
		}
		exit(err);

	} else if (pid > 0) {

		child_pid = pid;

		/* Be prepared for outside termination */
		signal (SIGINT, handler_exit);

		/* Allocate Ring-Buffer */
		addr = smon_mmap (0, cmd->mmap_pages*PAGE_SIZE, PROT_READ, MAP_SHARED, 0);
		if (addr == MAP_FAILED) {
			PDEBUG("smon: start_profile: error: mmap failed - n_pages = %d\n", cmd->mmap_pages);
			kill(pid, SIGINT);
			wait(NULL);
			return ++err;
		}

		/* Register child task to be monitored */
		err = smon_set_task(pid, envir);
		if (err) {
			PDEBUG("start_profile: error registering task\n");
			kill(pid, SIGINT);
			munmap (addr, cmd->mmap_pages*PAGE_SIZE);
			return err;
		}

		/* Start Child */
		sem_post(sem);
		sem_close(sem);

		/* Start Sampling */
		start_sampling(addr, cmd);

		/* Terminate */
		wait (NULL);
		smon_unset_task(pid);
		munmap (addr, cmd->mmap_pages*PAGE_SIZE);


	} else {
		PDEBUG("smon: start_profile: error: fork()\n");
		err++;
	}

	fclose(fp);
	/* Parse Output File */
	if (cmd->type == CMD_ROOF) {
		sprintf(exec_script, "/home/matallui/schedmon/scripts/parse_roof.sh %s %s.roof", cmd->out_file, cmd->out_file);
		system(exec_script);
	} else {
		sprintf(exec_script, "/home/matallui/schedmon/scripts/parse_prof.sh %s %s.prof", cmd->out_file, cmd->out_file);
		system(exec_script);
	}

	return err;
}
