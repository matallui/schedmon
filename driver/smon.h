#ifndef __SMON_SMON_H__
#define __SMON_SMON_H__

/*
 * --------- LIBRARIES ---------
 */
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/types.h>


/*
 * ------- DEBUG --------
 */
#undef PDEBUG
#ifdef SMON_DEBUG
#  ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "smon: " fmt, ## args)
#  else
/* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif /* __KERNEL__ */
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif /* SMON_DEBUG */

/* Error Codes (start with SME)*/
#define SMENOMEM	1	/* no memory */
#define SMEMAXEV	2	/* max events reached */
#define SMENOEVT	3	/* event id doesn't exist */
#define SMEMAXES	4	/* max evsets reached */
#define SMENOEVS	5	/* evset id doesn't exist */
#define SMEIOCTL	6	/* error in IOCTL parameters */

/*
 * ------- IOCTL --------
 */

/* Use "0xa6" as magic number */
#define	SMON_IOC_MAGIC	0xa6

#define SMON_IOCRESET	_IO(SMON_IOC_MAGIC, 0)

/*
 * S means "Set"
 * U means "Unset"
 * G means "Get"
 * C means "Check"
 *
 */
#define SMON_IOCSEVT	_IOW(SMON_IOC_MAGIC, 1, int)
#define SMON_IOCGEVT	_IOR(SMON_IOC_MAGIC, 2, int)
#define SMON_IOCCEVT	_IOR(SMON_IOC_MAGIC, 3, int)
#define SMON_IOCSEVS	_IOW(SMON_IOC_MAGIC, 4, int)
#define SMON_IOCGEVS	_IOR(SMON_IOC_MAGIC, 5, int)
#define SMON_IOCCEVS	_IOR(SMON_IOC_MAGIC, 6, int)
#define SMON_IOCSTSK	_IOW(SMON_IOC_MAGIC, 7, int)
#define SMON_IOCUTSK	_IOW(SMON_IOC_MAGIC, 8, int)
#define SMON_IOCREAD	_IOW(SMON_IOC_MAGIC, 9, int)

#define SMON_IOC_MAXNR	9

struct smon_ioctl {
	union {
		struct smon_event	*event;
		struct smon_evset	*evset;
		struct smon_envir	*envir;
		unsigned int		items;		/* for 'Read' */
	};
	union {
		int			id;		/* only for 'Get' */
		int			pid;
		unsigned int		size;		/* for 'Read' */
	};
};

/*
 * ------- EVENT -------
 */
#define MAX_EVENTS	64

struct smon_event {
	char			tag[64];
	union {
		struct {
			long long	evsel		: 8,
					umask		: 8,
					usr		: 1,
					os		: 1,
					edge		: 1,
					pc		: 1,
					apic		: 1,
					any		: 1,
					en		: 1,
					inv		: 1,
					cmask		: 8,
					reserved	: 32;
		};
		long long		perfevtsel;
	};
};

#ifdef __KERNEL__

extern int smon_event_init (void);

extern int smon_event_exit (void);

extern int smon_set_event (struct smon_event *event);

extern inline struct smon_event* smon_get_event (unsigned int id);

extern int smon_check_event (unsigned int id);

#endif /* __KERNEL__ */

/*
 * -------- EVENT-SET -------
 */
#define MAX_EVSETS	64

#define	MAX_FX_CTRS	3
#define MAX_GP_CTRS	4

struct smon_evset {
	char			tag[64];
	int			evids[MAX_GP_CTRS];	/* -1 (or -lt 0) when event not configured */
	union {
		struct {
			long long	fx0_en		: 2,
					fx0_any		: 1,
					fx0_pmi		: 1,
					fx1_en		: 2,
					fx1_any		: 1,
					fx1_pmi		: 1,
					fx2_en		: 2,
					fx2_any		: 1,
					fx2_pmi		: 1,
					reserved	: 52;
		};
		long long		fixed_ctrl;
	};
	union {
		struct {
			long long	gp_ctr0_en	: 1,
					gp_ctr1_en	: 1,
					gp_ctr2_en	: 1,
					gp_ctr3_en	: 1,
					reserved_1	: 28,
					fx_ctr0_en	: 1,
					fx_ctr1_en	: 1,
					fx_ctr2_en	: 1,
					reserved_2	: 29;
		};
		long long		global_ctrl;
	};
};


