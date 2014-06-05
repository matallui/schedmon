#ifndef _SMON_RB_H_
#define _SMON_RB_H_

#define PAGE_SIZE		4096
#define PAGE_SHIFT		12


struct smon_rb {
	void *addr;	/* starting address */
	unsigned long tail;
	unsigned int  size;
	unsigned int n_pages;
	unsigned int consumed;
};


void smon_rb_init (struct smon_rb *rb, void *addr, unsigned int mmap_pages);

inline unsigned int smon_rb_page (struct smon_rb *rb);

inline unsigned int smon_rb_offset (struct smon_rb *rb);

inline void * smon_rb_cursor (struct smon_rb *rb);

inline void smon_rb_consume (struct smon_rb *rb, unsigned int size);

inline unsigned int smon_rb_flush (struct smon_rb *rb);

inline void smon_rb_page_next (struct smon_rb *rb);

inline unsigned int smon_rb_count (struct smon_rb *rb, void *begin, void *end);

#endif
