#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/radix-tree.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/rcupdate.h>

#ifdef CONFIG_LFS_ON_32CPU
#define RADIX_TREE_1	1ULL
#define RADIX_TREE_BITS_PER_KEY		64
#else  
#if defined(MY_DEF_HERE)
#define RADIX_TREE_1	1UL
#define RADIX_TREE_BITS_PER_KEY		BITS_PER_LONG
#endif  
#endif  

#ifdef __KERNEL__
#define RADIX_TREE_MAP_SHIFT	(CONFIG_BASE_SMALL ? 4 : 6)
#else
#define RADIX_TREE_MAP_SHIFT	3	 
#endif

#ifdef MY_DEF_HERE
#define RADIX_TREE_MAP_SIZE	(RADIX_TREE_1 << RADIX_TREE_MAP_SHIFT)
#else  
#define RADIX_TREE_MAP_SIZE	(1UL << RADIX_TREE_MAP_SHIFT)
#endif  
#define RADIX_TREE_MAP_MASK	(RADIX_TREE_MAP_SIZE-1)

#define RADIX_TREE_TAG_LONGS	\
	((RADIX_TREE_MAP_SIZE + BITS_PER_LONG - 1) / BITS_PER_LONG)

struct radix_tree_node {
	unsigned int	height;		 
	unsigned int	count;
	union {
		struct radix_tree_node *parent;	 
		struct rcu_head	rcu_head;	 
	};
	void __rcu	*slots[RADIX_TREE_MAP_SIZE];
	unsigned long	tags[RADIX_TREE_MAX_TAGS][RADIX_TREE_TAG_LONGS];
};

#ifdef MY_DEF_HERE
#define RADIX_TREE_INDEX_BITS  (8   * sizeof(rdx_t))
#else  
#define RADIX_TREE_INDEX_BITS  (8   * sizeof(unsigned long))
#endif  
#define RADIX_TREE_MAX_PATH (DIV_ROUND_UP(RADIX_TREE_INDEX_BITS, \
					  RADIX_TREE_MAP_SHIFT))

#ifdef MY_DEF_HERE
static rdx_t height_to_maxindex[RADIX_TREE_MAX_PATH + 1] __read_mostly;
#else  
static unsigned long height_to_maxindex[RADIX_TREE_MAX_PATH + 1] __read_mostly;
#endif  

static struct kmem_cache *radix_tree_node_cachep;

#ifdef MY_DEF_HERE
 
#else  
 
#endif  
#define RADIX_TREE_PRELOAD_SIZE (RADIX_TREE_MAX_PATH * 2 - 1)

struct radix_tree_preload {
	int nr;
	struct radix_tree_node *nodes[RADIX_TREE_PRELOAD_SIZE];
};
static DEFINE_PER_CPU(struct radix_tree_preload, radix_tree_preloads) = { 0, };

static inline void *ptr_to_indirect(void *ptr)
{
	return (void *)((unsigned long)ptr | RADIX_TREE_INDIRECT_PTR);
}