#ifdef __KERNEL__

extern int smon_evset_init (void);

extern int smon_evset_exit (void);

extern int smon_set_evset (struct smon_evset *evset);

extern inline struct smon_evset* smon_get_evset (unsigned int id);

extern int smon_check_evset (unsigned int id);

#endif /* __KERNEL__ */

/*
 * -------- ENVIRONMENT --------
 */
#define MUX_EVSETS	32			/* Maximum Multiplexed Event-Sets per Environment*/

/* option flags */
#define ENVOP_INH	1
#define ENVOP_EXE	2
#define ENVOP_RAP	4
#define ENVOP_CPU	8
#define ENVOP_FORK	16
#define ENVOP_SCHED	32


#define DEFAULT_SAMPLE_TIME	10

struct smon_envir {
	int			esids[MUX_EVSETS];	/* EvSet Ids */
	unsigned int		n_esids;		/* Number of attached event-sets */

	unsigned int		sample_time;		/* Sample Time (if 0, only count) */
	unsigned int		batch_size;		/* Signal (poll) user each batch_size samples */

	union {
		unsigned int		inherit		: 1,
					on_exec		: 1,
					rapl		: 1,
					cpu		: 1,
					fork		: 1,
					reserved	: 27;
		unsigned int		options;
	};

#ifdef __KERNEL__
	atomic_t		count;			/* Number or tasks using this envir */
#endif /* __KERNEL__ */
};

#ifdef __KERNEL__

#define smon_for_each_evset(i,envir)	for(i = 0; (envir)->esids[i] >= 0; i++)

extern struct smon_envir * smon_create_envir (struct smon_envir *envir);

extern void smon_destroy_envir (struct smon_envir *envir);

extern struct smon_envir * smon_get_envir (struct smon_envir *envir);

extern void smon_release_envir (struct smon_envir *envir);

extern inline struct smon_evset * smon_envir_get_evset (struct smon_envir *envir, int id);

extern inline int smon_envir_next_evset (struct smon_envir *envir, int id);

extern inline int envir_option_inherit (struct smon_envir *envir);

extern inline int envir_option_exec (struct smon_envir *envir);

extern inline int envir_option_rapl (struct smon_envir *envir);

extern inline int envir_option_cpu (struct smon_envir *envir);

extern inline int envir_option_fork (struct smon_envir *envir);

extern inline int envir_option_sched (struct smon_envir *envir);


#endif /* __KERNEL__ */

/*
 * ---------- MSR ----------
 */

#ifdef __KERNEL__

extern inline long long msr_read ( int num );

extern inline void msr_write ( int num, long long val );

extern inline long long msr_rdtsc (void);

extern inline unsigned int msr_tsc_frequency (void);

#endif /*__KERNEL__*/


/*
 * ---------- PMCs ----------
 */
struct smon_pmc {
	union
	{
		struct
		{
			long long fx	[MAX_FX_CTRS];
			long long gp	[MAX_GP_CTRS];
		};
		long long ctr [MAX_FX_CTRS + MAX_GP_CTRS];
	};
};

#ifdef __KERNEL__

extern inline void smon_pmc_set (struct smon_evset *evset);

extern inline void smon_pmc_reset (void);

extern inline void smon_pmc_unset (struct smon_evset *evset);

extern inline void smon_pmc_start (struct smon_evset *evset);

extern inline void smon_pmc_stop (void);

extern inline void smon_pmc_read (struct smon_pmc *pmc, struct smon_evset *evset);

extern inline void smon_pmc_write (struct smon_pmc *pmc);

#endif /*__KERNEL__*/

/*
 * ---------- RAPL ----------
 */
struct smon_sample_info;

struct smon_rapl {
	long long pkg;
	long long pp0;
	long long pp1;
};

#ifdef __KERNEL__

extern inline void smon_rapl_get_info (struct smon_sample_info *info);

extern inline void smon_rapl_read (struct smon_rapl *rapl);

