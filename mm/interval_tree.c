#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/rmap.h>
#include <linux/interval_tree_generic.h>

#ifdef MY_DEF_HERE
static inline pgoff_t vma_start_pgoff(struct vm_area_struct *v)
#else  
static inline unsigned long vma_start_pgoff(struct vm_area_struct *v)
#endif  
{
	return v->vm_pgoff;
}

#ifdef MY_DEF_HERE
static inline pgoff_t vma_last_pgoff(struct vm_area_struct *v)
#else  
static inline unsigned long vma_last_pgoff(struct vm_area_struct *v)
#endif  
{
	return v->vm_pgoff + ((v->vm_end - v->vm_start) >> PAGE_SHIFT) - 1;
}

INTERVAL_TREE_DEFINE(struct vm_area_struct, shared.linear.rb,
#ifdef MY_DEF_HERE
		     pgoff_t, shared.linear.rb_subtree_last,
#else  
		     unsigned long, shared.linear.rb_subtree_last,
#endif  
		     vma_start_pgoff, vma_last_pgoff,, vma_interval_tree)

void vma_interval_tree_insert_after(struct vm_area_struct *node,
				    struct vm_area_struct *prev,
				    struct rb_root *root)
{
	struct rb_node **link;
	struct vm_area_struct *parent;
#ifdef MY_DEF_HERE
	pgoff_t last = vma_last_pgoff(node);
#else  
	unsigned long last = vma_last_pgoff(node);
#endif  

	VM_BUG_ON(vma_start_pgoff(node) != vma_start_pgoff(prev));

	if (!prev->shared.linear.rb.rb_right) {
		parent = prev;
		link = &prev->shared.linear.rb.rb_right;
	} else {
		parent = rb_entry(prev->shared.linear.rb.rb_right,
				  struct vm_area_struct, shared.linear.rb);
		if (parent->shared.linear.rb_subtree_last < last)
			parent->shared.linear.rb_subtree_last = last;
		while (parent->shared.linear.rb.rb_left) {
			parent = rb_entry(parent->shared.linear.rb.rb_left,
				struct vm_area_struct, shared.linear.rb);
			if (parent->shared.linear.rb_subtree_last < last)
				parent->shared.linear.rb_subtree_last = last;
		}
		link = &parent->shared.linear.rb.rb_left;
	}

	node->shared.linear.rb_subtree_last = last;
	rb_link_node(&node->shared.linear.rb, &parent->shared.linear.rb, link);
	rb_insert_augmented(&node->shared.linear.rb, root,
			    &vma_interval_tree_augment);
}

static inline unsigned long avc_start_pgoff(struct anon_vma_chain *avc)
{
	return vma_start_pgoff(avc->vma);
}

static inline unsigned long avc_last_pgoff(struct anon_vma_chain *avc)
{
	return vma_last_pgoff(avc->vma);
}

#ifdef MY_DEF_HERE
INTERVAL_TREE_DEFINE(struct anon_vma_chain, rb, pgoff_t, rb_subtree_last,
#else  
INTERVAL_TREE_DEFINE(struct anon_vma_chain, rb, unsigned long, rb_subtree_last,
#endif  
		     avc_start_pgoff, avc_last_pgoff,
		     static inline, __anon_vma_interval_tree)

void anon_vma_interval_tree_insert(struct anon_vma_chain *node,
				   struct rb_root *root)
{
#ifdef CONFIG_DEBUG_VM_RB
	node->cached_vma_start = avc_start_pgoff(node);
	node->cached_vma_last = avc_last_pgoff(node);
#endif
	__anon_vma_interval_tree_insert(node, root);
}

void anon_vma_interval_tree_remove(struct anon_vma_chain *node,
				   struct rb_root *root)
{
	__anon_vma_interval_tree_remove(node, root);
}

struct anon_vma_chain *
anon_vma_interval_tree_iter_first(struct rb_root *root,
#ifdef MY_DEF_HERE
				  pgoff_t first, pgoff_t last)
#else  
				  unsigned long first, unsigned long last)
#endif  
{
	return __anon_vma_interval_tree_iter_first(root, first, last);
}

struct anon_vma_chain *
anon_vma_interval_tree_iter_next(struct anon_vma_chain *node,
#ifdef MY_DEF_HERE
				 pgoff_t first, pgoff_t last)
#else  
				 unsigned long first, unsigned long last)
#endif  
{
	return __anon_vma_interval_tree_iter_next(node, first, last);
}

#ifdef CONFIG_DEBUG_VM_RB
void anon_vma_interval_tree_verify(struct anon_vma_chain *node)
{
	WARN_ON_ONCE(node->cached_vma_start != avc_start_pgoff(node));
	WARN_ON_ONCE(node->cached_vma_last != avc_last_pgoff(node));
}
#endif
