#include <linux/module.h>
#include <linux/kernel.h>
#include <trace/events/sched.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>

#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

#define MS_TO_NS(msec)	((msec)*1000000)

static struct smon_sched sched;

/*
 * Function Headers
 */
static void smon_sched_switch (void *data, struct task_struct *prev, struct task_struct *next);

static void smon_sched_migrate_task (void *data, struct task_struct *p, int dest_cpu);

static void smon_sched_process_fork (void *data, struct task_struct *parent, struct task_struct *child);

static void smon_sched_process_exec (void *data, struct task_struct *p, pid_t old_pid, struct linux_binprm *bprm);

static void smon_sched_process_exit (void *data, struct task_struct *p);

static void smon_sched__unregister_task (struct smon_task *task);

static enum hrtimer_restart smon_sched_timer_tick (struct hrtimer *timer);

static void smon_task_extra_start (struct smon_task *task);

static void smon_task_extra_rapl_start (struct smon_task_extra *extra);

static void smon_task_extra_rapl_stop (struct smon_task_extra *extra);


/*
 * SCHED ROUTINES
 */
int smon_sched_init (void)
{
	int i;

	INIT_LIST_HEAD (&sched.task_list);
	spin_lock_init (&sched.task_lock);
	INIT_LIST_HEAD (&sched.wait_list);
	spin_lock_init (&sched.wait_lock);
	for (i = 0; i < NR_CPUS; i++) {
		INIT_LIST_HEAD (&sched.cpu_list[i]);
		spin_lock_init(&sched.cpu_lock[i]);
	}

	register_trace_sched_switch (smon_sched_switch, NULL);
	register_trace_sched_migrate_task (smon_sched_migrate_task, NULL);
	register_trace_sched_process_fork (smon_sched_process_fork, NULL);
	register_trace_sched_process_exec (smon_sched_process_exec, NULL);
	register_trace_sched_process_exit (smon_sched_process_exit, NULL);

	return 0;
}

int smon_sched_exit (void)
{
	struct smon_task *task, *tmp;
	int i;

	unregister_trace_sched_switch (smon_sched_switch, NULL);
	unregister_trace_sched_migrate_task (smon_sched_migrate_task, NULL);
	unregister_trace_sched_process_fork (smon_sched_process_fork, NULL);
	unregister_trace_sched_process_exec (smon_sched_process_exec, NULL);
	unregister_trace_sched_process_exit (smon_sched_process_exit, NULL);

	for (i = 0; i < NR_CPUS; i++)
		list_for_each_entry_safe(task, tmp, &sched.cpu_list[i], cpu_list) {
			list_del(&task->cpu_list);
		}

	list_for_each_entry_safe(task, tmp, &sched.task_list, task_list) {
		smon_sched__unregister_task (task);
	}
	return 0;
}

int smon_sched_register_task (pid_t pid, struct smon_envir *envir, struct smon_ring_buffer * rb)
{
	struct smon_task *task;
	int cpu;
	unsigned long flags;

	PDEBUG("sched_register_task: pid = %d,\t current = %d\n", pid, current->pid);

	task = smon_create_task (pid, envir, rb);
	if (!task)
		return -1;

	/* Init timer for sampling */
	hrtimer_init (&task->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	task->timer.function = smon_sched_timer_tick;
	task->kt_remain = ns_to_ktime(0);

	/* add to global task list */
	spin_lock_irqsave(&sched.task_lock, flags);
	list_add(&task->task_list, &sched.task_list);
	spin_unlock_irqrestore(&sched.task_lock, flags);

	flags = 0;

	/* if on_exec option, add to wait_list (if not, put on cpu_list) */
	if (envir_option_exec(envir)) {
		PDEBUG("sched_register_task: exec option activated\n");
		spin_lock_irqsave(&sched.wait_lock, flags);
		list_add(&task->wait_list, &sched.wait_list);
		spin_unlock_irqrestore(&sched.wait_lock, flags);
	} else {
		PDEBUG("sched_register_task: exec option NOT activated\n");
		/* start extra (RAPL)*/
		if (task->extra)
			smon_task_extra_start(task);

		cpu = task_cpu(task->p);
		spin_lock_irqsave(&sched.cpu_lock[cpu], flags);
		list_add(&task->cpu_list, &sched.cpu_list[cpu]);
		spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);
	}

	return 0;
}

int smon_sched_unregister_task (pid_t pid)
{
	struct smon_task *task;
	int found = 0;
	unsigned long flags;

	PDEBUG("sched_unregister_task: pid = %d\n", pid);

	spin_lock_irqsave(&sched.task_lock, flags);
	list_for_each_entry(task, &sched.task_list, task_list) {
		if (task->pid == pid) {
			found++;
			break;
		}
	}
	spin_unlock_irqrestore(&sched.task_lock, flags);

	if (found)
		smon_sched__unregister_task(task);
	else
		return -1;
	return 0;
}