extern inline void smon_rapl_sub (struct smon_rapl *r, struct smon_rapl *a, struct smon_rapl *b);

#endif /*__KERNEL__*/
/*
 * ---------- SAMPLE ----------
 */

#define SAMPLE_TYPE_PMC		1
#define SAMPLE_TYPE_RAPL	2
#define SAMPLE_TYPE_CPU		3
#define	SAMPLE_TYPE_SCHED	4
#define SAMPLE_TYPE_FORK	5
#define SAMPLE_TYPE_INFO	6
#define SAMPLE_TYPE_PAD		7
#define SAMPLE_TYPE_EOF		8

struct smon_sample_header {
	unsigned int		type;
};

struct smon_sample_info {
	long long		timestamp;		/* starting TSC */
	unsigned int		pmc_sample_time;	
	unsigned int		rapl_sample_time;
	long long		energy_units;
	long long		power_units;
	long long		time_units;
	unsigned int		tsc_frequency;		/* TSC frequency */
	unsigned int		evsets;			/* number of event-sets */
	unsigned int		n_cpus;			/* number of logical cpus */
};

struct smon_sample_pmc {
	long long		timestamp_start;
	long long		timestamp_stop;
	long long		duration;
	pid_t			pid;
	struct smon_pmc		pmc;
	int			evset;
};

struct smon_sample_rapl {
	long long		timestamp;
	long long		duration;
	struct smon_rapl	rapl;
};

struct smon_sample_cpu {
	long long		timestamp;
	pid_t			pid;
	int			old_cpu;
	int			new_cpu;
};

struct smon_sample_fork {
	long long		timestamp;
	pid_t			parent;
	pid_t			child;
};

struct smon_sample_sched {
	long long		timestamp_in;
	long long		timestamp_out;
	pid_t			pid;
	int			cpu;
};

#ifdef __KERNEL__
struct smon_task;
struct smon_ring_buffer;

extern void smon_sample_init (void);

extern void smon_sample_write (struct smon_ring_buffer *rb, int type, void *sample );

extern void smon_sample_start (struct smon_task *task);

extern void smon_sample_stop (struct smon_task *task);

extern void smon_sample_save (struct smon_task *task);

extern void smon_sample_rapl_start (struct smon_sample_rapl *sample);

extern void smon_sample_rapl_stop (struct smon_sample_rapl *sample);

extern void smon_sample_rapl_save (struct smon_ring_buffer *rb, struct smon_sample_rapl *sample);

extern inline void smon_sample_sched_in (struct smon_task *task);

extern inline void smon_sample_sched_out (struct smon_task *task);

extern inline void smon_sample_sched_save (struct smon_task * task);

extern void smon_sample_get_info (struct smon_sample_info *info, struct smon_envir *envir);

extern void smon_sample_info_save (struct smon_ring_buffer *rb, struct smon_sample_info *sample);

#endif /* __KERNEL__ */

/*
 * --------- TASK --------
 */
#ifdef __KERNEL__

#include <linux/irq_work.h>
#include <linux/hrtimer.h>

#define smon_task_pid(task)	(task->p->pid)
#define smon_task_cpu(task)	task_cpu(task->p)

#define SMON_TIMER_ON		(unsigned int)1

struct smon_task_extra {

	struct smon_sample_rapl rapl_sample;
	unsigned int		rapl_sample_time;
	struct hrtimer		rapl_timer;
	struct irq_work		irq_start_timer;
	struct irq_work		irq_stop_timer;
	unsigned int		timer_flags;

	struct smon_sample_info info_sample;

	struct smon_ring_buffer *rb;
};

struct smon_task {
	struct task_struct 	*p;		/* pointer to kernel tast_struct */
	pid_t			pid;

	struct smon_envir	*envir;		/* pointer to environment */
	int			evset;		/* current evset id */

	struct list_head	task_list;	/* only the user-registered tasks (parents) get in this list */

	struct list_head	children;
	spinlock_t		lock_children;
	unsigned int		n_children;

	union {
		struct list_head	cpu_list;
		struct list_head	wait_list;
	};

