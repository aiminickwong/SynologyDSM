#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#ifndef _LINUX_RADIX_TREE_H
#define _LINUX_RADIX_TREE_H

#include <linux/preempt.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>

#ifdef CONFIG_LFS_ON_32CPU
 
#define rdx_t	unsigned long long
#define RDX_TREE_KEY_MAX_VALUE	ULLONG_MAX
#else  
#if defined(MY_DEF_HERE)
#define rdx_t	unsigned long
#define RDX_TREE_KEY_MAX_VALUE	ULONG_MAX
#endif  
#endif  
 
#define RADIX_TREE_INDIRECT_PTR		1
 
#define RADIX_TREE_EXCEPTIONAL_ENTRY	2
#define RADIX_TREE_EXCEPTIONAL_SHIFT	2

static inline int radix_tree_is_indirect_ptr(void *ptr)
{
	return (int)((unsigned long)ptr & RADIX_TREE_INDIRECT_PTR);
}

#define RADIX_TREE_MAX_TAGS 3

struct radix_tree_root {
	unsigned int		height;
	gfp_t			gfp_mask;
	struct radix_tree_node	__rcu *rnode;
};

#define RADIX_TREE_INIT(mask)	{					\
	.height = 0,							\
	.gfp_mask = (mask),						\
	.rnode = NULL,							\
}

#define RADIX_TREE(name, mask) \
	struct radix_tree_root name = RADIX_TREE_INIT(mask)

#define INIT_RADIX_TREE(root, mask)					\
do {									\
	(root)->height = 0;						\
	(root)->gfp_mask = (mask);					\
	(root)->rnode = NULL;						\
} while (0)

static inline void *radix_tree_deref_slot(void **pslot)
{
	return rcu_dereference(*pslot);
}

static inline void *radix_tree_deref_slot_protected(void **pslot,
							spinlock_t *treelock)
{
	return rcu_dereference_protected(*pslot, lockdep_is_held(treelock));
}

static inline int radix_tree_deref_retry(void *arg)
{
	return unlikely((unsigned long)arg & RADIX_TREE_INDIRECT_PTR);
}

static inline int radix_tree_exceptional_entry(void *arg)
{
	 
	return (unsigned long)arg & RADIX_TREE_EXCEPTIONAL_ENTRY;
}

static inline int radix_tree_exception(void *arg)
{
	return unlikely((unsigned long)arg &
		(RADIX_TREE_INDIRECT_PTR | RADIX_TREE_EXCEPTIONAL_ENTRY));
}

static inline void radix_tree_replace_slot(void **pslot, void *item)
{
	BUG_ON(radix_tree_is_indirect_ptr(item));
	rcu_assign_pointer(*pslot, item);
}

#ifdef MY_DEF_HERE
int radix_tree_insert(struct radix_tree_root *, rdx_t, void *);
void *radix_tree_lookup(struct radix_tree_root *, rdx_t);
void **radix_tree_lookup_slot(struct radix_tree_root *, rdx_t);
void *radix_tree_delete(struct radix_tree_root *, rdx_t);
unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
			rdx_t first_index, unsigned int max_items);
unsigned int radix_tree_gang_lookup_slot(struct radix_tree_root *root,
			void ***results, rdx_t *indices,
			rdx_t first_index, unsigned int max_items);
rdx_t radix_tree_next_hole(struct radix_tree_root *root,
				rdx_t index, rdx_t max_scan);
rdx_t radix_tree_prev_hole(struct radix_tree_root *root,
				rdx_t index, rdx_t max_scan);
int radix_tree_preload(gfp_t gfp_mask);
void radix_tree_init(void);
void *radix_tree_tag_set(struct radix_tree_root *root,
			rdx_t index, unsigned int tag);
void *radix_tree_tag_clear(struct radix_tree_root *root,
			rdx_t index, unsigned int tag);
int radix_tree_tag_get(struct radix_tree_root *root,
			rdx_t index, unsigned int tag);
unsigned int
radix_tree_gang_lookup_tag(struct radix_tree_root *root, void **results,
		rdx_t first_index, unsigned int max_items,
		unsigned int tag);
unsigned int
radix_tree_gang_lookup_tag_slot(struct radix_tree_root *root, void ***results,
		rdx_t first_index, unsigned int max_items,
		unsigned int tag);
unsigned long radix_tree_range_tag_if_tagged(struct radix_tree_root *root,
		rdx_t *first_indexp, rdx_t last_index,
		unsigned long nr_to_tag,
		unsigned int fromtag, unsigned int totag);
int radix_tree_tagged(struct radix_tree_root *root, unsigned int tag);
rdx_t radix_tree_locate_item(struct radix_tree_root *root, void *item);
#else  
int radix_tree_insert(struct radix_tree_root *, unsigned long, void *);
void *radix_tree_lookup(struct radix_tree_root *, unsigned long);
void **radix_tree_lookup_slot(struct radix_tree_root *, unsigned long);
void *radix_tree_delete(struct radix_tree_root *, unsigned long);
unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
			unsigned long first_index, unsigned int max_items);