static int smon_sched_register_child (struct smon_task *parent, struct task_struct *p)
{
	struct smon_task *child;
	int cpu = task_cpu(p);
	unsigned long flags;

	child = smon_create_child (parent, p);
	if (!child)
		return -1;

	hrtimer_init (&child->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	child->timer.function = smon_sched_timer_tick;
	child->kt_remain = ns_to_ktime(0);
	
	spin_lock_irqsave(&parent->lock_children, flags);
	list_add(&child->children, &parent->children);
	parent->n_children++;
	spin_unlock_irqrestore(&parent->lock_children, flags);

	if (cpu == task_cpu(parent->p))
		list_add(&child->cpu_list, &sched.cpu_list[cpu]);
	else {
		spin_lock_irqsave(&sched.cpu_lock[cpu], flags);
		list_add(&child->cpu_list, &sched.cpu_list[cpu]);
		spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);
	}

	return 0;
}

static void smon_sched_unregister_children (struct smon_task *task)
{
	struct smon_task *child, *tmp;

	list_for_each_entry_safe(child, tmp, &task->children, children) {
		list_del(&child->children);
		hrtimer_cancel(&child->timer);
		smon_destroy_task(child);
	}
}

static void smon_sched__unregister_task (struct smon_task *task)
{
	unsigned long flags;

	PDEBUG("sched__unregister_task: pid = %d\n", task->pid);

	hrtimer_cancel(&task->timer);

	smon_sched_unregister_children(task);

	spin_lock_irqsave(&sched.task_lock, flags);
	list_del(&task->task_list);
	spin_unlock_irqrestore(&sched.task_lock, flags);

	/* Stop RAPL */
	if (envir_option_rapl(task->envir))
		smon_task_extra_rapl_stop(task->extra);

	smon_destroy_task(task);
}


/*
 * PROFILE ROUTINES
 */
inline void start_timer_pmc (struct irq_work *q)
{
	struct smon_task *task = container_of(q, struct smon_task, irq_start_timer);
	struct hrtimer * timer = &task->timer;
	ktime_t kt;

	/* Set Timer */
	if (ktime_to_ns(task->kt_remain) <= 0)
		kt = ktime_set(0, MS_TO_NS(task->envir->sample_time));
	else
		kt = task->kt_remain;

	task->timer_flags |= SMON_TIMER_ON;

	hrtimer_start (timer, kt, HRTIMER_MODE_REL);

	/* Start Sampling */
	smon_sample_start(task);
}

inline void stop_timer_pmc (struct irq_work *q)
{
	struct smon_task *task = container_of(q, struct smon_task, irq_stop_timer);
	struct hrtimer * timer = &task->timer;

	/* Cancel Timer */
	hrtimer_cancel(timer);
}

inline static void smon_sched_task_in (struct smon_task *task)
{
	/* Trace sched_switch information */
	if (envir_option_sched(task->envir))
		smon_sample_sched_in(task);

	/* Start timer & sample ASAP */
	irq_work_queue(&task->irq_start_timer);
}

inline static void smon_sched_task_out (struct smon_task *task)
{
	struct hrtimer *timer = &task->timer;

	if (hrtimer_active(timer)) {

		/* Stop Counting */
		smon_sample_stop (task);

		/* Save timer remaining time */
		task->kt_remain = hrtimer_expires_remaining(&task->timer);

		if (ktime_to_ns(task->kt_remain) <= 0)
			smon_sample_save (task);

		/* Cancel Timer ASAP */
		task->timer_flags &= (~SMON_TIMER_ON);
		irq_work_queue(&task->irq_stop_timer);

		/* Trace sched_switch information */
		if (envir_option_sched(task->envir)) {
			smon_sample_sched_out(task);
			smon_sample_sched_save(task);
		}
	}
}

static enum hrtimer_restart smon_sched_timer_tick (struct hrtimer *timer)
{
	struct smon_task *task;
	ktime_t	kt;
	ktime_t now;

	task = container_of(timer, struct smon_task, timer);

	if (task->timer_flags & SMON_TIMER_ON)
	{
		/* Take & Save Sample */
		smon_sample_stop (task);
		smon_sample_save (task);

		/* Restart Timer */
		kt = ktime_set(0, MS_TO_NS(task->envir->sample_time));
		now = hrtimer_cb_get_time(timer);
		hrtimer_forward (timer, now, kt);
		task->kt_remain = kt;

		/* Start Sampling again */
		smon_sample_start(task);

		return HRTIMER_RESTART;
	}
	