	/* Sample Information */
	struct smon_sample_pmc	sample_pmc;	 /* PMC sample */
	long long		tsc_tmp;	 /* used to measure sample duration */
	struct hrtimer		timer;		 /* PMC sampling timer */
	ktime_t			kt_remain;	 /* remaining time of the current PMC sample */
	struct irq_work		irq_start_timer; /* Used to start the timer */
	struct irq_work		irq_stop_timer;	 /* Used to stop the timer */
	unsigned int		timer_flags;	 /* Used to avoid conflicts between irq and timer interrupts */

	struct smon_sample_sched sample_sched;

	/* Extra information for user-registered tasks (e.g., RAPL and sample_info) */
	struct smon_task_extra	*extra;

	/* ring buffer */
	struct smon_ring_buffer	*rb;
};

extern struct smon_task * smon_create_task (pid_t pid, struct smon_envir *envir, struct smon_ring_buffer *rb);

extern struct smon_task * smon_create_child (struct smon_task *parent, struct task_struct *p);

extern void smon_destroy_task (struct smon_task *task);

#endif /* __KERNEL__ */

/*
 * ---------- SCHEDULER --------
 */
#ifdef __KERNEL__

struct smon_cpu {
	int		evset;		/* currently configured evset */
};

struct smon_sched {
	struct list_head	task_list;
	spinlock_t		task_lock;

	struct list_head	cpu_list[NR_CPUS];
	spinlock_t		cpu_lock[NR_CPUS];

	struct list_head	wait_list;
	spinlock_t		wait_lock;

	struct smon_cpu		cpu[NR_CPUS];
};

extern int smon_sched_init (void);

extern int smon_sched_exit (void);

extern int smon_sched_register_task (pid_t pid, struct smon_envir *envir, struct smon_ring_buffer *rb);

extern int smon_sched_unregister_task (pid_t pid);

extern enum hrtimer_restart smon_sched_timer_rapl (struct hrtimer *timer);

extern inline void start_timer_pmc (struct irq_work *q);

extern inline void stop_timer_pmc (struct irq_work *q);

extern inline void start_timer_rapl (struct irq_work *q);

extern inline void stop_timer_rapl (struct irq_work *q);

#endif /* __KERNEL__ */

/*
 * ---------- RING-BUFFER ---------
 */
#ifdef __KERNEL__

#define SMON_MAX_PAGES		1024

struct smon_ring_buffer {

	unsigned int	n_pages;			/* total number of data pages */
	unsigned int	size;				/* n_pages * PAGE_SIZE */
	void		**data;				/* pages containing sampling information */
	
	unsigned int	head;				/* writing position */
	unsigned int	tail;				/* reading position */

	unsigned int	items;				/* number of samples in the buffer */
	unsigned int	batch;				/* batch size, defined by user (default is 1) */

	spinlock_t	lock;
	atomic_t	active;				/* number of active writers */

	wait_queue_head_t	inq;			/* wait queue for readers */
	struct irq_work		irq;			/* delay poll signaling */
};


extern struct smon_ring_buffer *smon_create_rb (unsigned int n_pages);

extern void smon_destroy_rb (struct smon_ring_buffer *rb);

extern inline struct smon_ring_buffer *smon_get_rb(struct smon_ring_buffer *rb);

extern inline void smon_release_rb(struct smon_ring_buffer *rb);

extern inline void smon_rb_lock (struct smon_ring_buffer *rb, unsigned long *flags);

extern inline void smon_rb_unlock (struct smon_ring_buffer *rb, unsigned long *flags);

extern inline int smon_rb_is_active (struct smon_ring_buffer *rb);

extern inline int smon_rb_is_empty (struct smon_ring_buffer *rb);

extern inline int smon_rb_is_full (struct smon_ring_buffer *rb);

extern void smon_rb_write (struct smon_ring_buffer *rb, struct smon_sample_header *header, void *sample);

extern void smon_rb_read (struct smon_ring_buffer *rb, unsigned int items, unsigned int sz);

extern inline int smon_rb_batch_available(struct smon_ring_buffer *rb);

extern void *smon_rb_get_page (struct smon_ring_buffer *rb, unsigned int i);

#endif /* __KERNEL__ */


#endif /* __SMON_SMON_H__ */
