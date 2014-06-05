#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>

#define SMON_DEBUG	/* Debug Mode (comment to deactivate)*/
#include "smon.h"

static void smon_task_extra_create (struct smon_task *task);
static void smon_task_extra_destroy (struct smon_task *task);


struct smon_task * smon_create_task (pid_t pid, struct smon_envir *envir, struct smon_ring_buffer *rb)
{
	struct smon_task	*new;
	struct task_struct	*p;

	/* find task in task_struct list */
	for_each_process(p) {
		if (p->pid == pid)
			break;
	}

	new = (struct smon_task *)kzalloc(sizeof(struct smon_task), GFP_ATOMIC);
	if (!new)
		goto bad_malloc;

	/* Environment */
	new->envir = smon_create_envir(envir);
	if (!new->envir)
		goto bad_envir;

	/* Ring-Buffer */
	new->rb = smon_get_rb(rb);
	if (envir->batch_size > 0)
		new->rb->batch = envir->batch_size;

	/* Extra Data (real-parent)*/
	smon_task_extra_create(new);

	PDEBUG("smon_create_task: task %d created\n", pid);

	INIT_LIST_HEAD(&new->task_list);
	INIT_LIST_HEAD(&new->cpu_list);
	INIT_LIST_HEAD(&new->children);

	spin_lock_init (&new->lock_children);
	init_irq_work(&new->irq_start_timer, start_timer_pmc);
	init_irq_work(&new->irq_stop_timer, stop_timer_pmc);

	new->evset = 0;
	new->sample_pmc.pid = pid;
	new->sample_sched.pid = pid;

	new->p = p;
	new->pid = pid;
	
	return new;

bad_envir:
	kfree(new);
bad_malloc:
	return NULL;
}

void smon_destroy_task (struct smon_task *task)
{
	if (task->rb) {
		smon_release_rb(task->rb);
		task->rb = NULL;
	}
	if (task->envir) {
		smon_release_envir(task->envir);
		task->envir = NULL;
	}
	if (task->extra)
		smon_task_extra_destroy(task);

	kfree(task);
	task = NULL;
}

struct smon_task * smon_create_child (struct smon_task *parent, struct task_struct *p)
{
	struct smon_task	*new;

	new = (struct smon_task *)kzalloc(sizeof(struct smon_task), GFP_ATOMIC);
	if (!new)
		return NULL;

	new->rb  = smon_get_rb  (parent->rb);
	new->envir = smon_get_envir (parent->envir);

	PDEBUG("smon_create_child: child %d created\n", p->pid);

	INIT_LIST_HEAD(&new->task_list);
	INIT_LIST_HEAD(&new->cpu_list);
	INIT_LIST_HEAD(&new->children);
	new->n_children = 0;
	spin_lock_init (&new->lock_children);
	
	init_irq_work(&new->irq_start_timer, start_timer_pmc);
	init_irq_work(&new->irq_stop_timer, stop_timer_pmc);

	new->evset = 0;
	new->sample_pmc.pid = p->pid;
	new->sample_sched.pid = p->pid;

	new->extra = NULL;

	new->p = p;
	new->pid = p->pid;

	return new;
}

static void smon_task_extra_create (struct smon_task *task)
{
	task->extra = (struct smon_task_extra *)kzalloc(sizeof(struct smon_task_extra), GFP_ATOMIC);

	/* Init timer for rapl sampling */
	hrtimer_init (&task->extra->rapl_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	task->extra->rapl_timer.function = smon_sched_timer_rapl;

	/* ring buffer */
	task->extra->rb = task->rb;

	/* RAPL sample time */
	task->extra->rapl_sample_time = task->envir->sample_time;

	init_irq_work(&task->extra->irq_start_timer, start_timer_rapl);
	init_irq_work(&task->extra->irq_stop_timer, stop_timer_rapl);
}

static void smon_task_extra_destroy (struct smon_task *task)
{
	hrtimer_cancel (&task->extra->rapl_timer);
	kfree (task->extra);
	task->extra = NULL;
}