static inline void *indirect_to_ptr(void *ptr)
{
	return (void *)((unsigned long)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

static inline gfp_t root_gfp_mask(struct radix_tree_root *root)
{
	return root->gfp_mask & __GFP_BITS_MASK;
}

static inline void tag_set(struct radix_tree_node *node, unsigned int tag,
		int offset)
{
	__set_bit(offset, node->tags[tag]);
}

static inline void tag_clear(struct radix_tree_node *node, unsigned int tag,
		int offset)
{
	__clear_bit(offset, node->tags[tag]);
}

static inline int tag_get(struct radix_tree_node *node, unsigned int tag,
		int offset)
{
	return test_bit(offset, node->tags[tag]);
}

static inline void root_tag_set(struct radix_tree_root *root, unsigned int tag)
{
	root->gfp_mask |= (__force gfp_t)(1 << (tag + __GFP_BITS_SHIFT));
}

static inline void root_tag_clear(struct radix_tree_root *root, unsigned int tag)
{
	root->gfp_mask &= (__force gfp_t)~(1 << (tag + __GFP_BITS_SHIFT));
}

static inline void root_tag_clear_all(struct radix_tree_root *root)
{
	root->gfp_mask &= __GFP_BITS_MASK;
}

static inline int root_tag_get(struct radix_tree_root *root, unsigned int tag)
{
	return (__force unsigned)root->gfp_mask & (1 << (tag + __GFP_BITS_SHIFT));
}

static inline int any_tag_set(struct radix_tree_node *node, unsigned int tag)
{
	int idx;
	for (idx = 0; idx < RADIX_TREE_TAG_LONGS; idx++) {
		if (node->tags[tag][idx])
			return 1;
	}
	return 0;
}

static __always_inline unsigned long
radix_tree_find_next_bit(const unsigned long *addr,
			 unsigned long size, unsigned long offset)
{
	if (!__builtin_constant_p(size))
		return find_next_bit(addr, size, offset);

	if (offset < size) {
		unsigned long tmp;

		addr += offset / BITS_PER_LONG;
		tmp = *addr >> (offset % BITS_PER_LONG);
		if (tmp)
			return __ffs(tmp) + offset;
		offset = (offset + BITS_PER_LONG) & ~(BITS_PER_LONG - 1);
		while (offset < size) {
			tmp = *++addr;
			if (tmp)
				return __ffs(tmp) + offset;
			offset += BITS_PER_LONG;
		}
	}
	return size;
}

static struct radix_tree_node *
radix_tree_node_alloc(struct radix_tree_root *root)
{
	struct radix_tree_node *ret = NULL;
	gfp_t gfp_mask = root_gfp_mask(root);

	if (!(gfp_mask & __GFP_WAIT)) {
		struct radix_tree_preload *rtp;

		rtp = &__get_cpu_var(radix_tree_preloads);
		if (rtp->nr) {
			ret = rtp->nodes[rtp->nr - 1];
			rtp->nodes[rtp->nr - 1] = NULL;
			rtp->nr--;
		}
	}
	if (ret == NULL)
		ret = kmem_cache_alloc(radix_tree_node_cachep, gfp_mask);

	BUG_ON(radix_tree_is_indirect_ptr(ret));
	return ret;
}

static void radix_tree_node_rcu_free(struct rcu_head *head)
{
	struct radix_tree_node *node =
			container_of(head, struct radix_tree_node, rcu_head);
	int i;

	for (i = 0; i < RADIX_TREE_MAX_TAGS; i++)
		tag_clear(node, i, 0);

	node->slots[0] = NULL;
	node->count = 0;

	kmem_cache_free(radix_tree_node_cachep, node);
}

static inline void
radix_tree_node_free(struct radix_tree_node *node)
{
	call_rcu(&node->rcu_head, radix_tree_node_rcu_free);
}

int radix_tree_preload(gfp_t gfp_mask)
{
	struct radix_tree_preload *rtp;
	struct radix_tree_node *node;
	int ret = -ENOMEM;

	preempt_disable();
	rtp = &__get_cpu_var(radix_tree_preloads);
	while (rtp->nr < ARRAY_SIZE(rtp->nodes)) {
		preempt_enable();
		node = kmem_cache_alloc(radix_tree_node_cachep, gfp_mask);
		if (node == NULL)
			goto out;
		preempt_disable();
		rtp = &__get_cpu_var(radix_tree_preloads);
		if (rtp->nr < ARRAY_SIZE(rtp->nodes))
			rtp->nodes[rtp->nr++] = node;
		else
			kmem_cache_free(radix_tree_node_cachep, node);
	}
	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL(radix_tree_preload);

#ifdef MY_DEF_HERE
static inline rdx_t radix_tree_maxindex(unsigned int height)
#else  
static inline unsigned long radix_tree_maxindex(unsigned int height)
#endif  
{
	return height_to_maxindex[height];
}

#ifdef MY_DEF_HERE
static int radix_tree_extend(struct radix_tree_root *root, rdx_t index)
#else  
static int radix_tree_extend(struct radix_tree_root *root, unsigned long index)
#endif  
{
	struct radix_tree_node *node;
	struct radix_tree_node *slot;
	unsigned int height;
	int tag;

	height = root->height + 1;
	while (index > radix_tree_maxindex(height))
		height++;

	if (root->rnode == NULL) {
		root->height = height;
		goto out;
	}

	do {
		unsigned int newheight;
		if (!(node = radix_tree_node_alloc(root)))
			return -ENOMEM;

		for (tag = 0; tag < RADIX_TREE_MAX_TAGS; tag++) {
			if (root_tag_get(root, tag))
				tag_set(node, tag, 0);
		}

		newheight = root->height+1;
		node->height = newheight;
		node->count = 1;
		node->parent = NULL;
		slot = root->rnode;
		if (newheight > 1) {
			slot = indirect_to_ptr(slot);
			slot->parent = node;
		}
		node->slots[0] = slot;
		node = ptr_to_indirect(node);
		rcu_assign_pointer(root->rnode, node);
		root->height = newheight;
	} while (height > root->height);
out:
	return 0;
}

int radix_tree_insert(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
			rdx_t index, void *item)
#else  
			unsigned long index, void *item)
#endif  
{
	struct radix_tree_node *node = NULL, *slot;
	unsigned int height, shift;
	int offset;
	int error;

	BUG_ON(radix_tree_is_indirect_ptr(item));

	if (index > radix_tree_maxindex(root->height)) {
		error = radix_tree_extend(root, index);
		if (error)
			return error;
	}

	slot = indirect_to_ptr(root->rnode);

	height = root->height;
	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	offset = 0;			 
	while (height > 0) {
		if (slot == NULL) {
			 
			if (!(slot = radix_tree_node_alloc(root)))
				return -ENOMEM;
			slot->height = height;
			slot->parent = node;
			if (node) {
				rcu_assign_pointer(node->slots[offset], slot);
				node->count++;
			} else
				rcu_assign_pointer(root->rnode, ptr_to_indirect(slot));
		}

		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = node->slots[offset];
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (slot != NULL)
		return -EEXIST;

	if (node) {
		node->count++;
		rcu_assign_pointer(node->slots[offset], item);
		BUG_ON(tag_get(node, 0, offset));
		BUG_ON(tag_get(node, 1, offset));
	} else {
		rcu_assign_pointer(root->rnode, item);
		BUG_ON(root_tag_get(root, 0));
		BUG_ON(root_tag_get(root, 1));
	}

	return 0;
}
EXPORT_SYMBOL(radix_tree_insert);

static void *radix_tree_lookup_element(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
				rdx_t index, int is_slot)
#else  
				unsigned long index, int is_slot)
#endif  
{
	unsigned int height, shift;
	struct radix_tree_node *node, **slot;

	node = rcu_dereference_raw(root->rnode);
	if (node == NULL)
		return NULL;

	if (!radix_tree_is_indirect_ptr(node)) {
		if (index > 0)
			return NULL;
		return is_slot ? (void *)&root->rnode : node;
	}
	node = indirect_to_ptr(node);

	height = node->height;
	if (index > radix_tree_maxindex(height))
		return NULL;

	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	do {
		slot = (struct radix_tree_node **)
			(node->slots + ((index>>shift) & RADIX_TREE_MAP_MASK));
		node = rcu_dereference_raw(*slot);
		if (node == NULL)
			return NULL;

		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	} while (height > 0);

	return is_slot ? (void *)slot : indirect_to_ptr(node);
}

#ifdef MY_DEF_HERE
void **radix_tree_lookup_slot(struct radix_tree_root *root, rdx_t index)
#else  
void **radix_tree_lookup_slot(struct radix_tree_root *root, unsigned long index)
#endif  
{
	return (void **)radix_tree_lookup_element(root, index, 1);
}
EXPORT_SYMBOL(radix_tree_lookup_slot);

#ifdef MY_DEF_HERE
void *radix_tree_lookup(struct radix_tree_root *root, rdx_t index)
#else  
void *radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
#endif  
{
	return radix_tree_lookup_element(root, index, 0);
}
EXPORT_SYMBOL(radix_tree_lookup);

void *radix_tree_tag_set(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
			rdx_t index, unsigned int tag)
#else  
			unsigned long index, unsigned int tag)
#endif  
{
	unsigned int height, shift;
	struct radix_tree_node *slot;

	height = root->height;
	BUG_ON(index > radix_tree_maxindex(height));

	slot = indirect_to_ptr(root->rnode);
	shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

	while (height > 0) {
		int offset;

		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		if (!tag_get(slot, tag, offset))
			tag_set(slot, tag, offset);
		slot = slot->slots[offset];
		BUG_ON(slot == NULL);
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}

	if (slot && !root_tag_get(root, tag))
		root_tag_set(root, tag);

	return slot;
}
EXPORT_SYMBOL(radix_tree_tag_set);

void *radix_tree_tag_clear(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
			rdx_t index, unsigned int tag)
#else  
			unsigned long index, unsigned int tag)