	return HRTIMER_NORESTART;
}


/*
 * EXTRA ROUTINES
 */
enum hrtimer_restart smon_sched_timer_rapl (struct hrtimer *timer)
{
	struct smon_task_extra *extra;
	ktime_t	kt;
	ktime_t now;

	extra = container_of(timer, struct smon_task_extra, rapl_timer);

	if (extra->timer_flags & SMON_TIMER_ON) {

		/* take, save  & restart Sample */
		smon_sample_rapl_stop (&extra->rapl_sample);
		smon_sample_rapl_save (extra->rb, &extra->rapl_sample);
		smon_sample_rapl_start(&extra->rapl_sample);

		/* Restart Timer */
		kt = ktime_set(0, MS_TO_NS(extra->rapl_sample_time));
		now = hrtimer_cb_get_time(timer);
		hrtimer_forward (timer, now, kt);

		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

inline void start_timer_rapl (struct irq_work *q)
{
	struct smon_task_extra *extra = container_of(q, struct smon_task_extra, irq_start_timer);
	ktime_t kt;

	extra->timer_flags |= SMON_TIMER_ON;

	/* Init RAPL sample */
	smon_sample_rapl_start(&extra->rapl_sample);

	/* Start RAPL timer */
	kt = ktime_set(0, MS_TO_NS(extra->rapl_sample_time));
	hrtimer_start (&extra->rapl_timer, kt, HRTIMER_MODE_REL);
}

inline void stop_timer_rapl (struct irq_work *q)
{
	struct smon_task_extra *extra = container_of(q, struct smon_task_extra, irq_start_timer);

	hrtimer_cancel (&extra->rapl_timer);
}

static void smon_task_extra_start (struct smon_task *task)
{
	struct smon_task_extra *extra = task->extra;


	/* Send Sample Info to User */
	smon_sample_get_info(&extra->info_sample, task->envir);
	smon_sample_info_save(extra->rb, &extra->info_sample);

	/* Start RAPL */
	if (envir_option_rapl(task->envir))
		irq_work_queue(&extra->irq_start_timer);
}

static void smon_task_extra_stop (struct smon_task *task)
{
	struct smon_task_extra *extra = task->extra;

	/* Start RAPL */
	if (envir_option_rapl(task->envir)) {

		extra->timer_flags &= (~SMON_TIMER_ON);
		irq_work_queue(&extra->irq_stop_timer);
	}
}

static inline void smon_task_extra_rapl_start (struct smon_task_extra *extra)
{
	ktime_t kt;

	/* Init RAPL sample */
	smon_sample_rapl_start(&extra->rapl_sample);

	/* Start RAPL timer */
	kt = ktime_set(0, MS_TO_NS(extra->rapl_sample_time));
	hrtimer_start (&extra->rapl_timer, kt, HRTIMER_MODE_REL);
}

static inline void smon_task_extra_rapl_stop (struct smon_task_extra *extra)
{
	hrtimer_cancel (&extra->rapl_timer);
}


/*
 * TRACEPOINT HOOKS
 */
static void smon_sched_switch (void *data, struct task_struct *prev, struct task_struct *next)
{
	struct smon_task *task;
	int cpu;
	int found;
	unsigned long flags;

	cpu = task_cpu(prev);

	found = 0;
	spin_lock_irqsave(&sched.cpu_lock[cpu], flags);
	list_for_each_entry (task, &sched.cpu_list[cpu], cpu_list) {
		if ( task->p == prev ) {
			spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);
			found++;
			//PDEBUG("smon_sched_switch: task %d OUT (cpu %d)\n", prev->pid, task_cpu(prev));
			smon_sched_task_out (task);
			break;
		}
	}
	if (!found)
		spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);

	found = 0;
	flags = 0;
	spin_lock_irqsave(&sched.cpu_lock[cpu], flags);
	list_for_each_entry (task, &sched.cpu_list[cpu], cpu_list) {
		if ( task->p == next ) {
			spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);
			found++;
			//PDEBUG("smon_sched_switch: task %d IN (cpu %d)\n", next->pid, task_cpu(next));
			smon_sched_task_in (task);
			break;
		}
	}
	if (!found)
		spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);

}

