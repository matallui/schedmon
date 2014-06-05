#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define SMON_DEBUG	/* Debug Mode (comment to toggle)*/
#include "smon.h"

#define RB_DEFAULT_BATCH		1

#define RB_PAGEMASK(rb)			((rb)->n_pages-1)	/* n_pages should be a power of 2 */

#define HEAD_PAGE(head)			((head) >> PAGE_SHIFT)
#define HEAD_OFFSET(head)		((head) & (PAGE_SIZE-1))

#define _RB_COUNT(head,tail,size)	(((head)-(tail)) & ((size)-1))
#define RB_COUNT(rb)			_RB_COUNT((rb)->head, (rb)->tail, (rb)->size)

#define _RB_SPACE(head,tail,size)	_RB_COUNT((tail), (head)+1, (size))
#define RB_SPACE(rb)			_RB_SPACE((rb)->head, (rb)->tail, (rb)->size)

#define _RB_PAGE_SPACE(offset)		((PAGE_SIZE) - (offset))
#define RB_PAGE_SPACE(rb)		_RB_PAGE_SPACE(HEAD_OFFSET((rb)->head))

#define RB_NEXT_PAGE(rb, page)		(((page)+1) & RB_PAGEMASK(rb))

#define HEAD_TRIM_PAGE(rb, head)	((*(head)) & ((RB_PAGEMASK(rb) << PAGE_SHIFT) | (PAGE_SIZE-1)))


static void smon_wake_up_reader (struct irq_work *q);
void smon_rb_write_eof (struct smon_ring_buffer *rb);

struct smon_ring_buffer *smon_create_rb(unsigned int n_pages)
{
	struct smon_ring_buffer *rb = NULL;
	int i;

	PDEBUG("create_rb:\t %d pages\n", n_pages);
	if (n_pages > SMON_MAX_PAGES)
		return NULL;

	/* allocate ring buffer */
	rb = (struct smon_ring_buffer *)kzalloc(sizeof(struct smon_ring_buffer), GFP_KERNEL);
	if (!rb)
		return NULL;

	/* allocate data pages */
	rb->data = kzalloc(n_pages * sizeof(void*), GFP_KERNEL);
	if (!rb->data)
		goto error_data;

	for (i = 0; i < n_pages; i++) {
		rb->data[i] = (char*)__get_free_pages(GFP_KERNEL, 0);
		if (!rb->data[i])
			goto error_pages;
	}

	/* set parameters */
	rb->n_pages = n_pages;
	rb->batch = RB_DEFAULT_BATCH;
	rb->size = n_pages * PAGE_SIZE;
	init_waitqueue_head(&rb->inq);
	init_irq_work(&rb->irq, smon_wake_up_reader);
	atomic_set(&rb->active, 0);
	spin_lock_init(&rb->lock);

	rb->head = 0;
	rb->tail = 0;

	return rb;

error_pages:
	while (--i) {
		free_pages((unsigned long)rb->data[i], 0);
		rb->data[i] = NULL;
	}
	kfree(rb->data);
	rb->data = NULL;
error_data:
	kfree(rb);
	return NULL;
}

void smon_destroy_rb (struct smon_ring_buffer *rb)
{
	int i;
	PDEBUG("destroy_rb:\t %d pages (%d items left)\n", rb->n_pages, rb->items);
	for (i = 0; i < rb->n_pages; i++) {
		free_pages((unsigned long)rb->data[i], 0);
		rb->data[i] = NULL;
	}
	kfree(rb->data);
	rb->data = NULL;
	kfree(rb);
}

struct smon_ring_buffer *smon_get_rb (struct smon_ring_buffer *rb)
{
	PDEBUG("get_rb: %d++\n", atomic_read(&rb->active));
	atomic_inc(&rb->active);
	return rb;
}

void smon_release_rb (struct smon_ring_buffer *rb)
{
	PDEBUG("release_rb: %d--\n", atomic_read(&rb->active));
	if (atomic_dec_and_test(&rb->active)) {
		smon_rb_write_eof(rb);
		irq_work_queue(&rb->irq);
	}
}

inline void smon_rb_lock (struct smon_ring_buffer *rb, unsigned long *flags)
{
	spin_lock_irqsave(&rb->lock, *flags);
}

inline void smon_rb_unlock (struct smon_ring_buffer *rb, unsigned long *flags)
{
	spin_unlock_irqrestore(&rb->lock, *flags);
}

inline int smon_rb_is_active (struct smon_ring_buffer *rb)
{
	return atomic_read(&rb->active) > 0;
}

inline int smon_rb_is_full (struct smon_ring_buffer *rb)
{
	return RB_SPACE(rb) == 0;
}

inline int smon_rb_is_empty (struct smon_ring_buffer *rb)
{
	return (RB_COUNT(rb) == 0);
	//return rb->items == 0;
}