#endif  
{
	struct radix_tree_node *node = NULL;
	struct radix_tree_node *slot = NULL;
	unsigned int height, shift;
	int uninitialized_var(offset);

	height = root->height;
	if (index > radix_tree_maxindex(height))
		goto out;

	shift = height * RADIX_TREE_MAP_SHIFT;
	slot = indirect_to_ptr(root->rnode);

	while (shift) {
		if (slot == NULL)
			goto out;

		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = slot->slots[offset];
	}

	if (slot == NULL)
		goto out;

	while (node) {
		if (!tag_get(node, tag, offset))
			goto out;
		tag_clear(node, tag, offset);
		if (any_tag_set(node, tag))
			goto out;

		index >>= RADIX_TREE_MAP_SHIFT;
		offset = index & RADIX_TREE_MAP_MASK;
		node = node->parent;
	}

	if (root_tag_get(root, tag))
		root_tag_clear(root, tag);

out:
	return slot;
}
EXPORT_SYMBOL(radix_tree_tag_clear);

int radix_tree_tag_get(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
			rdx_t index, unsigned int tag)
#else  
			unsigned long index, unsigned int tag)
#endif  
{
	unsigned int height, shift;
	struct radix_tree_node *node;

	if (!root_tag_get(root, tag))
		return 0;

	node = rcu_dereference_raw(root->rnode);
	if (node == NULL)
		return 0;

	if (!radix_tree_is_indirect_ptr(node))
		return (index == 0);
	node = indirect_to_ptr(node);

	height = node->height;
	if (index > radix_tree_maxindex(height))
		return 0;

	shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

	for ( ; ; ) {
		int offset;

		if (node == NULL)
			return 0;

		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		if (!tag_get(node, tag, offset))
			return 0;
		if (height == 1)
			return 1;
		node = rcu_dereference_raw(node->slots[offset]);
		shift -= RADIX_TREE_MAP_SHIFT;
		height--;
	}
}
EXPORT_SYMBOL(radix_tree_tag_get);