static void smon_sched_migrate_task (void *data, struct task_struct *p, int dest_cpu)
{
	struct smon_task *task, *tmp;
	int old_cpu, found = 0;
	unsigned long flags;
	struct smon_sample_cpu sample;

	old_cpu = task_cpu(p);
	spin_lock_irqsave(&sched.cpu_lock[old_cpu], flags);
	list_for_each_entry_safe(task, tmp, &sched.cpu_list[old_cpu], cpu_list) {
		if (task->p == p) {
			/* remove from old_cpu list */
			list_del(&task->cpu_list);
			found++;
			break;
		}
	}
	spin_unlock_irqrestore(&sched.cpu_lock[old_cpu], flags);

	if (found) {
		if (envir_option_cpu(task->envir)) {
			/* Send migration sample */
			sample.pid = task->pid;
			sample.old_cpu = old_cpu;
			sample.new_cpu = dest_cpu;
			sample.timestamp = msr_rdtsc();
			smon_sample_write (task->rb, SAMPLE_TYPE_CPU, (void*)&sample);
		}
		/* Insert into dest_cpu list */
		spin_lock_irqsave(&sched.cpu_lock[dest_cpu], flags);
		list_add(&task->cpu_list, &sched.cpu_list[dest_cpu]);
		spin_unlock_irqrestore(&sched.cpu_lock[dest_cpu], flags);
		//PDEBUG("sched_migrate_task: PID = %d,\t CPU = %d -> %d\n", smon_task_pid(task), old_cpu, dest_cpu);
	}
}

static void smon_sched_process_fork (void *data, struct task_struct *parent, struct task_struct *child)
{
	struct smon_task *task;
	int cpu;
	unsigned long flags;
	struct smon_sample_fork sample;

	cpu = task_cpu(parent);

	spin_lock_irqsave(&sched.cpu_lock[cpu], flags);

	list_for_each_entry(task, &sched.cpu_list[cpu], cpu_list) {

		if (task->p == parent) {

			if (envir_option_inherit(task->envir)) {
				/* Register child */
				PDEBUG("sched_process_fork: parent = %d,\t child = %d\n", parent->pid, child->pid);
				smon_sched_register_child (task, child);
			}

			if (envir_option_fork(task->envir)) {
				/* Send fork sample */
				sample.parent = task->pid;
				sample.child = child->pid;
				sample.timestamp = msr_rdtsc();
				smon_sample_write (task->rb, SAMPLE_TYPE_FORK, (void*)&sample);
			}

			break;
		}
	}

	spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);
}

static void smon_sched_process_exec (void *data, struct task_struct *p, pid_t old_pid, struct linux_binprm *bprm)
{
	struct smon_task *task, *tmp;
	int cpu, found = 0;
	unsigned long flags;

	spin_lock_irqsave(&sched.wait_lock, flags);
	list_for_each_entry_safe(task, tmp, &sched.wait_list, wait_list) {
		if (task->p == p) {
			list_del(&task->wait_list);
			found++;
			break;
		}
	}
	spin_unlock_irqrestore(&sched.wait_lock, flags);

	flags = 0;

	if (found) {
		PDEBUG("sched_process_exec: start monitor for task %d\n", p->pid);

		cpu = task_cpu(p);

		/* start extra */
		if (task->extra)
			smon_task_extra_start(task);

		spin_lock_irqsave(&sched.cpu_lock[cpu], flags);
		list_add(&task->cpu_list, &sched.cpu_list[cpu]);
		spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);

	}
}

static void smon_sched_process_exit (void *data, struct task_struct *p)
{
	struct smon_task *task, *tmp;
	int cpu;
	unsigned long flags;
	int found = 0;

	cpu = task_cpu(p);

	spin_lock_irqsave(&sched.cpu_lock[cpu], flags);

	list_for_each_entry_safe(task, tmp, &sched.cpu_list[cpu], cpu_list) {
		if (task->p == p) {
			found ++;
			/* Cancel Timer */
			if (hrtimer_active(&task->timer))
				hrtimer_cancel(&task->timer);
			/* Stop Counting */
			smon_sample_stop (task);

			/* Remove from Run List */
			list_del(&task->cpu_list);

			PDEBUG("sched_process_exit: task %d exits...\n", p->pid);

			task->p = NULL;
			smon_release_rb(task->rb);
			task->rb = NULL;

			if (task->extra)
				hrtimer_cancel (&task->extra->rapl_timer);

			break;
		}
	}

	spin_unlock_irqrestore(&sched.cpu_lock[cpu], flags);

	/* Check if it is in wait_list (execv error) */
	if (!found) {
		spin_lock_irqsave(&sched.wait_lock, flags);

		list_for_each_entry_safe(task, tmp, &sched.wait_list, wait_list) {
			if (task->p == p) {
				/* Remove from Wait List */
				list_del(&task->wait_list);

				PDEBUG("sched_process_exit: task %d exits... (*)\n", p->pid);

				task->p = NULL;
				smon_release_rb(task->rb);
				task->rb = NULL;

				break;
			}
		}

		spin_unlock_irqrestore(&sched.wait_lock, flags);
	}
}






