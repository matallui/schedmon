#include <stdio.h>
#include <stdlib.h>
#include "driver.h"
#include "rb.h"

void smon_rb_init (struct smon_rb *rb, void *addr, unsigned int mmap_pages)
{
	rb->addr = addr;
	rb->tail = 0;
	rb->consumed = 0;
	rb->size = mmap_pages * PAGE_SIZE;
	rb->n_pages = mmap_pages;
}

inline unsigned int smon_rb_page (struct smon_rb *rb)
{
	return ((unsigned int)(rb->tail >> PAGE_SHIFT));
}

inline unsigned int smon_rb_offset (struct smon_rb *rb)
{
	return ((unsigned int)(rb->tail & (PAGE_SIZE-1)));
}

inline void * smon_rb_cursor (struct smon_rb *rb)
{
	return (void*)((unsigned long)rb->addr + rb->tail);
}

inline void smon_rb_consume (struct smon_rb *rb, unsigned int size)
{
	unsigned long tmp = rb->tail;

	tmp += size;
	tmp &= (((rb->n_pages-1) << PAGE_SHIFT) | (PAGE_SIZE-1));

	rb->tail = tmp;

	rb->consumed += size;
}

inline unsigned int smon_rb_flush (struct smon_rb *rb)
{
	unsigned int tmp;
	
	tmp = rb->consumed;
	rb->consumed = 0;

	return tmp;
}
inline void smon_rb_page_next (struct smon_rb *rb)
{
	unsigned long old_tail = rb->tail;

	rb->tail += (1 << PAGE_SHIFT);
	rb->tail &= ((rb->n_pages-1) << PAGE_SHIFT);

	rb->consumed += smon_rb_count(rb, (void*)old_tail, (void*)rb->tail);
}

inline unsigned int smon_rb_count (struct smon_rb *rb, void *begin, void *end)
{
	return ((unsigned int)(((unsigned long)end-(unsigned long)begin)) & (rb->size - 1));
}