void **radix_tree_next_chunk(struct radix_tree_root *root,
			     struct radix_tree_iter *iter, unsigned flags)
{
	unsigned shift, tag = flags & RADIX_TREE_ITER_TAG_MASK;
	struct radix_tree_node *rnode, *node;
#ifdef MY_DEF_HERE
	rdx_t index, offset;
#else  
	unsigned long index, offset;
#endif  

	if ((flags & RADIX_TREE_ITER_TAGGED) && !root_tag_get(root, tag))
		return NULL;

	index = iter->next_index;
	if (!index && iter->index)
		return NULL;

	rnode = rcu_dereference_raw(root->rnode);
	if (radix_tree_is_indirect_ptr(rnode)) {
		rnode = indirect_to_ptr(rnode);
	} else if (rnode && !index) {
		 
		iter->index = 0;
		iter->next_index = 1;
		iter->tags = 1;
		return (void **)&root->rnode;
	} else
		return NULL;

restart:
	shift = (rnode->height - 1) * RADIX_TREE_MAP_SHIFT;
	offset = index >> shift;

	if (offset >= RADIX_TREE_MAP_SIZE)
		return NULL;

	node = rnode;
	while (1) {
		if ((flags & RADIX_TREE_ITER_TAGGED) ?
				!test_bit(offset, node->tags[tag]) :
				!node->slots[offset]) {
			 
			if (flags & RADIX_TREE_ITER_CONTIG)
				return NULL;

			if (flags & RADIX_TREE_ITER_TAGGED)
				offset = radix_tree_find_next_bit(
						node->tags[tag],
						RADIX_TREE_MAP_SIZE,
						offset + 1);
			else
				while (++offset	< RADIX_TREE_MAP_SIZE) {
					if (node->slots[offset])
						break;
				}
			index &= ~((RADIX_TREE_MAP_SIZE << shift) - 1);
			index += offset << shift;
			 
			if (!index)
				return NULL;
			if (offset == RADIX_TREE_MAP_SIZE)
				goto restart;
		}

		if (!shift)
			break;

		node = rcu_dereference_raw(node->slots[offset]);
		if (node == NULL)
			goto restart;
		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
	}

