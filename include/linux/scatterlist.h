#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _LINUX_SCATTERLIST_H
#define _LINUX_SCATTERLIST_H

#include <linux/string.h>
#include <linux/bug.h>
#include <linux/mm.h>

#include <asm/types.h>
#include <asm/scatterlist.h>
#include <asm/io.h>

struct sg_table {
	struct scatterlist *sgl;	 
	unsigned int nents;		 
	unsigned int orig_nents;	 
};

#define SG_MAGIC	0x87654321

#define sg_is_chain(sg)		((sg)->page_link & 0x01)
#define sg_is_last(sg)		((sg)->page_link & 0x02)
#define sg_chain_ptr(sg)	\
	((struct scatterlist *) ((sg)->page_link & ~0x03))

static inline void sg_assign_page(struct scatterlist *sg, struct page *page)
{
	unsigned long page_link = sg->page_link & 0x3;

	BUG_ON((unsigned long) page & 0x03);
#ifdef CONFIG_DEBUG_SG
	BUG_ON(sg->sg_magic != SG_MAGIC);
	BUG_ON(sg_is_chain(sg));
#endif
	sg->page_link = page_link | (unsigned long) page;
}

static inline void sg_set_page(struct scatterlist *sg, struct page *page,
			       unsigned int len, unsigned int offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

static inline struct page *sg_page(struct scatterlist *sg)
{
#ifdef CONFIG_DEBUG_SG
	BUG_ON(sg->sg_magic != SG_MAGIC);
	BUG_ON(sg_is_chain(sg));
#endif
	return (struct page *)((sg)->page_link & ~0x3);
}

static inline void sg_set_buf(struct scatterlist *sg, const void *buf,
			      unsigned int buflen)
{
#ifdef CONFIG_DEBUG_SG
	BUG_ON(!virt_addr_valid(buf));
#endif
	sg_set_page(sg, virt_to_page(buf), buflen, offset_in_page(buf));
}

#define for_each_sg(sglist, sg, nr, __i)	\
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

static inline void sg_chain(struct scatterlist *prv, unsigned int prv_nents,
			    struct scatterlist *sgl)
{
#ifndef ARCH_HAS_SG_CHAIN
	BUG();
#endif

	prv[prv_nents - 1].offset = 0;
	prv[prv_nents - 1].length = 0;

	prv[prv_nents - 1].page_link = ((unsigned long) sgl | 0x01) & ~0x02;
}

static inline void sg_mark_end(struct scatterlist *sg)
{
#ifdef CONFIG_DEBUG_SG
	BUG_ON(sg->sg_magic != SG_MAGIC);
#endif
	 
	sg->page_link |= 0x02;
	sg->page_link &= ~0x01;
}

static inline void sg_unmark_end(struct scatterlist *sg)
{
#ifdef CONFIG_DEBUG_SG
	BUG_ON(sg->sg_magic != SG_MAGIC);
#endif
	sg->page_link &= ~0x02;
}

static inline dma_addr_t sg_phys(struct scatterlist *sg)
{
	return page_to_phys(sg_page(sg)) + sg->offset;
}

static inline void *sg_virt(struct scatterlist *sg)
{
	return page_address(sg_page(sg)) + sg->offset;
}

int sg_nents(struct scatterlist *sg);
struct scatterlist *sg_next(struct scatterlist *);
struct scatterlist *sg_last(struct scatterlist *s, unsigned int);
void sg_init_table(struct scatterlist *, unsigned int);
void sg_init_one(struct scatterlist *, const void *, unsigned int);

typedef struct scatterlist *(sg_alloc_fn)(unsigned int, gfp_t);
typedef void (sg_free_fn)(struct scatterlist *, unsigned int);

void __sg_free_table(struct sg_table *, unsigned int, sg_free_fn *);
void sg_free_table(struct sg_table *);
int __sg_alloc_table(struct sg_table *, unsigned int, unsigned int, gfp_t,
		     sg_alloc_fn *);
int sg_alloc_table(struct sg_table *, unsigned int, gfp_t);
int sg_alloc_table_from_pages(struct sg_table *sgt,
	struct page **pages, unsigned int n_pages,
	unsigned long offset, unsigned long size,
	gfp_t gfp_mask);

size_t sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
			   void *buf, size_t buflen);
size_t sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents,
			 void *buf, size_t buflen);

#if defined(MY_DEF_HERE)
size_t sg_pcopy_from_buffer(struct scatterlist *sgl, unsigned int nents,
			    void *buf, size_t buflen, off_t skip);
size_t sg_pcopy_to_buffer(struct scatterlist *sgl, unsigned int nents,
			  void *buf, size_t buflen, off_t skip);
#endif  

#define SG_MAX_SINGLE_ALLOC		(PAGE_SIZE / sizeof(struct scatterlist))

struct sg_page_iter {
	struct scatterlist	*sg;		 
	unsigned int		sg_pgoffset;	 

	unsigned int		__nents;	 
	int			__pg_advance;	 
};

bool __sg_page_iter_next(struct sg_page_iter *piter);
void __sg_page_iter_start(struct sg_page_iter *piter,
			  struct scatterlist *sglist, unsigned int nents,
			  unsigned long pgoffset);
 
static inline struct page *sg_page_iter_page(struct sg_page_iter *piter)
{
	return nth_page(sg_page(piter->sg), piter->sg_pgoffset);
}

static inline dma_addr_t sg_page_iter_dma_address(struct sg_page_iter *piter)
{
	return sg_dma_address(piter->sg) + (piter->sg_pgoffset << PAGE_SHIFT);
}

#define for_each_sg_page(sglist, piter, nents, pgoffset)		   \
	for (__sg_page_iter_start((piter), (sglist), (nents), (pgoffset)); \
	     __sg_page_iter_next(piter);)

#define SG_MITER_ATOMIC		(1 << 0)	  
#define SG_MITER_TO_SG		(1 << 1)	 
#define SG_MITER_FROM_SG	(1 << 2)	 

struct sg_mapping_iter {
	 
	struct page		*page;		 
	void			*addr;		 
	size_t			length;		 
	size_t			consumed;	 
	struct sg_page_iter	piter;		 

	unsigned int		__offset;	 
	unsigned int		__remaining;	 
	unsigned int		__flags;
};

void sg_miter_start(struct sg_mapping_iter *miter, struct scatterlist *sgl,
		    unsigned int nents, unsigned int flags);
bool sg_miter_next(struct sg_mapping_iter *miter);
void sg_miter_stop(struct sg_mapping_iter *miter);

#endif  