unsigned int radix_tree_gang_lookup_slot(struct radix_tree_root *root,
			void ***results, unsigned long *indices,
			unsigned long first_index, unsigned int max_items);
unsigned long radix_tree_next_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan);
unsigned long radix_tree_prev_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan);
int radix_tree_preload(gfp_t gfp_mask);
void radix_tree_init(void);
void *radix_tree_tag_set(struct radix_tree_root *root,
			unsigned long index, unsigned int tag);
void *radix_tree_tag_clear(struct radix_tree_root *root,
			unsigned long index, unsigned int tag);
int radix_tree_tag_get(struct radix_tree_root *root,
			unsigned long index, unsigned int tag);
unsigned int
radix_tree_gang_lookup_tag(struct radix_tree_root *root, void **results,
		unsigned long first_index, unsigned int max_items,
		unsigned int tag);
unsigned int
radix_tree_gang_lookup_tag_slot(struct radix_tree_root *root, void ***results,
		unsigned long first_index, unsigned int max_items,
		unsigned int tag);
unsigned long radix_tree_range_tag_if_tagged(struct radix_tree_root *root,
		unsigned long *first_indexp, unsigned long last_index,
		unsigned long nr_to_tag,
		unsigned int fromtag, unsigned int totag);
int radix_tree_tagged(struct radix_tree_root *root, unsigned int tag);
unsigned long radix_tree_locate_item(struct radix_tree_root *root, void *item);
#endif  

static inline void radix_tree_preload_end(void)
{
	preempt_enable();
}

struct radix_tree_iter {
#ifdef MY_DEF_HERE
	rdx_t	index;
	rdx_t	next_index;
#else  
	unsigned long	index;
	unsigned long	next_index;
#endif  
	unsigned long	tags;
};

#define RADIX_TREE_ITER_TAG_MASK	0x00FF	 
#define RADIX_TREE_ITER_TAGGED		0x0100	 
#define RADIX_TREE_ITER_CONTIG		0x0200	 

static __always_inline void **
#ifdef MY_DEF_HERE
radix_tree_iter_init(struct radix_tree_iter *iter, rdx_t start)
#else  
radix_tree_iter_init(struct radix_tree_iter *iter, unsigned long start)
#endif  
{
	 
	iter->index = 0;
	iter->next_index = start;
	return NULL;
}

void **radix_tree_next_chunk(struct radix_tree_root *root,
			     struct radix_tree_iter *iter, unsigned flags);

static inline __must_check
void **radix_tree_iter_retry(struct radix_tree_iter *iter)
{
	iter->next_index = iter->index;
	return NULL;
}

static __always_inline long
radix_tree_chunk_size(struct radix_tree_iter *iter)
{
	return iter->next_index - iter->index;
}

static __always_inline void **
radix_tree_next_slot(void **slot, struct radix_tree_iter *iter, unsigned flags)
{
	if (flags & RADIX_TREE_ITER_TAGGED) {
		iter->tags >>= 1;
		if (likely(iter->tags & 1ul)) {
			iter->index++;
			return slot + 1;
		}
		if (!(flags & RADIX_TREE_ITER_CONTIG) && likely(iter->tags)) {
			unsigned offset = __ffs(iter->tags);

			iter->tags >>= offset;
			iter->index += offset + 1;
			return slot + offset + 1;
		}
	} else {
		long size = radix_tree_chunk_size(iter);

		while (--size > 0) {
			slot++;
			iter->index++;
			if (likely(*slot))
				return slot;
			if (flags & RADIX_TREE_ITER_CONTIG) {
				 
				iter->next_index = 0;
				break;
			}
		}
	}
	return NULL;
}

#define radix_tree_for_each_chunk(slot, root, iter, start, flags)	\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	      (slot = radix_tree_next_chunk(root, iter, flags)) ;)

#define radix_tree_for_each_chunk_slot(slot, iter, flags)		\
	for (; slot ; slot = radix_tree_next_slot(slot, iter, flags))

#define radix_tree_for_each_slot(slot, root, iter, start)		\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter, 0)) ;	\
	     slot = radix_tree_next_slot(slot, iter, 0))

#define radix_tree_for_each_contig(slot, root, iter, start)		\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter,		\
				RADIX_TREE_ITER_CONTIG)) ;		\
	     slot = radix_tree_next_slot(slot, iter,			\
				RADIX_TREE_ITER_CONTIG))

#define radix_tree_for_each_tagged(slot, root, iter, start, tag)	\
	for (slot = radix_tree_iter_init(iter, start) ;			\
	     slot || (slot = radix_tree_next_chunk(root, iter,		\
			      RADIX_TREE_ITER_TAGGED | tag)) ;		\
	     slot = radix_tree_next_slot(slot, iter,			\
				RADIX_TREE_ITER_TAGGED))

#endif  