	iter->index = index;
	iter->next_index = (index | RADIX_TREE_MAP_MASK) + 1;

	if (flags & RADIX_TREE_ITER_TAGGED) {
		unsigned tag_long, tag_bit;

		tag_long = offset / BITS_PER_LONG;
		tag_bit  = offset % BITS_PER_LONG;
		iter->tags = node->tags[tag][tag_long] >> tag_bit;
		 
		if (tag_long < RADIX_TREE_TAG_LONGS - 1) {
			 
			if (tag_bit)
				iter->tags |= node->tags[tag][tag_long + 1] <<
						(BITS_PER_LONG - tag_bit);
			 
			iter->next_index = index + BITS_PER_LONG;
		}
	}

	return node->slots + offset;
}
EXPORT_SYMBOL(radix_tree_next_chunk);

#ifdef MY_DEF_HERE
 
#else  
 
#endif  
unsigned long radix_tree_range_tag_if_tagged(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
		rdx_t *first_indexp, rdx_t last_index,
#else  
		unsigned long *first_indexp, unsigned long last_index,
#endif  
		unsigned long nr_to_tag,
		unsigned int iftag, unsigned int settag)
{
	unsigned int height = root->height;
	struct radix_tree_node *node = NULL;
	struct radix_tree_node *slot;
	unsigned int shift;
	unsigned long tagged = 0;
#ifdef MY_DEF_HERE
	rdx_t index = *first_indexp;
#else  
	unsigned long index = *first_indexp;
#endif  

	last_index = min(last_index, radix_tree_maxindex(height));
	if (index > last_index)
		return 0;
	if (!nr_to_tag)
		return 0;
	if (!root_tag_get(root, iftag)) {
		*first_indexp = last_index + 1;
		return 0;
	}
	if (height == 0) {
		*first_indexp = last_index + 1;
		root_tag_set(root, settag);
		return 1;
	}

	shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
	slot = indirect_to_ptr(root->rnode);

	for (;;) {
#ifdef MY_DEF_HERE
		rdx_t upindex;
#else  
		unsigned long upindex;
#endif  
		int offset;

		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		if (!slot->slots[offset])
			goto next;
		if (!tag_get(slot, iftag, offset))
			goto next;
		if (shift) {
			 
			shift -= RADIX_TREE_MAP_SHIFT;
			node = slot;
			slot = slot->slots[offset];
			continue;
		}

		tagged++;
		tag_set(slot, settag, offset);

		upindex = index;
		while (node) {
			upindex >>= RADIX_TREE_MAP_SHIFT;
			offset = upindex & RADIX_TREE_MAP_MASK;

			if (tag_get(node, settag, offset))
				break;
			tag_set(node, settag, offset);
			node = node->parent;
		}

		node = NULL;

next:
		 
		index = ((index >> shift) + 1) << shift;
		 
		if (index > last_index || !index)
			break;
		if (tagged >= nr_to_tag)
			break;
		while (((index >> shift) & RADIX_TREE_MAP_MASK) == 0) {
			 
			slot = slot->parent;
			shift += RADIX_TREE_MAP_SHIFT;
		}
	}
	 
	if (tagged > 0)
		root_tag_set(root, settag);
	*first_indexp = index;

	return tagged;
}
EXPORT_SYMBOL(radix_tree_range_tag_if_tagged);