inline void smon_rb_stuff_page (struct smon_ring_buffer *rb, unsigned int *head)
{
	unsigned int page = HEAD_PAGE(*head);
	unsigned int offset = HEAD_OFFSET(*head);
	struct smon_sample_header header = { .type = SAMPLE_TYPE_PAD, };

	memcpy ((void *)((unsigned long)rb->data[page] + (unsigned long)offset), (void*)&header, sizeof(struct smon_sample_header));

	*head = 0;
	*head |= (RB_NEXT_PAGE(rb, page) << PAGE_SHIFT);
}

inline void smon_wake_up_reader (struct irq_work *q)
{
	struct smon_ring_buffer *rb = container_of(q, struct smon_ring_buffer, irq);

	wake_up(&rb->inq);
}

void smon_rb_write_eof (struct smon_ring_buffer *rb)
{
	struct smon_sample_header header = { .type = SAMPLE_TYPE_EOF, };

	smon_rb_write(rb, &header, NULL);
}

void smon_rb_write (struct smon_ring_buffer *rb, struct smon_sample_header *header, void *sample)
{
	unsigned int page, offset;
	unsigned long flags;
	unsigned int sample_size;
	unsigned int total_size;
	unsigned int _head;	//-----------------//
	static unsigned int header_size = sizeof(struct smon_sample_header);

	switch (header->type) {
		case SAMPLE_TYPE_PMC:
			sample_size = sizeof(struct smon_sample_pmc);
			break;
		case SAMPLE_TYPE_RAPL:
			sample_size = sizeof(struct smon_sample_rapl);
			break;
		case SAMPLE_TYPE_CPU:
			sample_size = sizeof(struct smon_sample_cpu);
			break;
		case SAMPLE_TYPE_SCHED:
			sample_size = sizeof(struct smon_sample_sched);
			break;
		case SAMPLE_TYPE_FORK:
			sample_size = sizeof(struct smon_sample_fork);
			break;
		case SAMPLE_TYPE_INFO:
			sample_size = sizeof(struct smon_sample_info);
			break;
		case SAMPLE_TYPE_EOF:
			sample_size = 0;
			break;
		default:
			PDEBUG("rb_write: unknown header type (%d)\n", header->type);
			return;
	}


	total_size = sample_size + header_size;

	smon_rb_lock(rb, &flags);

	_head = rb->head;

	/* not enough space, don't write */
	if (total_size > RB_SPACE(rb)) {
		//PDEBUG("rb_write: buffer is FULL!\n");
		goto out;
	}

	/* does not fit this page -> stuff and update to next page */
	if (total_size > RB_PAGE_SPACE(rb)) {

		smon_rb_stuff_page(rb, &rb->head);

		/* check if enough space again */
		if (total_size > RB_SPACE(rb))
			goto out;
	}

	page = HEAD_PAGE(rb->head);
	offset = HEAD_OFFSET(rb->head);

	memcpy ((void*)((unsigned long)rb->data[page]+(unsigned long)offset), (void*)header, header_size);
	if (sample_size > 0)
		memcpy ((void*)((unsigned long)rb->data[page]+(unsigned long)offset+(unsigned long)header_size), sample, sample_size);

	rb->head += total_size;
	rb->head = HEAD_TRIM_PAGE(rb, &rb->head);

//	PDEBUG("smon_write: HEAD \t%u\t->\t%u\t(items = %u, RB_COUNT = %u)\n", _head, rb->head, rb->items+1, RB_COUNT(rb));

	if ((++(rb->items)) >= rb->batch) {
		irq_work_queue(&rb->irq);
//		PDEBUG("rb_write: alert user for %d items\n", rb->items);
	}

out:
	smon_rb_unlock(rb, &flags);
}

void smon_rb_read (struct smon_ring_buffer *rb, unsigned int items, unsigned int sz)
{
	unsigned long flags;
//	unsigned int tail; //----------//

	smon_rb_lock(rb, &flags);

//	tail = rb->tail;

	if (sz > RB_COUNT(rb)) {

		rb->tail = rb->head;

	} else {

		rb->tail += sz;
		rb->tail = HEAD_TRIM_PAGE(rb, &rb->tail);
	}

	if ( items > rb->items )
		rb->items = 0;
	else
		rb->items -= items;

//	PDEBUG("rb_read: TAIL \t%u\t->\t%u\t(items = %u, sz = %u)\n", tail, rb->tail, items, sz);

	smon_rb_unlock(rb, &flags);
}

inline int smon_rb_batch_available(struct smon_ring_buffer *rb)
{
	return (rb->items >= rb->batch);
}

void *smon_rb_get_page (struct smon_ring_buffer *rb, unsigned int i)
{
	if (i < rb->n_pages)
		return rb->data[i];
	return NULL;
}