#ifdef MY_DEF_HERE
rdx_t radix_tree_next_hole(struct radix_tree_root *root,
				rdx_t index, rdx_t max_scan)
{
	rdx_t i;
#else  
unsigned long radix_tree_next_hole(struct radix_tree_root *root,
				unsigned long index, unsigned long max_scan)
{
	unsigned long i;
#endif  

	for (i = 0; i < max_scan; i++) {
		if (!radix_tree_lookup(root, index))
			break;
		index++;
		if (index == 0)
			break;
	}

	return index;
}
EXPORT_SYMBOL(radix_tree_next_hole);

#ifdef MY_DEF_HERE
 
#else  
 
#endif  
 
#ifdef MY_DEF_HERE
rdx_t radix_tree_prev_hole(struct radix_tree_root *root,
				   rdx_t index, rdx_t max_scan)
{
	rdx_t i;
#else  
unsigned long radix_tree_prev_hole(struct radix_tree_root *root,
				   unsigned long index, unsigned long max_scan)
{
	unsigned long i;
#endif  

	for (i = 0; i < max_scan; i++) {
		if (!radix_tree_lookup(root, index))
			break;
		index--;
#ifdef MY_DEF_HERE
		if (index == RDX_TREE_KEY_MAX_VALUE)
#else  
		if (index == ULONG_MAX)
#endif  
			break;
	}

	return index;
}
EXPORT_SYMBOL(radix_tree_prev_hole);

unsigned int
radix_tree_gang_lookup(struct radix_tree_root *root, void **results,
#ifdef MY_DEF_HERE
			rdx_t first_index, unsigned int max_items)
#else  
			unsigned long first_index, unsigned int max_items)
#endif  
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_slot(slot, root, &iter, first_index) {
		results[ret] = rcu_dereference_raw(*slot);
		if (!results[ret])
			continue;
		if (radix_tree_is_indirect_ptr(results[ret])) {
			slot = radix_tree_iter_retry(&iter);
			continue;
		}
		if (++ret == max_items)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(radix_tree_gang_lookup);

unsigned int
radix_tree_gang_lookup_slot(struct radix_tree_root *root,
#ifdef MY_DEF_HERE
			void ***results, rdx_t *indices,
			rdx_t first_index, unsigned int max_items)
#else  
			void ***results, unsigned long *indices,
			unsigned long first_index, unsigned int max_items)
#endif  
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_slot(slot, root, &iter, first_index) {
		results[ret] = slot;
		if (indices)
			indices[ret] = iter.index;
		if (++ret == max_items)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(radix_tree_gang_lookup_slot);

unsigned int
radix_tree_gang_lookup_tag(struct radix_tree_root *root, void **results,
#ifdef MY_DEF_HERE
		rdx_t first_index, unsigned int max_items,
#else  
		unsigned long first_index, unsigned int max_items,
#endif  
		unsigned int tag)
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_tagged(slot, root, &iter, first_index, tag) {
		results[ret] = rcu_dereference_raw(*slot);
		if (!results[ret])
			continue;
		if (radix_tree_is_indirect_ptr(results[ret])) {
			slot = radix_tree_iter_retry(&iter);
			continue;
		}
		if (++ret == max_items)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(radix_tree_gang_lookup_tag);

unsigned int
radix_tree_gang_lookup_tag_slot(struct radix_tree_root *root, void ***results,
#ifdef MY_DEF_HERE
		rdx_t first_index, unsigned int max_items,
#else  
		unsigned long first_index, unsigned int max_items,
#endif  
		unsigned int tag)
{
	struct radix_tree_iter iter;
	void **slot;
	unsigned int ret = 0;

	if (unlikely(!max_items))
		return 0;

	radix_tree_for_each_tagged(slot, root, &iter, first_index, tag) {
		results[ret] = slot;
		if (++ret == max_items)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(radix_tree_gang_lookup_tag_slot);

#if defined(CONFIG_SHMEM) && defined(CONFIG_SWAP)
#include <linux/sched.h>  

#ifdef MY_DEF_HERE
static rdx_t __locate(struct radix_tree_node *slot, void *item,
			      rdx_t index, rdx_t *found_index)
#else  
static unsigned long __locate(struct radix_tree_node *slot, void *item,
			      unsigned long index, unsigned long *found_index)
#endif  
{
	unsigned int shift, height;
#ifdef MY_DEF_HERE
	rdx_t i;
#else  
	unsigned long i;
#endif  

	height = slot->height;
	shift = (height-1) * RADIX_TREE_MAP_SHIFT;

	for ( ; height > 1; height--) {
		i = (index >> shift) & RADIX_TREE_MAP_MASK;
		for (;;) {
			if (slot->slots[i] != NULL)
				break;
#ifdef MY_DEF_HERE
			index &= ~((RADIX_TREE_1 << shift) - 1);
			index += RADIX_TREE_1 << shift;
#else  
			index &= ~((1UL << shift) - 1);
			index += 1UL << shift;
#endif  
			if (index == 0)
				goto out;	 
			i++;
			if (i == RADIX_TREE_MAP_SIZE)
				goto out;
		}

		shift -= RADIX_TREE_MAP_SHIFT;
		slot = rcu_dereference_raw(slot->slots[i]);
		if (slot == NULL)
			goto out;
	}

	for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
		if (slot->slots[i] == item) {
			*found_index = index + i;
			index = 0;
			goto out;
		}
	}
	index += RADIX_TREE_MAP_SIZE;
out:
	return index;
}

#ifdef MY_DEF_HERE
rdx_t radix_tree_locate_item(struct radix_tree_root *root, void *item)
#else  
unsigned long radix_tree_locate_item(struct radix_tree_root *root, void *item)
#endif  
{
	struct radix_tree_node *node;
#ifdef MY_DEF_HERE
	rdx_t max_index;
	rdx_t cur_index = 0;
	rdx_t found_index = -1;
#else  
	unsigned long max_index;
	unsigned long cur_index = 0;
	unsigned long found_index = -1;
#endif  

	do {
		rcu_read_lock();
		node = rcu_dereference_raw(root->rnode);
		if (!radix_tree_is_indirect_ptr(node)) {
			rcu_read_unlock();
			if (node == item)
				found_index = 0;
			break;
		}

		node = indirect_to_ptr(node);
		max_index = radix_tree_maxindex(node->height);
		if (cur_index > max_index)
			break;

		cur_index = __locate(node, item, cur_index, &found_index);
		rcu_read_unlock();
		cond_resched();
	} while (cur_index != 0 && cur_index <= max_index);

	return found_index;
}
#else
#ifdef MY_DEF_HERE
rdx_t radix_tree_locate_item(struct radix_tree_root *root, void *item)
#else  
unsigned long radix_tree_locate_item(struct radix_tree_root *root, void *item)
#endif  
{
	return -1;
}
#endif  

static inline void radix_tree_shrink(struct radix_tree_root *root)
{
	 
	while (root->height > 0) {
		struct radix_tree_node *to_free = root->rnode;
		struct radix_tree_node *slot;

		BUG_ON(!radix_tree_is_indirect_ptr(to_free));
		to_free = indirect_to_ptr(to_free);

		if (to_free->count != 1)
			break;
		if (!to_free->slots[0])
			break;

		slot = to_free->slots[0];
		if (root->height > 1) {
			slot->parent = NULL;
			slot = ptr_to_indirect(slot);
		}
		root->rnode = slot;
		root->height--;

		if (root->height == 0)
			*((unsigned long *)&to_free->slots[0]) |=
						RADIX_TREE_INDIRECT_PTR;

		radix_tree_node_free(to_free);
	}
}

#ifdef MY_DEF_HERE
void *radix_tree_delete(struct radix_tree_root *root, rdx_t index)
#else  
void *radix_tree_delete(struct radix_tree_root *root, unsigned long index)
#endif  
{
	struct radix_tree_node *node = NULL;
	struct radix_tree_node *slot = NULL;
	struct radix_tree_node *to_free;
	unsigned int height, shift;
	int tag;
	int uninitialized_var(offset);

	height = root->height;
	if (index > radix_tree_maxindex(height))
		goto out;

	slot = root->rnode;
	if (height == 0) {
		root_tag_clear_all(root);
		root->rnode = NULL;
		goto out;
	}
	slot = indirect_to_ptr(slot);
	shift = height * RADIX_TREE_MAP_SHIFT;

	do {
		if (slot == NULL)
			goto out;

		shift -= RADIX_TREE_MAP_SHIFT;
		offset = (index >> shift) & RADIX_TREE_MAP_MASK;
		node = slot;
		slot = slot->slots[offset];
	} while (shift);

	if (slot == NULL)
		goto out;

	for (tag = 0; tag < RADIX_TREE_MAX_TAGS; tag++) {
		if (tag_get(node, tag, offset))
			radix_tree_tag_clear(root, index, tag);
	}

	to_free = NULL;
	 
	while (node) {
		node->slots[offset] = NULL;
		node->count--;
		 
		if (to_free)
			radix_tree_node_free(to_free);

		if (node->count) {
			if (node == indirect_to_ptr(root->rnode))
				radix_tree_shrink(root);
			goto out;
		}

		to_free = node;

		index >>= RADIX_TREE_MAP_SHIFT;
		offset = index & RADIX_TREE_MAP_MASK;
		node = node->parent;
	}

	root_tag_clear_all(root);
	root->height = 0;
	root->rnode = NULL;
	if (to_free)
		radix_tree_node_free(to_free);

out:
	return slot;
}
EXPORT_SYMBOL(radix_tree_delete);

int radix_tree_tagged(struct radix_tree_root *root, unsigned int tag)
{
	return root_tag_get(root, tag);
}
EXPORT_SYMBOL(radix_tree_tagged);

static void
radix_tree_node_ctor(void *node)
{
	memset(node, 0, sizeof(struct radix_tree_node));
}

#ifdef MY_DEF_HERE
static __init rdx_t __maxindex(unsigned int height)
#else  
static __init unsigned long __maxindex(unsigned int height)
#endif  
{
	unsigned int width = height * RADIX_TREE_MAP_SHIFT;
	int shift = RADIX_TREE_INDEX_BITS - width;

#ifdef MY_DEF_HERE
	if (shift < 0)
		return RDX_TREE_KEY_MAX_VALUE;
	if (shift >= RADIX_TREE_BITS_PER_KEY)
		return (rdx_t)0;
	return RDX_TREE_KEY_MAX_VALUE >> shift;
#else  
	if (shift < 0)
		return ~0UL;
	if (shift >= BITS_PER_LONG)
		return 0UL;
	return ~0UL >> shift;
#endif  
}

static __init void radix_tree_init_maxindex(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(height_to_maxindex); i++)
		height_to_maxindex[i] = __maxindex(i);
}

static int radix_tree_callback(struct notifier_block *nfb,
                            unsigned long action,
                            void *hcpu)
{
       int cpu = (long)hcpu;
       struct radix_tree_preload *rtp;

       if (action == CPU_DEAD || action == CPU_DEAD_FROZEN) {
               rtp = &per_cpu(radix_tree_preloads, cpu);
               while (rtp->nr) {
                       kmem_cache_free(radix_tree_node_cachep,
                                       rtp->nodes[rtp->nr-1]);
                       rtp->nodes[rtp->nr-1] = NULL;
                       rtp->nr--;
               }
       }
       return NOTIFY_OK;
}

void __init radix_tree_init(void)
{
	radix_tree_node_cachep = kmem_cache_create("radix_tree_node",
			sizeof(struct radix_tree_node), 0,
			SLAB_PANIC | SLAB_RECLAIM_ACCOUNT,
			radix_tree_node_ctor);
	radix_tree_init_maxindex();
	hotcpu_notifier(radix_tree_callback, 0);
}
