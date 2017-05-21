#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/cgroup.h>
#include <linux/cred.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/init_task.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/backing-dev.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/magic.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sort.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/delayacct.h>
#include <linux/cgroupstats.h>
#include <linux/hashtable.h>
#include <linux/namei.h>
#include <linux/pid_namespace.h>
#include <linux/idr.h>
#include <linux/vmalloc.h>  
#include <linux/eventfd.h>
#include <linux/poll.h>
#include <linux/flex_array.h>  
#include <linux/kthread.h>

#include <linux/atomic.h>

#define CSS_DEACT_BIAS		INT_MIN

#ifdef CONFIG_PROVE_RCU
DEFINE_MUTEX(cgroup_mutex);
EXPORT_SYMBOL_GPL(cgroup_mutex);	 
#else
static DEFINE_MUTEX(cgroup_mutex);
#endif

static DEFINE_MUTEX(cgroup_root_mutex);

static struct workqueue_struct *cgroup_destroy_wq;

#define SUBSYS(_x) [_x ## _subsys_id] = &_x ## _subsys,
#define IS_SUBSYS_ENABLED(option) IS_BUILTIN(option)
static struct cgroup_subsys *subsys[CGROUP_SUBSYS_COUNT] = {
#include <linux/cgroup_subsys.h>
};

static struct cgroupfs_root rootnode;

struct cfent {
	struct list_head		node;
	struct dentry			*dentry;
	struct cftype			*type;

	struct simple_xattrs		xattrs;
};

#define CSS_ID_MAX	(65535)
struct css_id {
	 
	struct cgroup_subsys_state __rcu *css;
	 
	unsigned short id;
	 
	unsigned short depth;
	 
	struct rcu_head rcu_head;
	 
	unsigned short stack[0];  
};

struct cgroup_event {
	 
	struct cgroup *cgrp;
	 
	struct cftype *cft;
	 
	struct eventfd_ctx *eventfd;
	 
	struct list_head list;
	 
	poll_table pt;
	wait_queue_head_t *wqh;
	wait_queue_t wait;
	struct work_struct remove;
};

static LIST_HEAD(roots);
static int root_count;

static DEFINE_IDA(hierarchy_ida);
static int next_hierarchy_id;
static DEFINE_SPINLOCK(hierarchy_id_lock);

#define dummytop (&rootnode.top_cgroup)

static struct cgroup_name root_cgroup_name = { .name = "/" };

static int need_forkexit_callback __read_mostly;

static int cgroup_destroy_locked(struct cgroup *cgrp);
static int cgroup_addrm_files(struct cgroup *cgrp, struct cgroup_subsys *subsys,
			      struct cftype cfts[], bool is_add);

static int css_unbias_refcnt(int refcnt)
{
	return refcnt >= 0 ? refcnt : refcnt - CSS_DEACT_BIAS;
}

static int css_refcnt(struct cgroup_subsys_state *css)
{
	int v = atomic_read(&css->refcnt);

	return css_unbias_refcnt(v);
}

inline int cgroup_is_removed(const struct cgroup *cgrp)
{
	return test_bit(CGRP_REMOVED, &cgrp->flags);
}

bool cgroup_is_descendant(struct cgroup *cgrp, struct cgroup *ancestor)
{
	while (cgrp) {
		if (cgrp == ancestor)
			return true;
		cgrp = cgrp->parent;
	}
	return false;
}
EXPORT_SYMBOL_GPL(cgroup_is_descendant);

static int cgroup_is_releasable(const struct cgroup *cgrp)
{
	const int bits =
		(1 << CGRP_RELEASABLE) |
		(1 << CGRP_NOTIFY_ON_RELEASE);
	return (cgrp->flags & bits) == bits;
}

static int notify_on_release(const struct cgroup *cgrp)
{
	return test_bit(CGRP_NOTIFY_ON_RELEASE, &cgrp->flags);
}

#define for_each_subsys(_root, _ss) \
list_for_each_entry(_ss, &_root->subsys_list, sibling)

#define for_each_active_root(_root) \
list_for_each_entry(_root, &roots, root_list)

static inline struct cgroup *__d_cgrp(struct dentry *dentry)
{
	return dentry->d_fsdata;
}

static inline struct cfent *__d_cfe(struct dentry *dentry)
{
	return dentry->d_fsdata;
}

static inline struct cftype *__d_cft(struct dentry *dentry)
{
	return __d_cfe(dentry)->type;
}

static bool cgroup_lock_live_group(struct cgroup *cgrp)
{
	mutex_lock(&cgroup_mutex);
	if (cgroup_is_removed(cgrp)) {
		mutex_unlock(&cgroup_mutex);
		return false;
	}
	return true;
}

static LIST_HEAD(release_list);
static DEFINE_RAW_SPINLOCK(release_list_lock);
static void cgroup_release_agent(struct work_struct *work);
static DECLARE_WORK(release_agent_work, cgroup_release_agent);
static void check_for_release(struct cgroup *cgrp);

struct cg_cgroup_link {
	 
	struct list_head cgrp_link_list;
	struct cgroup *cgrp;
	 
	struct list_head cg_link_list;
	struct css_set *cg;
};

static struct css_set init_css_set;
static struct cg_cgroup_link init_css_set_link;

static int cgroup_init_idr(struct cgroup_subsys *ss,
			   struct cgroup_subsys_state *css);

static DEFINE_RWLOCK(css_set_lock);
static int css_set_count;

#define CSS_SET_HASH_BITS	7
static DEFINE_HASHTABLE(css_set_table, CSS_SET_HASH_BITS);

static unsigned long css_set_hash(struct cgroup_subsys_state *css[])
{
	int i;
	unsigned long key = 0UL;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++)
		key += (unsigned long)css[i];
	key = (key >> 16) ^ key;

	return key;
}

static int use_task_css_set_links __read_mostly;

static void __put_css_set(struct css_set *cg, int taskexit)
{
	struct cg_cgroup_link *link;
	struct cg_cgroup_link *saved_link;
	 
	if (atomic_add_unless(&cg->refcount, -1, 1))
		return;
	write_lock(&css_set_lock);
	if (!atomic_dec_and_test(&cg->refcount)) {
		write_unlock(&css_set_lock);
		return;
	}

	hash_del(&cg->hlist);
	css_set_count--;

	list_for_each_entry_safe(link, saved_link, &cg->cg_links,
				 cg_link_list) {
		struct cgroup *cgrp = link->cgrp;
		list_del(&link->cg_link_list);
		list_del(&link->cgrp_link_list);

		rcu_read_lock();
		if (atomic_dec_and_test(&cgrp->count) &&
		    notify_on_release(cgrp)) {
			if (taskexit)
				set_bit(CGRP_RELEASABLE, &cgrp->flags);
			check_for_release(cgrp);
		}
		rcu_read_unlock();

		kfree(link);
	}

	write_unlock(&css_set_lock);
	kfree_rcu(cg, rcu_head);
}

static inline void get_css_set(struct css_set *cg)
{
	atomic_inc(&cg->refcount);
}

static inline void put_css_set(struct css_set *cg)
{
	__put_css_set(cg, 0);
}

static inline void put_css_set_taskexit(struct css_set *cg)
{
	__put_css_set(cg, 1);
}

static bool compare_css_sets(struct css_set *cg,
			     struct css_set *old_cg,
			     struct cgroup *new_cgrp,
			     struct cgroup_subsys_state *template[])
{
	struct list_head *l1, *l2;

	if (memcmp(template, cg->subsys, sizeof(cg->subsys))) {
		 
		return false;
	}

	l1 = &cg->cg_links;
	l2 = &old_cg->cg_links;
	while (1) {
		struct cg_cgroup_link *cgl1, *cgl2;
		struct cgroup *cg1, *cg2;

		l1 = l1->next;
		l2 = l2->next;
		 
		if (l1 == &cg->cg_links) {
			BUG_ON(l2 != &old_cg->cg_links);
			break;
		} else {
			BUG_ON(l2 == &old_cg->cg_links);
		}
		 
		cgl1 = list_entry(l1, struct cg_cgroup_link, cg_link_list);
		cgl2 = list_entry(l2, struct cg_cgroup_link, cg_link_list);
		cg1 = cgl1->cgrp;
		cg2 = cgl2->cgrp;
		 
		BUG_ON(cg1->root != cg2->root);

		if (cg1->root == new_cgrp->root) {
			if (cg1 != new_cgrp)
				return false;
		} else {
			if (cg1 != cg2)
				return false;
		}
	}
	return true;
}

static struct css_set *find_existing_css_set(
	struct css_set *oldcg,
	struct cgroup *cgrp,
	struct cgroup_subsys_state *template[])
{
	int i;
	struct cgroupfs_root *root = cgrp->root;
	struct css_set *cg;
	unsigned long key;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		if (root->subsys_mask & (1UL << i)) {
			 
			template[i] = cgrp->subsys[i];
		} else {
			 
			template[i] = oldcg->subsys[i];
		}
	}

	key = css_set_hash(template);
	hash_for_each_possible(css_set_table, cg, hlist, key) {
		if (!compare_css_sets(cg, oldcg, cgrp, template))
			continue;

		return cg;
	}

	return NULL;
}

static void free_cg_links(struct list_head *tmp)
{
	struct cg_cgroup_link *link;
	struct cg_cgroup_link *saved_link;

	list_for_each_entry_safe(link, saved_link, tmp, cgrp_link_list) {
		list_del(&link->cgrp_link_list);
		kfree(link);
	}
}

static int allocate_cg_links(int count, struct list_head *tmp)
{
	struct cg_cgroup_link *link;
	int i;
	INIT_LIST_HEAD(tmp);
	for (i = 0; i < count; i++) {
		link = kmalloc(sizeof(*link), GFP_KERNEL);
		if (!link) {
			free_cg_links(tmp);
			return -ENOMEM;
		}
		list_add(&link->cgrp_link_list, tmp);
	}
	return 0;
}

static void link_css_set(struct list_head *tmp_cg_links,
			 struct css_set *cg, struct cgroup *cgrp)
{
	struct cg_cgroup_link *link;

	BUG_ON(list_empty(tmp_cg_links));
	link = list_first_entry(tmp_cg_links, struct cg_cgroup_link,
				cgrp_link_list);
	link->cg = cg;
	link->cgrp = cgrp;
	atomic_inc(&cgrp->count);
	list_move(&link->cgrp_link_list, &cgrp->css_sets);
	 
	list_add_tail(&link->cg_link_list, &cg->cg_links);
}

static struct css_set *find_css_set(
	struct css_set *oldcg, struct cgroup *cgrp)
{
	struct css_set *res;
	struct cgroup_subsys_state *template[CGROUP_SUBSYS_COUNT];

	struct list_head tmp_cg_links;

	struct cg_cgroup_link *link;
	unsigned long key;

	read_lock(&css_set_lock);
	res = find_existing_css_set(oldcg, cgrp, template);
	if (res)
		get_css_set(res);
	read_unlock(&css_set_lock);

	if (res)
		return res;

	res = kmalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return NULL;

	if (allocate_cg_links(root_count, &tmp_cg_links) < 0) {
		kfree(res);
		return NULL;
	}

	atomic_set(&res->refcount, 1);
	INIT_LIST_HEAD(&res->cg_links);
	INIT_LIST_HEAD(&res->tasks);
	INIT_HLIST_NODE(&res->hlist);

	memcpy(res->subsys, template, sizeof(res->subsys));

	write_lock(&css_set_lock);
	 
	list_for_each_entry(link, &oldcg->cg_links, cg_link_list) {
		struct cgroup *c = link->cgrp;
		if (c->root == cgrp->root)
			c = cgrp;
		link_css_set(&tmp_cg_links, res, c);
	}

	BUG_ON(!list_empty(&tmp_cg_links));

	css_set_count++;

	key = css_set_hash(res->subsys);
	hash_add(css_set_table, &res->hlist, key);

	write_unlock(&css_set_lock);

	return res;
}

static struct cgroup *task_cgroup_from_root(struct task_struct *task,
					    struct cgroupfs_root *root)
{
	struct css_set *css;
	struct cgroup *res = NULL;

	BUG_ON(!mutex_is_locked(&cgroup_mutex));
	read_lock(&css_set_lock);
	 
	css = task->cgroups;
	if (css == &init_css_set) {
		res = &root->top_cgroup;
	} else {
		struct cg_cgroup_link *link;
		list_for_each_entry(link, &css->cg_links, cg_link_list) {
			struct cgroup *c = link->cgrp;
			if (c->root == root) {
				res = c;
				break;
			}
		}
	}
	read_unlock(&css_set_lock);
	BUG_ON(!res);
	return res;
}

static int cgroup_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static struct dentry *cgroup_lookup(struct inode *, struct dentry *, unsigned int);
static int cgroup_rmdir(struct inode *unused_dir, struct dentry *dentry);
static int cgroup_populate_dir(struct cgroup *cgrp, bool base_files,
			       unsigned long subsys_mask);
static const struct inode_operations cgroup_dir_inode_operations;
static const struct file_operations proc_cgroupstats_operations;

static struct backing_dev_info cgroup_backing_dev_info = {
	.name		= "cgroup",
	.capabilities	= BDI_CAP_NO_ACCT_AND_WRITEBACK,
};

static int alloc_css_id(struct cgroup_subsys *ss,
			struct cgroup *parent, struct cgroup *child);

static struct inode *cgroup_new_inode(umode_t mode, struct super_block *sb)
{
	struct inode *inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode->i_mode = mode;
		inode->i_uid = current_fsuid();
		inode->i_gid = current_fsgid();
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		inode->i_mapping->backing_dev_info = &cgroup_backing_dev_info;
	}
	return inode;
}

static struct cgroup_name *cgroup_alloc_name(struct dentry *dentry)
{
	struct cgroup_name *name;

	name = kmalloc(sizeof(*name) + dentry->d_name.len + 1, GFP_KERNEL);
	if (!name)
		return NULL;
	strcpy(name->name, dentry->d_name.name);
	return name;
}

static void cgroup_free_fn(struct work_struct *work)
{
	struct cgroup *cgrp = container_of(work, struct cgroup, free_work);
	struct cgroup_subsys *ss;

	mutex_lock(&cgroup_mutex);
	 
	for_each_subsys(cgrp->root, ss)
		ss->css_free(cgrp);

	cgrp->root->number_of_cgroups--;
	mutex_unlock(&cgroup_mutex);

	dput(cgrp->parent->dentry);

	ida_simple_remove(&cgrp->root->cgroup_ida, cgrp->id);

	deactivate_super(cgrp->root->sb);

	BUG_ON(!list_empty(&cgrp->pidlists));

	simple_xattrs_free(&cgrp->xattrs);

	kfree(rcu_dereference_raw(cgrp->name));
	kfree(cgrp);
}

static void cgroup_free_rcu(struct rcu_head *head)
{
	struct cgroup *cgrp = container_of(head, struct cgroup, rcu_head);

	queue_work(cgroup_destroy_wq, &cgrp->free_work);
}

static void cgroup_diput(struct dentry *dentry, struct inode *inode)
{
	 
	if (S_ISDIR(inode->i_mode)) {
		struct cgroup *cgrp = dentry->d_fsdata;

		BUG_ON(!(cgroup_is_removed(cgrp)));
		call_rcu(&cgrp->rcu_head, cgroup_free_rcu);
	} else {
		struct cfent *cfe = __d_cfe(dentry);
		struct cgroup *cgrp = dentry->d_parent->d_fsdata;

		WARN_ONCE(!list_empty(&cfe->node) &&
			  cgrp != &cgrp->root->top_cgroup,
			  "cfe still linked for %s\n", cfe->type->name);
		simple_xattrs_free(&cfe->xattrs);
		kfree(cfe);
	}
	iput(inode);
}

static void remove_dir(struct dentry *d)
{
	struct dentry *parent = dget(d->d_parent);

	d_delete(d);
	simple_rmdir(parent->d_inode, d);
	dput(parent);
}

static void cgroup_rm_file(struct cgroup *cgrp, const struct cftype *cft)
{
	struct cfent *cfe;

	lockdep_assert_held(&cgrp->dentry->d_inode->i_mutex);
	lockdep_assert_held(&cgroup_mutex);

	list_for_each_entry(cfe, &cgrp->files, node) {
		struct dentry *d = cfe->dentry;

		if (cft && cfe->type != cft)
			continue;

		dget(d);
		d_delete(d);
		simple_unlink(cgrp->dentry->d_inode, d);
		list_del_init(&cfe->node);
		dput(d);

		break;
	}
}

static void cgroup_clear_directory(struct dentry *dir, bool base_files,
				   unsigned long subsys_mask)
{
	struct cgroup *cgrp = __d_cgrp(dir);
	struct cgroup_subsys *ss;

	for_each_subsys(cgrp->root, ss) {
		struct cftype_set *set;
		if (!test_bit(ss->subsys_id, &subsys_mask))
			continue;
		list_for_each_entry(set, &ss->cftsets, node)
			cgroup_addrm_files(cgrp, NULL, set->cfts, false);
	}
	if (base_files) {
		while (!list_empty(&cgrp->files))
			cgroup_rm_file(cgrp, NULL);
	}
}

static void cgroup_d_remove_dir(struct dentry *dentry)
{
	struct dentry *parent;
	struct cgroupfs_root *root = dentry->d_sb->s_fs_info;

	cgroup_clear_directory(dentry, true, root->subsys_mask);

	parent = dentry->d_parent;
	spin_lock(&parent->d_lock);
	spin_lock_nested(&dentry->d_lock, DENTRY_D_LOCK_NESTED);
	list_del_init(&dentry->d_child);
	spin_unlock(&dentry->d_lock);
	spin_unlock(&parent->d_lock);
	remove_dir(dentry);
}

static int rebind_subsystems(struct cgroupfs_root *root,
			      unsigned long final_subsys_mask)
{
	unsigned long added_mask, removed_mask;
	struct cgroup *cgrp = &root->top_cgroup;
	int i;

	BUG_ON(!mutex_is_locked(&cgroup_mutex));
	BUG_ON(!mutex_is_locked(&cgroup_root_mutex));

	removed_mask = root->actual_subsys_mask & ~final_subsys_mask;
	added_mask = final_subsys_mask & ~root->actual_subsys_mask;
	 
	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		unsigned long bit = 1UL << i;
		struct cgroup_subsys *ss = subsys[i];
		if (!(bit & added_mask))
			continue;
		 
		BUG_ON(ss == NULL);
		if (ss->root != &rootnode) {
			 
			return -EBUSY;
		}
	}

	if (root->number_of_cgroups > 1)
		return -EBUSY;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		struct cgroup_subsys *ss = subsys[i];
		unsigned long bit = 1UL << i;
		if (bit & added_mask) {
			 
			BUG_ON(ss == NULL);
			BUG_ON(cgrp->subsys[i]);
			BUG_ON(!dummytop->subsys[i]);
			BUG_ON(dummytop->subsys[i]->cgroup != dummytop);
			cgrp->subsys[i] = dummytop->subsys[i];
			cgrp->subsys[i]->cgroup = cgrp;
			list_move(&ss->sibling, &root->subsys_list);
			ss->root = root;
			if (ss->bind)
				ss->bind(cgrp);
			 
		} else if (bit & removed_mask) {
			 
			BUG_ON(ss == NULL);
			BUG_ON(cgrp->subsys[i] != dummytop->subsys[i]);
			BUG_ON(cgrp->subsys[i]->cgroup != cgrp);
			if (ss->bind)
				ss->bind(dummytop);
			dummytop->subsys[i]->cgroup = dummytop;
			cgrp->subsys[i] = NULL;
			subsys[i]->root = &rootnode;
			list_move(&ss->sibling, &rootnode.subsys_list);
			 
			module_put(ss->module);
		} else if (bit & final_subsys_mask) {
			 
			BUG_ON(ss == NULL);
			BUG_ON(!cgrp->subsys[i]);
			 
			module_put(ss->module);
#ifdef CONFIG_MODULE_UNLOAD
			BUG_ON(ss->module && !module_refcount(ss->module));
#endif
		} else {
			 
			BUG_ON(cgrp->subsys[i]);
		}
	}
	root->subsys_mask = root->actual_subsys_mask = final_subsys_mask;

	return 0;
}

static int cgroup_show_options(struct seq_file *seq, struct dentry *dentry)
{
	struct cgroupfs_root *root = dentry->d_sb->s_fs_info;
	struct cgroup_subsys *ss;

	mutex_lock(&cgroup_root_mutex);
	for_each_subsys(root, ss)
		seq_printf(seq, ",%s", ss->name);
	if (root->flags & CGRP_ROOT_SANE_BEHAVIOR)
		seq_puts(seq, ",sane_behavior");
	if (root->flags & CGRP_ROOT_NOPREFIX)
		seq_puts(seq, ",noprefix");
	if (root->flags & CGRP_ROOT_XATTR)
		seq_puts(seq, ",xattr");
	if (strlen(root->release_agent_path))
		seq_printf(seq, ",release_agent=%s", root->release_agent_path);
	if (test_bit(CGRP_CPUSET_CLONE_CHILDREN, &root->top_cgroup.flags))
		seq_puts(seq, ",clone_children");
	if (strlen(root->name))
		seq_printf(seq, ",name=%s", root->name);
	mutex_unlock(&cgroup_root_mutex);
	return 0;
}

struct cgroup_sb_opts {
	unsigned long subsys_mask;
	unsigned long flags;
	char *release_agent;
	bool cpuset_clone_children;
	char *name;
	 
	bool none;

	struct cgroupfs_root *new_root;

};

static int parse_cgroupfs_options(char *data, struct cgroup_sb_opts *opts)
{
	char *token, *o = data;
	bool all_ss = false, one_ss = false;
	unsigned long mask = (unsigned long)-1;
	int i;
	bool module_pin_failed = false;

	BUG_ON(!mutex_is_locked(&cgroup_mutex));

#ifdef CONFIG_CPUSETS
	mask = ~(1UL << cpuset_subsys_id);
#endif

	memset(opts, 0, sizeof(*opts));

	while ((token = strsep(&o, ",")) != NULL) {
		if (!*token)
			return -EINVAL;
		if (!strcmp(token, "none")) {
			 
			opts->none = true;
			continue;
		}
		if (!strcmp(token, "all")) {
			 
			if (one_ss)
				return -EINVAL;
			all_ss = true;
			continue;
		}
		if (!strcmp(token, "__DEVEL__sane_behavior")) {
			opts->flags |= CGRP_ROOT_SANE_BEHAVIOR;
			continue;
		}
		if (!strcmp(token, "noprefix")) {
			opts->flags |= CGRP_ROOT_NOPREFIX;
			continue;
		}
		if (!strcmp(token, "clone_children")) {
			opts->cpuset_clone_children = true;
			continue;
		}
		if (!strcmp(token, "xattr")) {
			opts->flags |= CGRP_ROOT_XATTR;
			continue;
		}
		if (!strncmp(token, "release_agent=", 14)) {
			 
			if (opts->release_agent)
				return -EINVAL;
			opts->release_agent =
				kstrndup(token + 14, PATH_MAX - 1, GFP_KERNEL);
			if (!opts->release_agent)
				return -ENOMEM;
			continue;
		}
		if (!strncmp(token, "name=", 5)) {
			const char *name = token + 5;
			 
			if (!strlen(name))
				return -EINVAL;
			 
			for (i = 0; i < strlen(name); i++) {
				char c = name[i];
				if (isalnum(c))
					continue;
				if ((c == '.') || (c == '-') || (c == '_'))
					continue;
				return -EINVAL;
			}
			 
			if (opts->name)
				return -EINVAL;
			opts->name = kstrndup(name,
					      MAX_CGROUP_ROOT_NAMELEN - 1,
					      GFP_KERNEL);
			if (!opts->name)
				return -ENOMEM;

			continue;
		}

		for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
			struct cgroup_subsys *ss = subsys[i];
			if (ss == NULL)
				continue;
			if (strcmp(token, ss->name))
				continue;
			if (ss->disabled)
				continue;

			if (all_ss)
				return -EINVAL;
			set_bit(i, &opts->subsys_mask);
			one_ss = true;

			break;
		}
		if (i == CGROUP_SUBSYS_COUNT)
			return -ENOENT;
	}

	if (all_ss || (!one_ss && !opts->none && !opts->name)) {
		for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
			struct cgroup_subsys *ss = subsys[i];
			if (ss == NULL)
				continue;
			if (ss->disabled)
				continue;
			set_bit(i, &opts->subsys_mask);
		}
	}

	if (opts->flags & CGRP_ROOT_SANE_BEHAVIOR) {
		pr_warning("cgroup: sane_behavior: this is still under development and its behaviors will change, proceed at your own risk\n");

		if (opts->flags & CGRP_ROOT_NOPREFIX) {
			pr_err("cgroup: sane_behavior: noprefix is not allowed\n");
			return -EINVAL;
		}

		if (opts->cpuset_clone_children) {
			pr_err("cgroup: sane_behavior: clone_children is not allowed\n");
			return -EINVAL;
		}
	}

	if ((opts->flags & CGRP_ROOT_NOPREFIX) && (opts->subsys_mask & mask))
		return -EINVAL;

	if (opts->subsys_mask && opts->none)
		return -EINVAL;

	if (!opts->subsys_mask && !opts->name)
		return -EINVAL;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		unsigned long bit = 1UL << i;

		if (!(bit & opts->subsys_mask))
			continue;
		if (!try_module_get(subsys[i]->module)) {
			module_pin_failed = true;
			break;
		}
	}
	if (module_pin_failed) {
		 
		for (i--; i >= 0; i--) {
			 
			unsigned long bit = 1UL << i;

			if (!(bit & opts->subsys_mask))
				continue;
			module_put(subsys[i]->module);
		}
		return -ENOENT;
	}

	return 0;
}

static void drop_parsed_module_refcounts(unsigned long subsys_mask)
{
	int i;
	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		unsigned long bit = 1UL << i;

		if (!(bit & subsys_mask))
			continue;
		module_put(subsys[i]->module);
	}
}

static int cgroup_remount(struct super_block *sb, int *flags, char *data)
{
	int ret = 0;
	struct cgroupfs_root *root = sb->s_fs_info;
	struct cgroup *cgrp = &root->top_cgroup;
	struct cgroup_sb_opts opts;
	unsigned long added_mask, removed_mask;

	if (root->flags & CGRP_ROOT_SANE_BEHAVIOR) {
		pr_err("cgroup: sane_behavior: remount is not allowed\n");
		return -EINVAL;
	}

	mutex_lock(&cgrp->dentry->d_inode->i_mutex);
	mutex_lock(&cgroup_mutex);
	mutex_lock(&cgroup_root_mutex);

	ret = parse_cgroupfs_options(data, &opts);
	if (ret)
		goto out_unlock;

	if (opts.subsys_mask != root->actual_subsys_mask || opts.release_agent)
		pr_warning("cgroup: option changes via remount are deprecated (pid=%d comm=%s)\n",
			   task_tgid_nr(current), current->comm);

	added_mask = opts.subsys_mask & ~root->subsys_mask;
	removed_mask = root->subsys_mask & ~opts.subsys_mask;

	if (opts.flags != root->flags ||
	    (opts.name && strcmp(opts.name, root->name))) {
		ret = -EINVAL;
		drop_parsed_module_refcounts(opts.subsys_mask);
		goto out_unlock;
	}

	cgroup_clear_directory(cgrp->dentry, false, removed_mask);

	ret = rebind_subsystems(root, opts.subsys_mask);
	if (ret) {
		 
		cgroup_populate_dir(cgrp, false, removed_mask);
		drop_parsed_module_refcounts(opts.subsys_mask);
		goto out_unlock;
	}

	cgroup_populate_dir(cgrp, false, added_mask);

	if (opts.release_agent)
		strcpy(root->release_agent_path, opts.release_agent);
 out_unlock:
	kfree(opts.release_agent);
	kfree(opts.name);
	mutex_unlock(&cgroup_root_mutex);
	mutex_unlock(&cgroup_mutex);
	mutex_unlock(&cgrp->dentry->d_inode->i_mutex);
	return ret;
}

static const struct super_operations cgroup_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode,
	.show_options = cgroup_show_options,
	.remount_fs = cgroup_remount,
};

static void init_cgroup_housekeeping(struct cgroup *cgrp)
{
	INIT_LIST_HEAD(&cgrp->sibling);
	INIT_LIST_HEAD(&cgrp->children);
	INIT_LIST_HEAD(&cgrp->files);
	INIT_LIST_HEAD(&cgrp->css_sets);
	INIT_LIST_HEAD(&cgrp->allcg_node);
	INIT_LIST_HEAD(&cgrp->release_list);
	INIT_LIST_HEAD(&cgrp->pidlists);
	INIT_WORK(&cgrp->free_work, cgroup_free_fn);
	mutex_init(&cgrp->pidlist_mutex);
	INIT_LIST_HEAD(&cgrp->event_list);
	spin_lock_init(&cgrp->event_list_lock);
	simple_xattrs_init(&cgrp->xattrs);
}

static void init_cgroup_root(struct cgroupfs_root *root)
{
	struct cgroup *cgrp = &root->top_cgroup;

	INIT_LIST_HEAD(&root->subsys_list);
	INIT_LIST_HEAD(&root->root_list);
	INIT_LIST_HEAD(&root->allcg_list);
	root->number_of_cgroups = 1;
	cgrp->root = root;
	cgrp->name = &root_cgroup_name;
	init_cgroup_housekeeping(cgrp);
	list_add_tail(&cgrp->allcg_node, &root->allcg_list);
}

static bool init_root_id(struct cgroupfs_root *root)
{
	int ret = 0;

	do {
		if (!ida_pre_get(&hierarchy_ida, GFP_KERNEL))
			return false;
		spin_lock(&hierarchy_id_lock);
		 
		ret = ida_get_new_above(&hierarchy_ida, next_hierarchy_id,
					&root->hierarchy_id);
		if (ret == -ENOSPC)
			 
			ret = ida_get_new(&hierarchy_ida, &root->hierarchy_id);
		if (!ret) {
			next_hierarchy_id = root->hierarchy_id + 1;
		} else if (ret != -EAGAIN) {
			 
			BUG_ON(ret);
		}
		spin_unlock(&hierarchy_id_lock);
	} while (ret);
	return true;
}

static int cgroup_test_super(struct super_block *sb, void *data)
{
	struct cgroup_sb_opts *opts = data;
	struct cgroupfs_root *root = sb->s_fs_info;

	if (opts->name && strcmp(opts->name, root->name))
		return 0;

	if ((opts->subsys_mask || opts->none)
	    && (opts->subsys_mask != root->subsys_mask))
		return 0;

	return 1;
}

static struct cgroupfs_root *cgroup_root_from_opts(struct cgroup_sb_opts *opts)
{
	struct cgroupfs_root *root;

	if (!opts->subsys_mask && !opts->none)
		return NULL;

	root = kzalloc(sizeof(*root), GFP_KERNEL);
	if (!root)
		return ERR_PTR(-ENOMEM);

	if (!init_root_id(root)) {
		kfree(root);
		return ERR_PTR(-ENOMEM);
	}
	init_cgroup_root(root);

	root->subsys_mask = opts->subsys_mask;
	root->flags = opts->flags;
	ida_init(&root->cgroup_ida);
	if (opts->release_agent)
		strcpy(root->release_agent_path, opts->release_agent);
	if (opts->name)
		strcpy(root->name, opts->name);
	if (opts->cpuset_clone_children)
		set_bit(CGRP_CPUSET_CLONE_CHILDREN, &root->top_cgroup.flags);
	return root;
}

static void cgroup_drop_root(struct cgroupfs_root *root)
{
	if (!root)
		return;

	BUG_ON(!root->hierarchy_id);
	spin_lock(&hierarchy_id_lock);
	ida_remove(&hierarchy_ida, root->hierarchy_id);
	spin_unlock(&hierarchy_id_lock);
	ida_destroy(&root->cgroup_ida);
	kfree(root);
}

static int cgroup_set_super(struct super_block *sb, void *data)
{
	int ret;
	struct cgroup_sb_opts *opts = data;

	if (!opts->new_root)
		return -EINVAL;

	BUG_ON(!opts->subsys_mask && !opts->none);

	ret = set_anon_super(sb, NULL);
	if (ret)
		return ret;

	sb->s_fs_info = opts->new_root;
	opts->new_root->sb = sb;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = CGROUP_SUPER_MAGIC;
	sb->s_op = &cgroup_ops;

	return 0;
}

static int cgroup_get_rootdir(struct super_block *sb)
{
	static const struct dentry_operations cgroup_dops = {
		.d_iput = cgroup_diput,
		.d_delete = always_delete_dentry,
	};

	struct inode *inode =
		cgroup_new_inode(S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR, sb);

	if (!inode)
		return -ENOMEM;

	inode->i_fop = &simple_dir_operations;
	inode->i_op = &cgroup_dir_inode_operations;
	 
	inc_nlink(inode);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;
	 
	sb->s_d_op = &cgroup_dops;
	return 0;
}

static struct dentry *cgroup_mount(struct file_system_type *fs_type,
			 int flags, const char *unused_dev_name,
			 void *data)
{
	struct cgroup_sb_opts opts;
	struct cgroupfs_root *root;
	int ret = 0;
	struct super_block *sb;
	struct cgroupfs_root *new_root;
	struct inode *inode;

	mutex_lock(&cgroup_mutex);
	ret = parse_cgroupfs_options(data, &opts);
	mutex_unlock(&cgroup_mutex);
	if (ret)
		goto out_err;

	new_root = cgroup_root_from_opts(&opts);
	if (IS_ERR(new_root)) {
		ret = PTR_ERR(new_root);
		goto drop_modules;
	}
	opts.new_root = new_root;

	sb = sget(fs_type, cgroup_test_super, cgroup_set_super, 0, &opts);
	if (IS_ERR(sb)) {
		ret = PTR_ERR(sb);
		cgroup_drop_root(opts.new_root);
		goto drop_modules;
	}

	root = sb->s_fs_info;
	BUG_ON(!root);
	if (root == opts.new_root) {
		 
		struct list_head tmp_cg_links;
		struct cgroup *root_cgrp = &root->top_cgroup;
		struct cgroupfs_root *existing_root;
		const struct cred *cred;
		int i;
		struct css_set *cg;

		BUG_ON(sb->s_root != NULL);

		ret = cgroup_get_rootdir(sb);
		if (ret)
			goto drop_new_super;
		inode = sb->s_root->d_inode;

		mutex_lock(&inode->i_mutex);
		mutex_lock(&cgroup_mutex);
		mutex_lock(&cgroup_root_mutex);

		ret = -EBUSY;
		if (strlen(root->name))
			for_each_active_root(existing_root)
				if (!strcmp(existing_root->name, root->name))
					goto unlock_drop;

		ret = allocate_cg_links(css_set_count, &tmp_cg_links);
		if (ret)
			goto unlock_drop;

		ret = rebind_subsystems(root, root->subsys_mask);
		if (ret == -EBUSY) {
			free_cg_links(&tmp_cg_links);
			goto unlock_drop;
		}
		 
		BUG_ON(ret);

		list_add(&root->root_list, &roots);
		root_count++;

		sb->s_root->d_fsdata = root_cgrp;
		root->top_cgroup.dentry = sb->s_root;

		write_lock(&css_set_lock);
		hash_for_each(css_set_table, i, cg, hlist)
			link_css_set(&tmp_cg_links, cg, root_cgrp);
		write_unlock(&css_set_lock);

		free_cg_links(&tmp_cg_links);

		BUG_ON(!list_empty(&root_cgrp->children));
		BUG_ON(root->number_of_cgroups != 1);

		cred = override_creds(&init_cred);
		cgroup_populate_dir(root_cgrp, true, root->subsys_mask);
		revert_creds(cred);
		mutex_unlock(&cgroup_root_mutex);
		mutex_unlock(&cgroup_mutex);
		mutex_unlock(&inode->i_mutex);
	} else {
		 
		cgroup_drop_root(opts.new_root);

		if (root->flags != opts.flags) {
			if ((root->flags | opts.flags) & CGRP_ROOT_SANE_BEHAVIOR) {
				pr_err("cgroup: sane_behavior: new mount options should match the existing superblock\n");
				ret = -EINVAL;
				goto drop_new_super;
			} else {
				pr_warning("cgroup: new mount options do not match the existing superblock, will be ignored\n");
			}
		}

		drop_parsed_module_refcounts(opts.subsys_mask);
	}

	kfree(opts.release_agent);
	kfree(opts.name);
	return dget(sb->s_root);

 unlock_drop:
	mutex_unlock(&cgroup_root_mutex);
	mutex_unlock(&cgroup_mutex);
	mutex_unlock(&inode->i_mutex);
 drop_new_super:
	deactivate_locked_super(sb);
 drop_modules:
	drop_parsed_module_refcounts(opts.subsys_mask);
 out_err:
	kfree(opts.release_agent);
	kfree(opts.name);
	return ERR_PTR(ret);
}

static void cgroup_kill_sb(struct super_block *sb) {
	struct cgroupfs_root *root = sb->s_fs_info;
	struct cgroup *cgrp = &root->top_cgroup;
	int ret;
	struct cg_cgroup_link *link;
	struct cg_cgroup_link *saved_link;

	BUG_ON(!root);

	BUG_ON(root->number_of_cgroups != 1);
	BUG_ON(!list_empty(&cgrp->children));

	mutex_lock(&cgroup_mutex);
	mutex_lock(&cgroup_root_mutex);

	ret = rebind_subsystems(root, 0);
	 
	BUG_ON(ret);

	write_lock(&css_set_lock);

	list_for_each_entry_safe(link, saved_link, &cgrp->css_sets,
				 cgrp_link_list) {
		list_del(&link->cg_link_list);
		list_del(&link->cgrp_link_list);
		kfree(link);
	}
	write_unlock(&css_set_lock);

	if (!list_empty(&root->root_list)) {
		list_del(&root->root_list);
		root_count--;
	}

	mutex_unlock(&cgroup_root_mutex);
	mutex_unlock(&cgroup_mutex);

	simple_xattrs_free(&cgrp->xattrs);

	kill_litter_super(sb);
	cgroup_drop_root(root);
}

static struct file_system_type cgroup_fs_type = {
	.name = "cgroup",
	.mount = cgroup_mount,
	.kill_sb = cgroup_kill_sb,
};

static struct kobject *cgroup_kobj;

int cgroup_path(const struct cgroup *cgrp, char *buf, int buflen)
{
	int ret = -ENAMETOOLONG;
	char *start;

	if (!cgrp->parent) {
		if (strlcpy(buf, "/", buflen) >= buflen)
			return -ENAMETOOLONG;
		return 0;
	}

	start = buf + buflen - 1;
	*start = '\0';

	rcu_read_lock();
	do {
		const char *name = cgroup_name(cgrp);
		int len;

		len = strlen(name);
		if ((start -= len) < buf)
			goto out;
		memcpy(start, name, len);

		if (--start < buf)
			goto out;
		*start = '/';

		cgrp = cgrp->parent;
	} while (cgrp->parent);
	ret = 0;
	memmove(buf, start, buf + buflen - start);
out:
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(cgroup_path);

struct task_and_cgroup {
	struct task_struct	*task;
	struct cgroup		*cgrp;
	struct css_set		*cg;
};

struct cgroup_taskset {
	struct task_and_cgroup	single;
	struct flex_array	*tc_array;
	int			tc_array_len;
	int			idx;
	struct cgroup		*cur_cgrp;
};

struct task_struct *cgroup_taskset_first(struct cgroup_taskset *tset)
{
	if (tset->tc_array) {
		tset->idx = 0;
		return cgroup_taskset_next(tset);
	} else {
		tset->cur_cgrp = tset->single.cgrp;
		return tset->single.task;
	}
}
EXPORT_SYMBOL_GPL(cgroup_taskset_first);

struct task_struct *cgroup_taskset_next(struct cgroup_taskset *tset)
{
	struct task_and_cgroup *tc;

	if (!tset->tc_array || tset->idx >= tset->tc_array_len)
		return NULL;

	tc = flex_array_get(tset->tc_array, tset->idx++);
	tset->cur_cgrp = tc->cgrp;
	return tc->task;
}
EXPORT_SYMBOL_GPL(cgroup_taskset_next);

struct cgroup *cgroup_taskset_cur_cgroup(struct cgroup_taskset *tset)
{
	return tset->cur_cgrp;
}
EXPORT_SYMBOL_GPL(cgroup_taskset_cur_cgroup);

int cgroup_taskset_size(struct cgroup_taskset *tset)
{
	return tset->tc_array ? tset->tc_array_len : 1;
}
EXPORT_SYMBOL_GPL(cgroup_taskset_size);

static void cgroup_task_migrate(struct cgroup *oldcgrp,
				struct task_struct *tsk, struct css_set *newcg)
{
	struct css_set *oldcg;

	WARN_ON_ONCE(tsk->flags & PF_EXITING);
	oldcg = tsk->cgroups;

	task_lock(tsk);
	rcu_assign_pointer(tsk->cgroups, newcg);
	task_unlock(tsk);

	write_lock(&css_set_lock);
	if (!list_empty(&tsk->cg_list))
		list_move(&tsk->cg_list, &newcg->tasks);
	write_unlock(&css_set_lock);

	set_bit(CGRP_RELEASABLE, &oldcgrp->flags);
	put_css_set(oldcg);
}

static int cgroup_attach_task(struct cgroup *cgrp, struct task_struct *tsk,
			      bool threadgroup)
{
	int retval, i, group_size;
	struct cgroup_subsys *ss, *failed_ss = NULL;
	struct cgroupfs_root *root = cgrp->root;
	 
	struct task_struct *leader = tsk;
	struct task_and_cgroup *tc;
	struct flex_array *group;
	struct cgroup_taskset tset = { };

	if (threadgroup)
		group_size = get_nr_threads(tsk);
	else
		group_size = 1;
	 
	group = flex_array_alloc(sizeof(*tc), group_size, GFP_KERNEL);
	if (!group)
		return -ENOMEM;
	 
	retval = flex_array_prealloc(group, 0, group_size, GFP_KERNEL);
	if (retval)
		goto out_free_group_list;

	i = 0;
	 
	rcu_read_lock();
	do {
		struct task_and_cgroup ent;

		if (tsk->flags & PF_EXITING)
			goto next;

		BUG_ON(i >= group_size);
		ent.task = tsk;
		ent.cgrp = task_cgroup_from_root(tsk, root);
		 
		if (ent.cgrp == cgrp)
			goto next;
		 
		retval = flex_array_put(group, i, &ent, GFP_ATOMIC);
		BUG_ON(retval != 0);
		i++;
	next:
		if (!threadgroup)
			break;
	} while_each_thread(leader, tsk);
	rcu_read_unlock();
	 
	group_size = i;
	tset.tc_array = group;
	tset.tc_array_len = group_size;

	retval = 0;
	if (!group_size)
		goto out_free_group_list;

	for_each_subsys(root, ss) {
		if (ss->can_attach) {
			retval = ss->can_attach(cgrp, &tset);
			if (retval) {
				failed_ss = ss;
				goto out_cancel_attach;
			}
		}
	}

	for (i = 0; i < group_size; i++) {
		tc = flex_array_get(group, i);
		tc->cg = find_css_set(tc->task->cgroups, cgrp);
		if (!tc->cg) {
			retval = -ENOMEM;
			goto out_put_css_set_refs;
		}
	}

	for (i = 0; i < group_size; i++) {
		tc = flex_array_get(group, i);
		cgroup_task_migrate(tc->cgrp, tc->task, tc->cg);
	}
	 
	for_each_subsys(root, ss) {
		if (ss->attach)
			ss->attach(cgrp, &tset);
	}

	retval = 0;
out_put_css_set_refs:
	if (retval) {
		for (i = 0; i < group_size; i++) {
			tc = flex_array_get(group, i);
			if (!tc->cg)
				break;
			put_css_set(tc->cg);
		}
	}
out_cancel_attach:
	if (retval) {
		for_each_subsys(root, ss) {
			if (ss == failed_ss)
				break;
			if (ss->cancel_attach)
				ss->cancel_attach(cgrp, &tset);
		}
	}
out_free_group_list:
	flex_array_free(group);
	return retval;
}

#if defined(CONFIG_SYNO_LSP_HI3536)
static int cgroup_allow_attach(struct cgroup *cgrp, struct cgroup_taskset *tset)
{
	struct cgroup_subsys *ss;
	int ret;

	for_each_subsys(cgrp->root, ss) {
		if (ss->allow_attach) {
			ret = ss->allow_attach(cgrp, tset);
			if (ret)
				return ret;
		} else {
			return -EACCES;
		}
	}

	return 0;
}

int subsys_cgroup_allow_attach(struct cgroup *cgrp, struct cgroup_taskset *tset)
{
	const struct cred *cred = current_cred(), *tcred;
	struct task_struct *task;

	if (capable(CAP_SYS_NICE))
		return 0;

	cgroup_taskset_for_each(task, cgrp, tset) {
		tcred = __task_cred(task);

		if (current != task && cred->euid != tcred->uid &&
		    cred->euid != tcred->suid)
			return -EACCES;
	}

	return 0;
}
#endif  

static int attach_task_by_pid(struct cgroup *cgrp, u64 pid, bool threadgroup)
{
	struct task_struct *tsk;
	const struct cred *cred = current_cred(), *tcred;
	int ret;

	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;

retry_find_task:
	rcu_read_lock();
	if (pid) {
		tsk = find_task_by_vpid(pid);
		if (!tsk) {
			rcu_read_unlock();
			ret= -ESRCH;
			goto out_unlock_cgroup;
		}
		 
		tcred = __task_cred(tsk);
		if (!uid_eq(cred->euid, GLOBAL_ROOT_UID) &&
		    !uid_eq(cred->euid, tcred->uid) &&
		    !uid_eq(cred->euid, tcred->suid)) {
#if defined(CONFIG_SYNO_LSP_HI3536)
			 
			struct cgroup_taskset tset = { };
			tset.single.task = tsk;
			tset.single.cgrp = cgrp;
			ret = cgroup_allow_attach(cgrp, &tset);
			if (ret) {
				rcu_read_unlock();
				goto out_unlock_cgroup;
			}
#else  
			rcu_read_unlock();
			ret = -EACCES;
			goto out_unlock_cgroup;
#endif  
		}
	} else
		tsk = current;

	if (threadgroup)
		tsk = tsk->group_leader;

	if (tsk == kthreadd_task || (tsk->flags & PF_NO_SETAFFINITY)) {
		ret = -EINVAL;
		rcu_read_unlock();
		goto out_unlock_cgroup;
	}

	get_task_struct(tsk);
	rcu_read_unlock();

	threadgroup_lock(tsk);
	if (threadgroup) {
		if (!thread_group_leader(tsk)) {
			 
			threadgroup_unlock(tsk);
			put_task_struct(tsk);
			goto retry_find_task;
		}
	}

	ret = cgroup_attach_task(cgrp, tsk, threadgroup);

	threadgroup_unlock(tsk);

	put_task_struct(tsk);
out_unlock_cgroup:
	mutex_unlock(&cgroup_mutex);
	return ret;
}

int cgroup_attach_task_all(struct task_struct *from, struct task_struct *tsk)
{
	struct cgroupfs_root *root;
	int retval = 0;

	mutex_lock(&cgroup_mutex);
	for_each_active_root(root) {
		struct cgroup *from_cg = task_cgroup_from_root(from, root);

		retval = cgroup_attach_task(from_cg, tsk, false);
		if (retval)
			break;
	}
	mutex_unlock(&cgroup_mutex);

	return retval;
}
EXPORT_SYMBOL_GPL(cgroup_attach_task_all);

static int cgroup_tasks_write(struct cgroup *cgrp, struct cftype *cft, u64 pid)
{
	return attach_task_by_pid(cgrp, pid, false);
}

static int cgroup_procs_write(struct cgroup *cgrp, struct cftype *cft, u64 tgid)
{
	return attach_task_by_pid(cgrp, tgid, true);
}

static int cgroup_release_agent_write(struct cgroup *cgrp, struct cftype *cft,
				      const char *buffer)
{
	BUILD_BUG_ON(sizeof(cgrp->root->release_agent_path) < PATH_MAX);
	if (strlen(buffer) >= PATH_MAX)
		return -EINVAL;
	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;
	mutex_lock(&cgroup_root_mutex);
	strcpy(cgrp->root->release_agent_path, buffer);
	mutex_unlock(&cgroup_root_mutex);
	mutex_unlock(&cgroup_mutex);
	return 0;
}

static int cgroup_release_agent_show(struct cgroup *cgrp, struct cftype *cft,
				     struct seq_file *seq)
{
	if (!cgroup_lock_live_group(cgrp))
		return -ENODEV;
	seq_puts(seq, cgrp->root->release_agent_path);
	seq_putc(seq, '\n');
	mutex_unlock(&cgroup_mutex);
	return 0;
}

static int cgroup_sane_behavior_show(struct cgroup *cgrp, struct cftype *cft,
				     struct seq_file *seq)
{
	seq_printf(seq, "%d\n", cgroup_sane_behavior(cgrp));
	return 0;
}

#define CGROUP_LOCAL_BUFFER_SIZE 64

static ssize_t cgroup_write_X64(struct cgroup *cgrp, struct cftype *cft,
				struct file *file,
				const char __user *userbuf,
				size_t nbytes, loff_t *unused_ppos)
{
	char buffer[CGROUP_LOCAL_BUFFER_SIZE];
	int retval = 0;
	char *end;

	if (!nbytes)
		return -EINVAL;
	if (nbytes >= sizeof(buffer))
		return -E2BIG;
	if (copy_from_user(buffer, userbuf, nbytes))
		return -EFAULT;

	buffer[nbytes] = 0;      
	if (cft->write_u64) {
		u64 val = simple_strtoull(strstrip(buffer), &end, 0);
		if (*end)
			return -EINVAL;
		retval = cft->write_u64(cgrp, cft, val);
	} else {
		s64 val = simple_strtoll(strstrip(buffer), &end, 0);
		if (*end)
			return -EINVAL;
		retval = cft->write_s64(cgrp, cft, val);
	}
	if (!retval)
		retval = nbytes;
	return retval;
}

static ssize_t cgroup_write_string(struct cgroup *cgrp, struct cftype *cft,
				   struct file *file,
				   const char __user *userbuf,
				   size_t nbytes, loff_t *unused_ppos)
{
	char local_buffer[CGROUP_LOCAL_BUFFER_SIZE];
	int retval = 0;
	size_t max_bytes = cft->max_write_len;
	char *buffer = local_buffer;

	if (!max_bytes)
		max_bytes = sizeof(local_buffer) - 1;
	if (nbytes >= max_bytes)
		return -E2BIG;
	 
	if (nbytes >= sizeof(local_buffer)) {
		buffer = kmalloc(nbytes + 1, GFP_KERNEL);
		if (buffer == NULL)
			return -ENOMEM;
	}
	if (nbytes && copy_from_user(buffer, userbuf, nbytes)) {
		retval = -EFAULT;
		goto out;
	}

	buffer[nbytes] = 0;      
	retval = cft->write_string(cgrp, cft, strstrip(buffer));
	if (!retval)
		retval = nbytes;
out:
	if (buffer != local_buffer)
		kfree(buffer);
	return retval;
}

static ssize_t cgroup_file_write(struct file *file, const char __user *buf,
						size_t nbytes, loff_t *ppos)
{
	struct cftype *cft = __d_cft(file->f_dentry);
	struct cgroup *cgrp = __d_cgrp(file->f_dentry->d_parent);

	if (cgroup_is_removed(cgrp))
		return -ENODEV;
	if (cft->write)
		return cft->write(cgrp, cft, file, buf, nbytes, ppos);
	if (cft->write_u64 || cft->write_s64)
		return cgroup_write_X64(cgrp, cft, file, buf, nbytes, ppos);
	if (cft->write_string)
		return cgroup_write_string(cgrp, cft, file, buf, nbytes, ppos);
	if (cft->trigger) {
		int ret = cft->trigger(cgrp, (unsigned int)cft->private);
		return ret ? ret : nbytes;
	}
	return -EINVAL;
}

static ssize_t cgroup_read_u64(struct cgroup *cgrp, struct cftype *cft,
			       struct file *file,
			       char __user *buf, size_t nbytes,
			       loff_t *ppos)
{
	char tmp[CGROUP_LOCAL_BUFFER_SIZE];
	u64 val = cft->read_u64(cgrp, cft);
	int len = sprintf(tmp, "%llu\n", (unsigned long long) val);

	return simple_read_from_buffer(buf, nbytes, ppos, tmp, len);
}

static ssize_t cgroup_read_s64(struct cgroup *cgrp, struct cftype *cft,
			       struct file *file,
			       char __user *buf, size_t nbytes,
			       loff_t *ppos)
{
	char tmp[CGROUP_LOCAL_BUFFER_SIZE];
	s64 val = cft->read_s64(cgrp, cft);
	int len = sprintf(tmp, "%lld\n", (long long) val);

	return simple_read_from_buffer(buf, nbytes, ppos, tmp, len);
}

static ssize_t cgroup_file_read(struct file *file, char __user *buf,
				   size_t nbytes, loff_t *ppos)
{
	struct cftype *cft = __d_cft(file->f_dentry);
	struct cgroup *cgrp = __d_cgrp(file->f_dentry->d_parent);

	if (cgroup_is_removed(cgrp))
		return -ENODEV;

	if (cft->read)
		return cft->read(cgrp, cft, file, buf, nbytes, ppos);
	if (cft->read_u64)
		return cgroup_read_u64(cgrp, cft, file, buf, nbytes, ppos);
	if (cft->read_s64)
		return cgroup_read_s64(cgrp, cft, file, buf, nbytes, ppos);
	return -EINVAL;
}

struct cgroup_seqfile_state {
	struct cftype *cft;
	struct cgroup *cgroup;
};

static int cgroup_map_add(struct cgroup_map_cb *cb, const char *key, u64 value)
{
	struct seq_file *sf = cb->state;
	return seq_printf(sf, "%s %llu\n", key, (unsigned long long)value);
}

static int cgroup_seqfile_show(struct seq_file *m, void *arg)
{
	struct cgroup_seqfile_state *state = m->private;
	struct cftype *cft = state->cft;
	if (cft->read_map) {
		struct cgroup_map_cb cb = {
			.fill = cgroup_map_add,
			.state = m,
		};
		return cft->read_map(state->cgroup, cft, &cb);
	}
	return cft->read_seq_string(state->cgroup, cft, m);
}

static int cgroup_seqfile_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	kfree(seq->private);
	return single_release(inode, file);
}

static const struct file_operations cgroup_seqfile_operations = {
	.read = seq_read,
	.write = cgroup_file_write,
	.llseek = seq_lseek,
	.release = cgroup_seqfile_release,
};

static int cgroup_file_open(struct inode *inode, struct file *file)
{
	int err;
	struct cftype *cft;

	err = generic_file_open(inode, file);
	if (err)
		return err;
	cft = __d_cft(file->f_dentry);

	if (cft->read_map || cft->read_seq_string) {
		struct cgroup_seqfile_state *state =
			kzalloc(sizeof(*state), GFP_USER);
		if (!state)
			return -ENOMEM;
		state->cft = cft;
		state->cgroup = __d_cgrp(file->f_dentry->d_parent);
		file->f_op = &cgroup_seqfile_operations;
		err = single_open(file, cgroup_seqfile_show, state);
		if (err < 0)
			kfree(state);
	} else if (cft->open)
		err = cft->open(inode, file);
	else
		err = 0;

	return err;
}

static int cgroup_file_release(struct inode *inode, struct file *file)
{
	struct cftype *cft = __d_cft(file->f_dentry);
	if (cft->release)
		return cft->release(inode, file);
	return 0;
}

static int cgroup_rename(struct inode *old_dir, struct dentry *old_dentry,
			    struct inode *new_dir, struct dentry *new_dentry)
{
	int ret;
	struct cgroup_name *name, *old_name;
	struct cgroup *cgrp;

	lockdep_assert_held(&old_dir->i_mutex);

	if (!S_ISDIR(old_dentry->d_inode->i_mode))
		return -ENOTDIR;
	if (new_dentry->d_inode)
		return -EEXIST;
	if (old_dir != new_dir)
		return -EIO;

	cgrp = __d_cgrp(old_dentry);

	name = cgroup_alloc_name(new_dentry);
	if (!name)
		return -ENOMEM;

	ret = simple_rename(old_dir, old_dentry, new_dir, new_dentry);
	if (ret) {
		kfree(name);
		return ret;
	}

	old_name = cgrp->name;
	rcu_assign_pointer(cgrp->name, name);

	kfree_rcu(old_name, rcu_head);
	return 0;
}

static struct simple_xattrs *__d_xattrs(struct dentry *dentry)
{
	if (S_ISDIR(dentry->d_inode->i_mode))
		return &__d_cgrp(dentry)->xattrs;
	else
		return &__d_cfe(dentry)->xattrs;
}

static inline int xattr_enabled(struct dentry *dentry)
{
	struct cgroupfs_root *root = dentry->d_sb->s_fs_info;
	return root->flags & CGRP_ROOT_XATTR;
}

static bool is_valid_xattr(const char *name)
{
	if (!strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN) ||
	    !strncmp(name, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN))
		return true;
	return false;
}

static int cgroup_setxattr(struct dentry *dentry, const char *name,
			   const void *val, size_t size, int flags)
{
	if (!xattr_enabled(dentry))
		return -EOPNOTSUPP;
	if (!is_valid_xattr(name))
		return -EINVAL;
	return simple_xattr_set(__d_xattrs(dentry), name, val, size, flags);
}

static int cgroup_removexattr(struct dentry *dentry, const char *name)
{
	if (!xattr_enabled(dentry))
		return -EOPNOTSUPP;
	if (!is_valid_xattr(name))
		return -EINVAL;
	return simple_xattr_remove(__d_xattrs(dentry), name);
}

static ssize_t cgroup_getxattr(struct dentry *dentry, const char *name,
			       void *buf, size_t size)
{
	if (!xattr_enabled(dentry))
		return -EOPNOTSUPP;
	if (!is_valid_xattr(name))
		return -EINVAL;
	return simple_xattr_get(__d_xattrs(dentry), name, buf, size);
}

static ssize_t cgroup_listxattr(struct dentry *dentry, char *buf, size_t size)
{
	if (!xattr_enabled(dentry))
		return -EOPNOTSUPP;
	return simple_xattr_list(__d_xattrs(dentry), buf, size);
}

static const struct file_operations cgroup_file_operations = {
	.read = cgroup_file_read,
	.write = cgroup_file_write,
	.llseek = generic_file_llseek,
	.open = cgroup_file_open,
	.release = cgroup_file_release,
};

static const struct inode_operations cgroup_file_inode_operations = {
	.setxattr = cgroup_setxattr,
	.getxattr = cgroup_getxattr,
	.listxattr = cgroup_listxattr,
	.removexattr = cgroup_removexattr,
};

static const struct inode_operations cgroup_dir_inode_operations = {
	.lookup = cgroup_lookup,
	.mkdir = cgroup_mkdir,
	.rmdir = cgroup_rmdir,
	.rename = cgroup_rename,
	.setxattr = cgroup_setxattr,
	.getxattr = cgroup_getxattr,
	.listxattr = cgroup_listxattr,
	.removexattr = cgroup_removexattr,
};

static struct dentry *cgroup_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	d_add(dentry, NULL);
	return NULL;
}

static inline struct cftype *__file_cft(struct file *file)
{
	if (file_inode(file)->i_fop != &cgroup_file_operations)
		return ERR_PTR(-EINVAL);
	return __d_cft(file->f_dentry);
}

static int cgroup_create_file(struct dentry *dentry, umode_t mode,
				struct super_block *sb)
{
	struct inode *inode;

	if (!dentry)
		return -ENOENT;
	if (dentry->d_inode)
		return -EEXIST;

	inode = cgroup_new_inode(mode, sb);
	if (!inode)
		return -ENOMEM;

	if (S_ISDIR(mode)) {
		inode->i_op = &cgroup_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;

		inc_nlink(inode);
		inc_nlink(dentry->d_parent->d_inode);

		WARN_ON_ONCE(!mutex_trylock(&inode->i_mutex));
	} else if (S_ISREG(mode)) {
		inode->i_size = 0;
		inode->i_fop = &cgroup_file_operations;
		inode->i_op = &cgroup_file_inode_operations;
	}
	d_instantiate(dentry, inode);
	dget(dentry);	 
	return 0;
}

static umode_t cgroup_file_mode(const struct cftype *cft)
{
	umode_t mode = 0;

	if (cft->mode)
		return cft->mode;

	if (cft->read || cft->read_u64 || cft->read_s64 ||
	    cft->read_map || cft->read_seq_string)
		mode |= S_IRUGO;

	if (cft->write || cft->write_u64 || cft->write_s64 ||
	    cft->write_string || cft->trigger)
		mode |= S_IWUSR;

	return mode;
}

static int cgroup_add_file(struct cgroup *cgrp, struct cgroup_subsys *subsys,
			   struct cftype *cft)
{
	struct dentry *dir = cgrp->dentry;
	struct cgroup *parent = __d_cgrp(dir);
	struct dentry *dentry;
	struct cfent *cfe;
	int error;
	umode_t mode;
	char name[MAX_CGROUP_TYPE_NAMELEN + MAX_CFTYPE_NAME + 2] = { 0 };

	if (subsys && !(cgrp->root->flags & CGRP_ROOT_NOPREFIX)) {
		strcpy(name, subsys->name);
		strcat(name, ".");
	}
	strcat(name, cft->name);

	BUG_ON(!mutex_is_locked(&dir->d_inode->i_mutex));

	cfe = kzalloc(sizeof(*cfe), GFP_KERNEL);
	if (!cfe)
		return -ENOMEM;

	dentry = lookup_one_len(name, dir, strlen(name));
	if (IS_ERR(dentry)) {
		error = PTR_ERR(dentry);
		goto out;
	}

	cfe->type = (void *)cft;
	cfe->dentry = dentry;
	dentry->d_fsdata = cfe;
	simple_xattrs_init(&cfe->xattrs);

	mode = cgroup_file_mode(cft);
	error = cgroup_create_file(dentry, mode | S_IFREG, cgrp->root->sb);
	if (!error) {
		list_add_tail(&cfe->node, &parent->files);
		cfe = NULL;
	}
	dput(dentry);
out:
	kfree(cfe);
	return error;
}

static int cgroup_addrm_files(struct cgroup *cgrp, struct cgroup_subsys *subsys,
			      struct cftype cfts[], bool is_add)
{
	struct cftype *cft;
	int err, ret = 0;

	for (cft = cfts; cft->name[0] != '\0'; cft++) {
		 
		if ((cft->flags & CFTYPE_INSANE) && cgroup_sane_behavior(cgrp))
			continue;
		if ((cft->flags & CFTYPE_NOT_ON_ROOT) && !cgrp->parent)
			continue;
		if ((cft->flags & CFTYPE_ONLY_ON_ROOT) && cgrp->parent)
			continue;

		if (is_add) {
			err = cgroup_add_file(cgrp, subsys, cft);
			if (err)
				pr_warn("cgroup_addrm_files: failed to add %s, err=%d\n",
					cft->name, err);
			ret = err;
		} else {
			cgroup_rm_file(cgrp, cft);
		}
	}
	return ret;
}

static DEFINE_MUTEX(cgroup_cft_mutex);

static void cgroup_cfts_prepare(void)
	__acquires(&cgroup_cft_mutex) __acquires(&cgroup_mutex)
{
	 
	mutex_lock(&cgroup_cft_mutex);
	mutex_lock(&cgroup_mutex);
}

static void cgroup_cfts_commit(struct cgroup_subsys *ss,
			       struct cftype *cfts, bool is_add)
	__releases(&cgroup_mutex) __releases(&cgroup_cft_mutex)
{
	LIST_HEAD(pending);
	struct cgroup *cgrp, *n;
	struct super_block *sb = ss->root->sb;

	if (cfts && ss->root != &rootnode &&
	    atomic_inc_not_zero(&sb->s_active)) {
		list_for_each_entry(cgrp, &ss->root->allcg_list, allcg_node) {
			dget(cgrp->dentry);
			list_add_tail(&cgrp->cft_q_node, &pending);
		}
	} else {
		sb = NULL;
	}

	mutex_unlock(&cgroup_mutex);

	list_for_each_entry_safe(cgrp, n, &pending, cft_q_node) {
		struct inode *inode = cgrp->dentry->d_inode;

		mutex_lock(&inode->i_mutex);
		mutex_lock(&cgroup_mutex);
		if (!cgroup_is_removed(cgrp))
			cgroup_addrm_files(cgrp, ss, cfts, is_add);
		mutex_unlock(&cgroup_mutex);
		mutex_unlock(&inode->i_mutex);

		list_del_init(&cgrp->cft_q_node);
		dput(cgrp->dentry);
	}

	if (sb)
		deactivate_super(sb);

	mutex_unlock(&cgroup_cft_mutex);
}

int cgroup_add_cftypes(struct cgroup_subsys *ss, struct cftype *cfts)
{
	struct cftype_set *set;

	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		return -ENOMEM;

	cgroup_cfts_prepare();
	set->cfts = cfts;
	list_add_tail(&set->node, &ss->cftsets);
	cgroup_cfts_commit(ss, cfts, true);

	return 0;
}
EXPORT_SYMBOL_GPL(cgroup_add_cftypes);

int cgroup_rm_cftypes(struct cgroup_subsys *ss, struct cftype *cfts)
{
	struct cftype_set *set;

	cgroup_cfts_prepare();

	list_for_each_entry(set, &ss->cftsets, node) {
		if (set->cfts == cfts) {
			list_del_init(&set->node);
			cgroup_cfts_commit(ss, cfts, false);
			return 0;
		}
	}

	cgroup_cfts_commit(ss, NULL, false);
	return -ENOENT;
}

int cgroup_task_count(const struct cgroup *cgrp)
{
	int count = 0;
	struct cg_cgroup_link *link;

	read_lock(&css_set_lock);
	list_for_each_entry(link, &cgrp->css_sets, cgrp_link_list) {
		count += atomic_read(&link->cg->refcount);
	}
	read_unlock(&css_set_lock);
	return count;
}

static void cgroup_advance_iter(struct cgroup *cgrp,
				struct cgroup_iter *it)
{
	struct list_head *l = it->cg_link;
	struct cg_cgroup_link *link;
	struct css_set *cg;

	do {
		l = l->next;
		if (l == &cgrp->css_sets) {
			it->cg_link = NULL;
			return;
		}
		link = list_entry(l, struct cg_cgroup_link, cgrp_link_list);
		cg = link->cg;
	} while (list_empty(&cg->tasks));
	it->cg_link = l;
	it->task = cg->tasks.next;
}

static void cgroup_enable_task_cg_lists(void)
{
	struct task_struct *p, *g;
	write_lock(&css_set_lock);
	use_task_css_set_links = 1;
	 
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		task_lock(p);
		 
		if (!(p->flags & PF_EXITING) && list_empty(&p->cg_list))
			list_add(&p->cg_list, &p->cgroups->tasks);
		task_unlock(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
	write_unlock(&css_set_lock);
}

struct cgroup *cgroup_next_descendant_pre(struct cgroup *pos,
					  struct cgroup *cgroup)
{
	struct cgroup *next;

	WARN_ON_ONCE(!rcu_read_lock_held());

	if (!pos)
		pos = cgroup;

	next = list_first_or_null_rcu(&pos->children, struct cgroup, sibling);
	if (next)
		return next;

	while (pos != cgroup) {
		next = list_entry_rcu(pos->sibling.next, struct cgroup,
				      sibling);
		if (&next->sibling != &pos->parent->children)
			return next;

		pos = pos->parent;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(cgroup_next_descendant_pre);

struct cgroup *cgroup_rightmost_descendant(struct cgroup *pos)
{
	struct cgroup *last, *tmp;

	WARN_ON_ONCE(!rcu_read_lock_held());

	do {
		last = pos;
		 
		pos = NULL;
		list_for_each_entry_rcu(tmp, &last->children, sibling)
			pos = tmp;
	} while (pos);

	return last;
}
EXPORT_SYMBOL_GPL(cgroup_rightmost_descendant);

static struct cgroup *cgroup_leftmost_descendant(struct cgroup *pos)
{
	struct cgroup *last;

	do {
		last = pos;
		pos = list_first_or_null_rcu(&pos->children, struct cgroup,
					     sibling);
	} while (pos);

	return last;
}

struct cgroup *cgroup_next_descendant_post(struct cgroup *pos,
					   struct cgroup *cgroup)
{
	struct cgroup *next;

	WARN_ON_ONCE(!rcu_read_lock_held());

	if (!pos) {
		next = cgroup_leftmost_descendant(cgroup);
		return next != cgroup ? next : NULL;
	}

	next = list_entry_rcu(pos->sibling.next, struct cgroup, sibling);
	if (&next->sibling != &pos->parent->children)
		return cgroup_leftmost_descendant(next);

	next = pos->parent;
	return next != cgroup ? next : NULL;
}
EXPORT_SYMBOL_GPL(cgroup_next_descendant_post);

void cgroup_iter_start(struct cgroup *cgrp, struct cgroup_iter *it)
	__acquires(css_set_lock)
{
	 
	if (!use_task_css_set_links)
		cgroup_enable_task_cg_lists();

	read_lock(&css_set_lock);
	it->cg_link = &cgrp->css_sets;
	cgroup_advance_iter(cgrp, it);
}

struct task_struct *cgroup_iter_next(struct cgroup *cgrp,
					struct cgroup_iter *it)
{
	struct task_struct *res;
	struct list_head *l = it->task;
	struct cg_cgroup_link *link;

	if (!it->cg_link)
		return NULL;
	res = list_entry(l, struct task_struct, cg_list);
	 
	l = l->next;
	link = list_entry(it->cg_link, struct cg_cgroup_link, cgrp_link_list);
	if (l == &link->cg->tasks) {
		 
		cgroup_advance_iter(cgrp, it);
	} else {
		it->task = l;
	}
	return res;
}

void cgroup_iter_end(struct cgroup *cgrp, struct cgroup_iter *it)
	__releases(css_set_lock)
{
	read_unlock(&css_set_lock);
}

static inline int started_after_time(struct task_struct *t1,
				     struct timespec *time,
				     struct task_struct *t2)
{
	int start_diff = timespec_compare(&t1->start_time, time);
	if (start_diff > 0) {
		return 1;
	} else if (start_diff < 0) {
		return 0;
	} else {
		 
		return t1 > t2;
	}
}

static inline int started_after(void *p1, void *p2)
{
	struct task_struct *t1 = p1;
	struct task_struct *t2 = p2;
	return started_after_time(t1, &t2->start_time, t2);
}

int cgroup_scan_tasks(struct cgroup_scanner *scan)
{
	int retval, i;
	struct cgroup_iter it;
	struct task_struct *p, *dropped;
	 
	struct task_struct *latest_task = NULL;
	struct ptr_heap tmp_heap;
	struct ptr_heap *heap;
	struct timespec latest_time = { 0, 0 };

	if (scan->heap) {
		 
		heap = scan->heap;
		heap->gt = &started_after;
	} else {
		 
		heap = &tmp_heap;
		retval = heap_init(heap, PAGE_SIZE, GFP_KERNEL, &started_after);
		if (retval)
			 
			return retval;
	}

 again:
	 
	heap->size = 0;
	cgroup_iter_start(scan->cg, &it);
	while ((p = cgroup_iter_next(scan->cg, &it))) {
		 
		if (scan->test_task && !scan->test_task(p, scan))
			continue;
		 
		if (!started_after_time(p, &latest_time, latest_task))
			continue;
		dropped = heap_insert(heap, p);
		if (dropped == NULL) {
			 
			get_task_struct(p);
		} else if (dropped != p) {
			 
			get_task_struct(p);
			put_task_struct(dropped);
		}
		 
	}
	cgroup_iter_end(scan->cg, &it);

	if (heap->size) {
		for (i = 0; i < heap->size; i++) {
			struct task_struct *q = heap->ptrs[i];
			if (i == 0) {
				latest_time = q->start_time;
				latest_task = q;
			}
			 
			scan->process_task(q, scan);
			put_task_struct(q);
		}
		 
		goto again;
	}
	if (heap == &tmp_heap)
		heap_free(&tmp_heap);
	return 0;
}

static void cgroup_transfer_one_task(struct task_struct *task,
				     struct cgroup_scanner *scan)
{
	struct cgroup *new_cgroup = scan->data;

	mutex_lock(&cgroup_mutex);
	cgroup_attach_task(new_cgroup, task, false);
	mutex_unlock(&cgroup_mutex);
}

int cgroup_transfer_tasks(struct cgroup *to, struct cgroup *from)
{
	struct cgroup_scanner scan;

	scan.cg = from;
	scan.test_task = NULL;  
	scan.process_task = cgroup_transfer_one_task;
	scan.heap = NULL;
	scan.data = to;

	return cgroup_scan_tasks(&scan);
}

enum cgroup_filetype {
	CGROUP_FILE_PROCS,
	CGROUP_FILE_TASKS,
};

struct cgroup_pidlist {
	 
	struct { enum cgroup_filetype type; struct pid_namespace *ns; } key;
	 
	pid_t *list;
	 
	int length;
	 
	int use_count;
	 
	struct list_head links;
	 
	struct cgroup *owner;
	 
	struct rw_semaphore mutex;
};

#define PIDLIST_TOO_LARGE(c) ((c) * sizeof(pid_t) > (PAGE_SIZE * 2))
static void *pidlist_allocate(int count)
{
	if (PIDLIST_TOO_LARGE(count))
		return vmalloc(count * sizeof(pid_t));
	else
		return kmalloc(count * sizeof(pid_t), GFP_KERNEL);
}
static void pidlist_free(void *p)
{
	if (is_vmalloc_addr(p))
		vfree(p);
	else
		kfree(p);
}

static int pidlist_uniq(pid_t *list, int length)
{
	int src, dest = 1;

	if (length == 0 || length == 1)
		return length;
	 
	for (src = 1; src < length; src++) {
		 
		while (list[src] == list[src-1]) {
			src++;
			if (src == length)
				goto after;
		}
		 
		list[dest] = list[src];
		dest++;
	}
after:
	return dest;
}

static int cmppid(const void *a, const void *b)
{
	return *(pid_t *)a - *(pid_t *)b;
}

static struct cgroup_pidlist *cgroup_pidlist_find(struct cgroup *cgrp,
						  enum cgroup_filetype type)
{
	struct cgroup_pidlist *l;
	 
	struct pid_namespace *ns = task_active_pid_ns(current);

	mutex_lock(&cgrp->pidlist_mutex);
	list_for_each_entry(l, &cgrp->pidlists, links) {
		if (l->key.type == type && l->key.ns == ns) {
			 
			down_write(&l->mutex);
			mutex_unlock(&cgrp->pidlist_mutex);
			return l;
		}
	}
	 
	l = kmalloc(sizeof(struct cgroup_pidlist), GFP_KERNEL);
	if (!l) {
		mutex_unlock(&cgrp->pidlist_mutex);
		return l;
	}
	init_rwsem(&l->mutex);
	down_write(&l->mutex);
	l->key.type = type;
	l->key.ns = get_pid_ns(ns);
	l->use_count = 0;  
	l->list = NULL;
	l->owner = cgrp;
	list_add(&l->links, &cgrp->pidlists);
	mutex_unlock(&cgrp->pidlist_mutex);
	return l;
}

static int pidlist_array_load(struct cgroup *cgrp, enum cgroup_filetype type,
			      struct cgroup_pidlist **lp)
{
	pid_t *array;
	int length;
	int pid, n = 0;  
	struct cgroup_iter it;
	struct task_struct *tsk;
	struct cgroup_pidlist *l;

	length = cgroup_task_count(cgrp);
	array = pidlist_allocate(length);
	if (!array)
		return -ENOMEM;
	 
	cgroup_iter_start(cgrp, &it);
	while ((tsk = cgroup_iter_next(cgrp, &it))) {
		if (unlikely(n == length))
			break;
		 
		if (type == CGROUP_FILE_PROCS)
			pid = task_tgid_vnr(tsk);
		else
			pid = task_pid_vnr(tsk);
		if (pid > 0)  
			array[n++] = pid;
	}
	cgroup_iter_end(cgrp, &it);
	length = n;
	 
	sort(array, length, sizeof(pid_t), cmppid, NULL);
	if (type == CGROUP_FILE_PROCS)
		length = pidlist_uniq(array, length);
	l = cgroup_pidlist_find(cgrp, type);
	if (!l) {
		pidlist_free(array);
		return -ENOMEM;
	}
	 
	pidlist_free(l->list);
	l->list = array;
	l->length = length;
	l->use_count++;
	up_write(&l->mutex);
	*lp = l;
	return 0;
}

int cgroupstats_build(struct cgroupstats *stats, struct dentry *dentry)
{
	int ret = -EINVAL;
	struct cgroup *cgrp;
	struct cgroup_iter it;
	struct task_struct *tsk;

	if (dentry->d_sb->s_op != &cgroup_ops ||
	    !S_ISDIR(dentry->d_inode->i_mode))
		 goto err;

	ret = 0;
	cgrp = dentry->d_fsdata;

	cgroup_iter_start(cgrp, &it);
	while ((tsk = cgroup_iter_next(cgrp, &it))) {
		switch (tsk->state) {
		case TASK_RUNNING:
			stats->nr_running++;
			break;
		case TASK_INTERRUPTIBLE:
			stats->nr_sleeping++;
			break;
		case TASK_UNINTERRUPTIBLE:
			stats->nr_uninterruptible++;
			break;
		case TASK_STOPPED:
			stats->nr_stopped++;
			break;
		default:
			if (delayacct_is_task_waiting_on_io(tsk))
				stats->nr_io_wait++;
			break;
		}
	}
	cgroup_iter_end(cgrp, &it);

err:
	return ret;
}

static void *cgroup_pidlist_start(struct seq_file *s, loff_t *pos)
{
	 
	struct cgroup_pidlist *l = s->private;
	int index = 0, pid = *pos;
	int *iter;

	down_read(&l->mutex);
	if (pid) {
		int end = l->length;

		while (index < end) {
			int mid = (index + end) / 2;
			if (l->list[mid] == pid) {
				index = mid;
				break;
			} else if (l->list[mid] <= pid)
				index = mid + 1;
			else
				end = mid;
		}
	}
	 
	if (index >= l->length)
		return NULL;
	 
	iter = l->list + index;
	*pos = *iter;
	return iter;
}

static void cgroup_pidlist_stop(struct seq_file *s, void *v)
{
	struct cgroup_pidlist *l = s->private;
	up_read(&l->mutex);
}

static void *cgroup_pidlist_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct cgroup_pidlist *l = s->private;
	pid_t *p = v;
	pid_t *end = l->list + l->length;
	 
	p++;
	if (p >= end) {
		return NULL;
	} else {
		*pos = *p;
		return p;
	}
}

static int cgroup_pidlist_show(struct seq_file *s, void *v)
{
	return seq_printf(s, "%d\n", *(int *)v);
}

static const struct seq_operations cgroup_pidlist_seq_operations = {
	.start = cgroup_pidlist_start,
	.stop = cgroup_pidlist_stop,
	.next = cgroup_pidlist_next,
	.show = cgroup_pidlist_show,
};

static void cgroup_release_pid_array(struct cgroup_pidlist *l)
{
	 
	mutex_lock(&l->owner->pidlist_mutex);
	down_write(&l->mutex);
	BUG_ON(!l->use_count);
	if (!--l->use_count) {
		 
		list_del(&l->links);
		mutex_unlock(&l->owner->pidlist_mutex);
		pidlist_free(l->list);
		put_pid_ns(l->key.ns);
		up_write(&l->mutex);
		kfree(l);
		return;
	}
	mutex_unlock(&l->owner->pidlist_mutex);
	up_write(&l->mutex);
}

static int cgroup_pidlist_release(struct inode *inode, struct file *file)
{
	struct cgroup_pidlist *l;
	if (!(file->f_mode & FMODE_READ))
		return 0;
	 
	l = ((struct seq_file *)file->private_data)->private;
	cgroup_release_pid_array(l);
	return seq_release(inode, file);
}

static const struct file_operations cgroup_pidlist_operations = {
	.read = seq_read,
	.llseek = seq_lseek,
	.write = cgroup_file_write,
	.release = cgroup_pidlist_release,
};

static int cgroup_pidlist_open(struct file *file, enum cgroup_filetype type)
{
	struct cgroup *cgrp = __d_cgrp(file->f_dentry->d_parent);
	struct cgroup_pidlist *l;
	int retval;

	if (!(file->f_mode & FMODE_READ))
		return 0;

	retval = pidlist_array_load(cgrp, type, &l);
	if (retval)
		return retval;
	 
	file->f_op = &cgroup_pidlist_operations;

	retval = seq_open(file, &cgroup_pidlist_seq_operations);
	if (retval) {
		cgroup_release_pid_array(l);
		return retval;
	}
	((struct seq_file *)file->private_data)->private = l;
	return 0;
}
static int cgroup_tasks_open(struct inode *unused, struct file *file)
{
	return cgroup_pidlist_open(file, CGROUP_FILE_TASKS);
}
static int cgroup_procs_open(struct inode *unused, struct file *file)
{
	return cgroup_pidlist_open(file, CGROUP_FILE_PROCS);
}

static u64 cgroup_read_notify_on_release(struct cgroup *cgrp,
					    struct cftype *cft)
{
	return notify_on_release(cgrp);
}

static int cgroup_write_notify_on_release(struct cgroup *cgrp,
					  struct cftype *cft,
					  u64 val)
{
	clear_bit(CGRP_RELEASABLE, &cgrp->flags);
	if (val)
		set_bit(CGRP_NOTIFY_ON_RELEASE, &cgrp->flags);
	else
		clear_bit(CGRP_NOTIFY_ON_RELEASE, &cgrp->flags);
	return 0;
}

static void cgroup_dput(struct cgroup *cgrp)
{
	struct super_block *sb = cgrp->root->sb;

	atomic_inc(&sb->s_active);
	dput(cgrp->dentry);
	deactivate_super(sb);
}

static void cgroup_event_remove(struct work_struct *work)
{
	struct cgroup_event *event = container_of(work, struct cgroup_event,
			remove);
	struct cgroup *cgrp = event->cgrp;

	remove_wait_queue(event->wqh, &event->wait);

	event->cft->unregister_event(cgrp, event->cft, event->eventfd);

	eventfd_signal(event->eventfd, 1);

	eventfd_ctx_put(event->eventfd);
	kfree(event);
	cgroup_dput(cgrp);
}

static int cgroup_event_wake(wait_queue_t *wait, unsigned mode,
		int sync, void *key)
{
	struct cgroup_event *event = container_of(wait,
			struct cgroup_event, wait);
	struct cgroup *cgrp = event->cgrp;
	unsigned long flags = (unsigned long)key;

	if (flags & POLLHUP) {
		 
		spin_lock(&cgrp->event_list_lock);
		if (!list_empty(&event->list)) {
			list_del_init(&event->list);
			 
			schedule_work(&event->remove);
		}
		spin_unlock(&cgrp->event_list_lock);
	}

	return 0;
}

static void cgroup_event_ptable_queue_proc(struct file *file,
		wait_queue_head_t *wqh, poll_table *pt)
{
	struct cgroup_event *event = container_of(pt,
			struct cgroup_event, pt);

	event->wqh = wqh;
	add_wait_queue(wqh, &event->wait);
}

static int cgroup_write_event_control(struct cgroup *cgrp, struct cftype *cft,
				      const char *buffer)
{
	struct cgroup_event *event = NULL;
	struct cgroup *cgrp_cfile;
	unsigned int efd, cfd;
	struct file *efile = NULL;
	struct file *cfile = NULL;
	char *endp;
	int ret;

	efd = simple_strtoul(buffer, &endp, 10);
	if (*endp != ' ')
		return -EINVAL;
	buffer = endp + 1;

	cfd = simple_strtoul(buffer, &endp, 10);
	if ((*endp != ' ') && (*endp != '\0'))
		return -EINVAL;
	buffer = endp + 1;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return -ENOMEM;
	event->cgrp = cgrp;
	INIT_LIST_HEAD(&event->list);
	init_poll_funcptr(&event->pt, cgroup_event_ptable_queue_proc);
	init_waitqueue_func_entry(&event->wait, cgroup_event_wake);
	INIT_WORK(&event->remove, cgroup_event_remove);

	efile = eventfd_fget(efd);
	if (IS_ERR(efile)) {
		ret = PTR_ERR(efile);
		goto fail;
	}

	event->eventfd = eventfd_ctx_fileget(efile);
	if (IS_ERR(event->eventfd)) {
		ret = PTR_ERR(event->eventfd);
		goto fail;
	}

	cfile = fget(cfd);
	if (!cfile) {
		ret = -EBADF;
		goto fail;
	}

	ret = inode_permission(file_inode(cfile), MAY_READ);
	if (ret < 0)
		goto fail;

	event->cft = __file_cft(cfile);
	if (IS_ERR(event->cft)) {
		ret = PTR_ERR(event->cft);
		goto fail;
	}

	cgrp_cfile = __d_cgrp(cfile->f_dentry->d_parent);
	if (cgrp_cfile != cgrp) {
		ret = -EINVAL;
		goto fail;
	}

	if (!event->cft->register_event || !event->cft->unregister_event) {
		ret = -EINVAL;
		goto fail;
	}

	ret = event->cft->register_event(cgrp, event->cft,
			event->eventfd, buffer);
	if (ret)
		goto fail;

	efile->f_op->poll(efile, &event->pt);

	dget(cgrp->dentry);

	spin_lock(&cgrp->event_list_lock);
	list_add(&event->list, &cgrp->event_list);
	spin_unlock(&cgrp->event_list_lock);

	fput(cfile);
	fput(efile);

	return 0;

fail:
	if (cfile)
		fput(cfile);

	if (event && event->eventfd && !IS_ERR(event->eventfd))
		eventfd_ctx_put(event->eventfd);

	if (!IS_ERR_OR_NULL(efile))
		fput(efile);

	kfree(event);

	return ret;
}

static u64 cgroup_clone_children_read(struct cgroup *cgrp,
				    struct cftype *cft)
{
	return test_bit(CGRP_CPUSET_CLONE_CHILDREN, &cgrp->flags);
}

static int cgroup_clone_children_write(struct cgroup *cgrp,
				     struct cftype *cft,
				     u64 val)
{
	if (val)
		set_bit(CGRP_CPUSET_CLONE_CHILDREN, &cgrp->flags);
	else
		clear_bit(CGRP_CPUSET_CLONE_CHILDREN, &cgrp->flags);
	return 0;
}

#define CGROUP_FILE_GENERIC_PREFIX "cgroup."
static struct cftype files[] = {
	{
		.name = "tasks",
		.open = cgroup_tasks_open,
		.write_u64 = cgroup_tasks_write,
		.release = cgroup_pidlist_release,
		.mode = S_IRUGO | S_IWUSR,
	},
	{
		.name = CGROUP_FILE_GENERIC_PREFIX "procs",
		.open = cgroup_procs_open,
		.write_u64 = cgroup_procs_write,
		.release = cgroup_pidlist_release,
		.mode = S_IRUGO | S_IWUSR,
	},
	{
		.name = "notify_on_release",
		.read_u64 = cgroup_read_notify_on_release,
		.write_u64 = cgroup_write_notify_on_release,
	},
	{
		.name = CGROUP_FILE_GENERIC_PREFIX "event_control",
		.write_string = cgroup_write_event_control,
		.mode = S_IWUGO,
	},
	{
		.name = "cgroup.clone_children",
		.flags = CFTYPE_INSANE,
		.read_u64 = cgroup_clone_children_read,
		.write_u64 = cgroup_clone_children_write,
	},
	{
		.name = "cgroup.sane_behavior",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_seq_string = cgroup_sane_behavior_show,
	},
	{
		.name = "release_agent",
		.flags = CFTYPE_ONLY_ON_ROOT,
		.read_seq_string = cgroup_release_agent_show,
		.write_string = cgroup_release_agent_write,
		.max_write_len = PATH_MAX,
	},
	{ }	 
};

static int cgroup_populate_dir(struct cgroup *cgrp, bool base_files,
			       unsigned long subsys_mask)
{
	int err;
	struct cgroup_subsys *ss;

	if (base_files) {
		err = cgroup_addrm_files(cgrp, NULL, files, true);
		if (err < 0)
			return err;
	}

	for_each_subsys(cgrp->root, ss) {
		struct cftype_set *set;
		if (!test_bit(ss->subsys_id, &subsys_mask))
			continue;

		list_for_each_entry(set, &ss->cftsets, node)
			cgroup_addrm_files(cgrp, ss, set->cfts, true);
	}

	for_each_subsys(cgrp->root, ss) {
		struct cgroup_subsys_state *css = cgrp->subsys[ss->subsys_id];
		 
		if (css->id)
			rcu_assign_pointer(css->id->css, css);
	}

	return 0;
}

static void css_dput_fn(struct work_struct *work)
{
	struct cgroup_subsys_state *css =
		container_of(work, struct cgroup_subsys_state, dput_work);

	cgroup_dput(css->cgroup);
}

static void init_cgroup_css(struct cgroup_subsys_state *css,
			       struct cgroup_subsys *ss,
			       struct cgroup *cgrp)
{
	css->cgroup = cgrp;
	atomic_set(&css->refcnt, 1);
	css->flags = 0;
	css->id = NULL;
	if (cgrp == dummytop)
		css->flags |= CSS_ROOT;
	BUG_ON(cgrp->subsys[ss->subsys_id]);
	cgrp->subsys[ss->subsys_id] = css;

	INIT_WORK(&css->dput_work, css_dput_fn);
}

static int online_css(struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	int ret = 0;

	lockdep_assert_held(&cgroup_mutex);

	if (ss->css_online)
		ret = ss->css_online(cgrp);
	if (!ret)
		cgrp->subsys[ss->subsys_id]->flags |= CSS_ONLINE;
	return ret;
}

static void offline_css(struct cgroup_subsys *ss, struct cgroup *cgrp)
	__releases(&cgroup_mutex) __acquires(&cgroup_mutex)
{
	struct cgroup_subsys_state *css = cgrp->subsys[ss->subsys_id];

	lockdep_assert_held(&cgroup_mutex);

	if (!(css->flags & CSS_ONLINE))
		return;

	if (ss->css_offline)
		ss->css_offline(cgrp);

	cgrp->subsys[ss->subsys_id]->flags &= ~CSS_ONLINE;
}

static long cgroup_create(struct cgroup *parent, struct dentry *dentry,
			     umode_t mode)
{
	struct cgroup *cgrp;
	struct cgroup_name *name;
	struct cgroupfs_root *root = parent->root;
	int err = 0;
	struct cgroup_subsys *ss;
	struct super_block *sb = root->sb;

	cgrp = kzalloc(sizeof(*cgrp), GFP_KERNEL);
	if (!cgrp)
		return -ENOMEM;

	name = cgroup_alloc_name(dentry);
	if (!name)
		goto err_free_cgrp;
	rcu_assign_pointer(cgrp->name, name);

	cgrp->id = ida_simple_get(&root->cgroup_ida, 1, 0, GFP_KERNEL);
	if (cgrp->id < 0)
		goto err_free_name;

	if (!cgroup_lock_live_group(parent)) {
		err = -ENODEV;
		goto err_free_id;
	}

	atomic_inc(&sb->s_active);

	init_cgroup_housekeeping(cgrp);

	dentry->d_fsdata = cgrp;
	cgrp->dentry = dentry;

	cgrp->parent = parent;
	cgrp->root = parent->root;

	if (notify_on_release(parent))
		set_bit(CGRP_NOTIFY_ON_RELEASE, &cgrp->flags);

	if (test_bit(CGRP_CPUSET_CLONE_CHILDREN, &parent->flags))
		set_bit(CGRP_CPUSET_CLONE_CHILDREN, &cgrp->flags);

	for_each_subsys(root, ss) {
		struct cgroup_subsys_state *css;

		css = ss->css_alloc(cgrp);
		if (IS_ERR(css)) {
			err = PTR_ERR(css);
			goto err_free_all;
		}
		init_cgroup_css(css, ss, cgrp);
		if (ss->use_id) {
			err = alloc_css_id(ss, parent, cgrp);
			if (err)
				goto err_free_all;
		}
	}

	err = cgroup_create_file(dentry, S_IFDIR | mode, sb);
	if (err < 0)
		goto err_free_all;
	lockdep_assert_held(&dentry->d_inode->i_mutex);

	list_add_tail(&cgrp->allcg_node, &root->allcg_list);
	list_add_tail_rcu(&cgrp->sibling, &cgrp->parent->children);
	root->number_of_cgroups++;

	for_each_subsys(root, ss)
		dget(dentry);

	dget(parent->dentry);

	for_each_subsys(root, ss) {
		err = online_css(ss, cgrp);
		if (err)
			goto err_destroy;

		if (ss->broken_hierarchy && !ss->warned_broken_hierarchy &&
		    parent->parent) {
			pr_warning("cgroup: %s (%d) created nested cgroup for controller \"%s\" which has incomplete hierarchy support. Nested cgroups may change behavior in the future.\n",
				   current->comm, current->pid, ss->name);
			if (!strcmp(ss->name, "memory"))
				pr_warning("cgroup: \"memory\" requires setting use_hierarchy to 1 on the root.\n");
			ss->warned_broken_hierarchy = true;
		}
	}

	err = cgroup_populate_dir(cgrp, true, root->subsys_mask);
	if (err)
		goto err_destroy;

	mutex_unlock(&cgroup_mutex);
	mutex_unlock(&cgrp->dentry->d_inode->i_mutex);

	return 0;

err_free_all:
	for_each_subsys(root, ss) {
		if (cgrp->subsys[ss->subsys_id])
			ss->css_free(cgrp);
	}
	mutex_unlock(&cgroup_mutex);
	 
	deactivate_super(sb);
err_free_id:
	ida_simple_remove(&root->cgroup_ida, cgrp->id);
err_free_name:
	kfree(rcu_dereference_raw(cgrp->name));
err_free_cgrp:
	kfree(cgrp);
	return err;

err_destroy:
	cgroup_destroy_locked(cgrp);
	mutex_unlock(&cgroup_mutex);
	mutex_unlock(&dentry->d_inode->i_mutex);
	return err;
}

static int cgroup_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct cgroup *c_parent = dentry->d_parent->d_fsdata;

	return cgroup_create(c_parent, dentry, mode | S_IFDIR);
}

static int cgroup_destroy_locked(struct cgroup *cgrp)
	__releases(&cgroup_mutex) __acquires(&cgroup_mutex)
{
	struct dentry *d = cgrp->dentry;
	struct cgroup *parent = cgrp->parent;
	struct cgroup_event *event, *tmp;
	struct cgroup_subsys *ss;

	lockdep_assert_held(&d->d_inode->i_mutex);
	lockdep_assert_held(&cgroup_mutex);

	if (atomic_read(&cgrp->count) || !list_empty(&cgrp->children))
		return -EBUSY;

	for_each_subsys(cgrp->root, ss) {
		struct cgroup_subsys_state *css = cgrp->subsys[ss->subsys_id];

		WARN_ON(atomic_read(&css->refcnt) < 0);
		atomic_add(CSS_DEACT_BIAS, &css->refcnt);
	}
	set_bit(CGRP_REMOVED, &cgrp->flags);

	for_each_subsys(cgrp->root, ss)
		offline_css(ss, cgrp);

	for_each_subsys(cgrp->root, ss)
		css_put(cgrp->subsys[ss->subsys_id]);

	raw_spin_lock(&release_list_lock);
	if (!list_empty(&cgrp->release_list))
		list_del_init(&cgrp->release_list);
	raw_spin_unlock(&release_list_lock);

	list_del_rcu(&cgrp->sibling);
	list_del_init(&cgrp->allcg_node);

	dget(d);
	cgroup_d_remove_dir(d);
	dput(d);

	set_bit(CGRP_RELEASABLE, &parent->flags);
	check_for_release(parent);

	spin_lock(&cgrp->event_list_lock);
	list_for_each_entry_safe(event, tmp, &cgrp->event_list, list) {
		list_del_init(&event->list);
		schedule_work(&event->remove);
	}
	spin_unlock(&cgrp->event_list_lock);

	return 0;
}

static int cgroup_rmdir(struct inode *unused_dir, struct dentry *dentry)
{
	int ret;

	mutex_lock(&cgroup_mutex);
	ret = cgroup_destroy_locked(dentry->d_fsdata);
	mutex_unlock(&cgroup_mutex);

	return ret;
}

static void __init_or_module cgroup_init_cftsets(struct cgroup_subsys *ss)
{
	INIT_LIST_HEAD(&ss->cftsets);

	if (ss->base_cftypes) {
		ss->base_cftset.cfts = ss->base_cftypes;
		list_add_tail(&ss->base_cftset.node, &ss->cftsets);
	}
}

static void __init cgroup_init_subsys(struct cgroup_subsys *ss)
{
	struct cgroup_subsys_state *css;

	printk(KERN_INFO "Initializing cgroup subsys %s\n", ss->name);

	mutex_lock(&cgroup_mutex);

	cgroup_init_cftsets(ss);

	list_add(&ss->sibling, &rootnode.subsys_list);
	ss->root = &rootnode;
	css = ss->css_alloc(dummytop);
	 
	BUG_ON(IS_ERR(css));
	init_cgroup_css(css, ss, dummytop);

	init_css_set.subsys[ss->subsys_id] = css;

	need_forkexit_callback |= ss->fork || ss->exit;

	BUG_ON(!list_empty(&init_task.tasks));

	BUG_ON(online_css(ss, dummytop));

	mutex_unlock(&cgroup_mutex);

	BUG_ON(ss->module);
}

int __init_or_module cgroup_load_subsys(struct cgroup_subsys *ss)
{
	struct cgroup_subsys_state *css;
	int i, ret;
	struct hlist_node *tmp;
	struct css_set *cg;
	unsigned long key;

	if (ss->name == NULL || strlen(ss->name) > MAX_CGROUP_TYPE_NAMELEN ||
	    ss->css_alloc == NULL || ss->css_free == NULL)
		return -EINVAL;

	if (ss->fork || ss->exit)
		return -EINVAL;

	if (ss->module == NULL) {
		 
		BUG_ON(subsys[ss->subsys_id] != ss);
		return 0;
	}

	cgroup_init_cftsets(ss);

	mutex_lock(&cgroup_mutex);
	subsys[ss->subsys_id] = ss;

	css = ss->css_alloc(dummytop);
	if (IS_ERR(css)) {
		 
		subsys[ss->subsys_id] = NULL;
		mutex_unlock(&cgroup_mutex);
		return PTR_ERR(css);
	}

	list_add(&ss->sibling, &rootnode.subsys_list);
	ss->root = &rootnode;

	init_cgroup_css(css, ss, dummytop);
	 
	if (ss->use_id) {
		ret = cgroup_init_idr(ss, css);
		if (ret)
			goto err_unload;
	}

	write_lock(&css_set_lock);
	hash_for_each_safe(css_set_table, i, tmp, cg, hlist) {
		 
		if (cg->subsys[ss->subsys_id])
			continue;
		 
		hash_del(&cg->hlist);
		 
		cg->subsys[ss->subsys_id] = css;
		 
		key = css_set_hash(cg->subsys);
		hash_add(css_set_table, &cg->hlist, key);
	}
	write_unlock(&css_set_lock);

	ret = online_css(ss, dummytop);
	if (ret)
		goto err_unload;

	mutex_unlock(&cgroup_mutex);
	return 0;

err_unload:
	mutex_unlock(&cgroup_mutex);
	 
	cgroup_unload_subsys(ss);
	return ret;
}
EXPORT_SYMBOL_GPL(cgroup_load_subsys);

void cgroup_unload_subsys(struct cgroup_subsys *ss)
{
	struct cg_cgroup_link *link;

	BUG_ON(ss->module == NULL);

	BUG_ON(ss->root != &rootnode);

	mutex_lock(&cgroup_mutex);

	offline_css(ss, dummytop);

	if (ss->use_id)
		idr_destroy(&ss->idr);

	subsys[ss->subsys_id] = NULL;

	list_del_init(&ss->sibling);

	write_lock(&css_set_lock);
	list_for_each_entry(link, &dummytop->css_sets, cgrp_link_list) {
		struct css_set *cg = link->cg;
		unsigned long key;

		hash_del(&cg->hlist);
		cg->subsys[ss->subsys_id] = NULL;
		key = css_set_hash(cg->subsys);
		hash_add(css_set_table, &cg->hlist, key);
	}
	write_unlock(&css_set_lock);

	ss->css_free(dummytop);
	dummytop->subsys[ss->subsys_id] = NULL;

	mutex_unlock(&cgroup_mutex);
}
EXPORT_SYMBOL_GPL(cgroup_unload_subsys);

int __init cgroup_init_early(void)
{
	int i;
	atomic_set(&init_css_set.refcount, 1);
	INIT_LIST_HEAD(&init_css_set.cg_links);
	INIT_LIST_HEAD(&init_css_set.tasks);
	INIT_HLIST_NODE(&init_css_set.hlist);
	css_set_count = 1;
	init_cgroup_root(&rootnode);
	root_count = 1;
	init_task.cgroups = &init_css_set;

	init_css_set_link.cg = &init_css_set;
	init_css_set_link.cgrp = dummytop;
	list_add(&init_css_set_link.cgrp_link_list,
		 &rootnode.top_cgroup.css_sets);
	list_add(&init_css_set_link.cg_link_list,
		 &init_css_set.cg_links);

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		struct cgroup_subsys *ss = subsys[i];

		if (!ss || ss->module)
			continue;

		BUG_ON(!ss->name);
		BUG_ON(strlen(ss->name) > MAX_CGROUP_TYPE_NAMELEN);
		BUG_ON(!ss->css_alloc);
		BUG_ON(!ss->css_free);
		if (ss->subsys_id != i) {
			printk(KERN_ERR "cgroup: Subsys %s id == %d\n",
			       ss->name, ss->subsys_id);
			BUG();
		}

		if (ss->early_init)
			cgroup_init_subsys(ss);
	}
	return 0;
}

#ifdef MY_ABC_HERE

static unsigned int SynoCgroupMemIsDisabled = 1;

#endif  

int __init cgroup_init(void)
{
	int err;
	int i;
	unsigned long key;

	err = bdi_init(&cgroup_backing_dev_info);
	if (err)
		return err;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		struct cgroup_subsys *ss = subsys[i];

		if (!ss || ss->module)
			continue;

		if (!ss->early_init)
			cgroup_init_subsys(ss);
		if (ss->use_id)
			cgroup_init_idr(ss, init_css_set.subsys[ss->subsys_id]);

#ifdef MY_ABC_HERE

		if (SynoCgroupMemIsDisabled && !strcmp("memory", ss->name)) {
			ss->disabled = 1;
			printk(KERN_INFO "Disabling %s control group"
				" subsystem\n", ss->name);
		}

#endif
	}

	key = css_set_hash(init_css_set.subsys);
	hash_add(css_set_table, &init_css_set.hlist, key);
	BUG_ON(!init_root_id(&rootnode));

	cgroup_kobj = kobject_create_and_add("cgroup", fs_kobj);
	if (!cgroup_kobj) {
		err = -ENOMEM;
		goto out;
	}

	err = register_filesystem(&cgroup_fs_type);
	if (err < 0) {
		kobject_put(cgroup_kobj);
		goto out;
	}

	proc_create("cgroups", 0, NULL, &proc_cgroupstats_operations);

out:
	if (err)
		bdi_destroy(&cgroup_backing_dev_info);

	return err;
}

static int __init cgroup_wq_init(void)
{
	 
	cgroup_destroy_wq = alloc_workqueue("cgroup_destroy", 0, 1);
	BUG_ON(!cgroup_destroy_wq);
	return 0;
}
core_initcall(cgroup_wq_init);

int proc_cgroup_show(struct seq_file *m, void *v)
{
	struct pid *pid;
	struct task_struct *tsk;
	char *buf;
	int retval;
	struct cgroupfs_root *root;

	retval = -ENOMEM;
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		goto out;

	retval = -ESRCH;
	pid = m->private;
	tsk = get_pid_task(pid, PIDTYPE_PID);
	if (!tsk)
		goto out_free;

	retval = 0;

	mutex_lock(&cgroup_mutex);

	for_each_active_root(root) {
		struct cgroup_subsys *ss;
		struct cgroup *cgrp;
		int count = 0;

		seq_printf(m, "%d:", root->hierarchy_id);
		for_each_subsys(root, ss)
			seq_printf(m, "%s%s", count++ ? "," : "", ss->name);
		if (strlen(root->name))
			seq_printf(m, "%sname=%s", count ? "," : "",
				   root->name);
		seq_putc(m, ':');
		cgrp = task_cgroup_from_root(tsk, root);
		retval = cgroup_path(cgrp, buf, PAGE_SIZE);
		if (retval < 0)
			goto out_unlock;
		seq_puts(m, buf);
		seq_putc(m, '\n');
	}

out_unlock:
	mutex_unlock(&cgroup_mutex);
	put_task_struct(tsk);
out_free:
	kfree(buf);
out:
	return retval;
}

static int proc_cgroupstats_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "#subsys_name\thierarchy\tnum_cgroups\tenabled\n");
	 
	mutex_lock(&cgroup_mutex);
	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		struct cgroup_subsys *ss = subsys[i];
		if (ss == NULL)
			continue;
		seq_printf(m, "%s\t%d\t%d\t%d\n",
			   ss->name, ss->root->hierarchy_id,
			   ss->root->number_of_cgroups, !ss->disabled);
	}
	mutex_unlock(&cgroup_mutex);
	return 0;
}

static int cgroupstats_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_cgroupstats_show, NULL);
}

static const struct file_operations proc_cgroupstats_operations = {
	.open = cgroupstats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void cgroup_fork(struct task_struct *child)
{
	task_lock(current);
	child->cgroups = current->cgroups;
	get_css_set(child->cgroups);
	task_unlock(current);
	INIT_LIST_HEAD(&child->cg_list);
}

void cgroup_post_fork(struct task_struct *child)
{
	int i;

	if (use_task_css_set_links) {
		write_lock(&css_set_lock);
		task_lock(child);
		if (list_empty(&child->cg_list))
			list_add(&child->cg_list, &child->cgroups->tasks);
		task_unlock(child);
		write_unlock(&css_set_lock);
	}

	if (need_forkexit_callback) {
		 
		for (i = 0; i < CGROUP_BUILTIN_SUBSYS_COUNT; i++) {
			struct cgroup_subsys *ss = subsys[i];

			if (ss->fork)
				ss->fork(child);
		}
	}
}

void cgroup_exit(struct task_struct *tsk, int run_callbacks)
{
	struct css_set *cg;
	int i;

	if (!list_empty(&tsk->cg_list)) {
		write_lock(&css_set_lock);
		if (!list_empty(&tsk->cg_list))
			list_del_init(&tsk->cg_list);
		write_unlock(&css_set_lock);
	}

	task_lock(tsk);
	cg = tsk->cgroups;
	tsk->cgroups = &init_css_set;

	if (run_callbacks && need_forkexit_callback) {
		 
		for (i = 0; i < CGROUP_BUILTIN_SUBSYS_COUNT; i++) {
			struct cgroup_subsys *ss = subsys[i];

			if (ss->exit) {
				struct cgroup *old_cgrp =
					rcu_dereference_raw(cg->subsys[i])->cgroup;
				struct cgroup *cgrp = task_cgroup(tsk, i);
				ss->exit(cgrp, old_cgrp, tsk);
			}
		}
	}
	task_unlock(tsk);

	put_css_set_taskexit(cg);
}

static void check_for_release(struct cgroup *cgrp)
{
	 
	if (cgroup_is_releasable(cgrp) &&
	    !atomic_read(&cgrp->count) && list_empty(&cgrp->children)) {
		 
		int need_schedule_work = 0;

		raw_spin_lock(&release_list_lock);
		if (!cgroup_is_removed(cgrp) &&
		    list_empty(&cgrp->release_list)) {
			list_add(&cgrp->release_list, &release_list);
			need_schedule_work = 1;
		}
		raw_spin_unlock(&release_list_lock);
		if (need_schedule_work)
			schedule_work(&release_agent_work);
	}
}

bool __css_tryget(struct cgroup_subsys_state *css)
{
	while (true) {
		int t, v;

		v = css_refcnt(css);
		t = atomic_cmpxchg(&css->refcnt, v, v + 1);
		if (likely(t == v))
			return true;
		else if (t < 0)
			return false;
		cpu_relax();
	}
}
EXPORT_SYMBOL_GPL(__css_tryget);

void __css_put(struct cgroup_subsys_state *css)
{
	int v;

	v = css_unbias_refcnt(atomic_dec_return(&css->refcnt));
	if (v == 0)
		queue_work(cgroup_destroy_wq, &css->dput_work);
}
EXPORT_SYMBOL_GPL(__css_put);

static void cgroup_release_agent(struct work_struct *work)
{
	BUG_ON(work != &release_agent_work);
	mutex_lock(&cgroup_mutex);
	raw_spin_lock(&release_list_lock);
	while (!list_empty(&release_list)) {
		char *argv[3], *envp[3];
		int i;
		char *pathbuf = NULL, *agentbuf = NULL;
		struct cgroup *cgrp = list_entry(release_list.next,
						    struct cgroup,
						    release_list);
		list_del_init(&cgrp->release_list);
		raw_spin_unlock(&release_list_lock);
		pathbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!pathbuf)
			goto continue_free;
		if (cgroup_path(cgrp, pathbuf, PAGE_SIZE) < 0)
			goto continue_free;
		agentbuf = kstrdup(cgrp->root->release_agent_path, GFP_KERNEL);
		if (!agentbuf)
			goto continue_free;

		i = 0;
		argv[i++] = agentbuf;
		argv[i++] = pathbuf;
		argv[i] = NULL;

		i = 0;
		 
		envp[i++] = "HOME=/";
		envp[i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
		envp[i] = NULL;

		mutex_unlock(&cgroup_mutex);
		call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);
		mutex_lock(&cgroup_mutex);
 continue_free:
		kfree(pathbuf);
		kfree(agentbuf);
		raw_spin_lock(&release_list_lock);
	}
	raw_spin_unlock(&release_list_lock);
	mutex_unlock(&cgroup_mutex);
}

static int __init cgroup_disable(char *str)
{
	int i;
	char *token;

	while ((token = strsep(&str, ",")) != NULL) {
		if (!*token)
			continue;
		for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
			struct cgroup_subsys *ss = subsys[i];

			if (!ss || ss->module)
				continue;

			if (!strcmp(token, ss->name)) {
				ss->disabled = 1;
				printk(KERN_INFO "Disabling %s control group"
					" subsystem\n", ss->name);
				break;
			}
		}
	}
	return 1;
}
__setup("cgroup_disable=", cgroup_disable);

#ifdef MY_ABC_HERE

static int __init SynoCGroupMemEnable(char *str) {

	int i;

	SynoCgroupMemIsDisabled = 0;

	for (i = 0; i < CGROUP_SUBSYS_COUNT; i++) {
		struct cgroup_subsys *ss = subsys[i];

		if (!ss || ss->module)
			continue;

		if (!strcmp("memory", ss->name)) {
			ss->disabled = 0;
			printk(KERN_INFO "Enabling %s control group"
				" subsystem\n", ss->name);
			break;
		}
	}
#if defined(CONFIG_SYNO_HI3536)
	return 1;
#endif  
}

__setup("cgroup_memory", SynoCGroupMemEnable);

#endif  

unsigned short css_id(struct cgroup_subsys_state *css)
{
	struct css_id *cssid;

	cssid = rcu_dereference_check(css->id, css_refcnt(css));

	if (cssid)
		return cssid->id;
	return 0;
}
EXPORT_SYMBOL_GPL(css_id);

unsigned short css_depth(struct cgroup_subsys_state *css)
{
	struct css_id *cssid;

	cssid = rcu_dereference_check(css->id, css_refcnt(css));

	if (cssid)
		return cssid->depth;
	return 0;
}
EXPORT_SYMBOL_GPL(css_depth);

bool css_is_ancestor(struct cgroup_subsys_state *child,
		    const struct cgroup_subsys_state *root)
{
	struct css_id *child_id;
	struct css_id *root_id;

	child_id  = rcu_dereference(child->id);
	if (!child_id)
		return false;
	root_id = rcu_dereference(root->id);
	if (!root_id)
		return false;
	if (child_id->depth < root_id->depth)
		return false;
	if (child_id->stack[root_id->depth] != root_id->id)
		return false;
	return true;
}

void free_css_id(struct cgroup_subsys *ss, struct cgroup_subsys_state *css)
{
	struct css_id *id = css->id;
	 
	if (!id)
		return;

	BUG_ON(!ss->use_id);

	rcu_assign_pointer(id->css, NULL);
	rcu_assign_pointer(css->id, NULL);
	spin_lock(&ss->id_lock);
	idr_remove(&ss->idr, id->id);
	spin_unlock(&ss->id_lock);
	kfree_rcu(id, rcu_head);
}
EXPORT_SYMBOL_GPL(free_css_id);

static struct css_id *get_new_cssid(struct cgroup_subsys *ss, int depth)
{
	struct css_id *newid;
	int ret, size;

	BUG_ON(!ss->use_id);

	size = sizeof(*newid) + sizeof(unsigned short) * (depth + 1);
	newid = kzalloc(size, GFP_KERNEL);
	if (!newid)
		return ERR_PTR(-ENOMEM);

	idr_preload(GFP_KERNEL);
	spin_lock(&ss->id_lock);
	 
	ret = idr_alloc(&ss->idr, newid, 1, CSS_ID_MAX + 1, GFP_NOWAIT);
	spin_unlock(&ss->id_lock);
	idr_preload_end();

	if (ret < 0)
		goto err_out;

	newid->id = ret;
	newid->depth = depth;
	return newid;
err_out:
	kfree(newid);
	return ERR_PTR(ret);

}

static int __init_or_module cgroup_init_idr(struct cgroup_subsys *ss,
					    struct cgroup_subsys_state *rootcss)
{
	struct css_id *newid;

	spin_lock_init(&ss->id_lock);
	idr_init(&ss->idr);

	newid = get_new_cssid(ss, 0);
	if (IS_ERR(newid))
		return PTR_ERR(newid);

	newid->stack[0] = newid->id;
	newid->css = rootcss;
	rootcss->id = newid;
	return 0;
}

static int alloc_css_id(struct cgroup_subsys *ss, struct cgroup *parent,
			struct cgroup *child)
{
	int subsys_id, i, depth = 0;
	struct cgroup_subsys_state *parent_css, *child_css;
	struct css_id *child_id, *parent_id;

	subsys_id = ss->subsys_id;
	parent_css = parent->subsys[subsys_id];
	child_css = child->subsys[subsys_id];
	parent_id = parent_css->id;
	depth = parent_id->depth + 1;

	child_id = get_new_cssid(ss, depth);
	if (IS_ERR(child_id))
		return PTR_ERR(child_id);

	for (i = 0; i < depth; i++)
		child_id->stack[i] = parent_id->stack[i];
	child_id->stack[depth] = child_id->id;
	 
	rcu_assign_pointer(child_css->id, child_id);

	return 0;
}

struct cgroup_subsys_state *css_lookup(struct cgroup_subsys *ss, int id)
{
	struct css_id *cssid = NULL;

	BUG_ON(!ss->use_id);
	cssid = idr_find(&ss->idr, id);

	if (unlikely(!cssid))
		return NULL;

	return rcu_dereference(cssid->css);
}
EXPORT_SYMBOL_GPL(css_lookup);

struct cgroup_subsys_state *cgroup_css_from_dir(struct file *f, int id)
{
	struct cgroup *cgrp;
	struct inode *inode;
	struct cgroup_subsys_state *css;

	inode = file_inode(f);
	 
	if (inode->i_op != &cgroup_dir_inode_operations)
		return ERR_PTR(-EBADF);

	if (id < 0 || id >= CGROUP_SUBSYS_COUNT)
		return ERR_PTR(-EINVAL);

	cgrp = __d_cgrp(f->f_dentry);
	css = cgrp->subsys[id];
	return css ? css : ERR_PTR(-ENOENT);
}

#ifdef CONFIG_CGROUP_DEBUG
static struct cgroup_subsys_state *debug_css_alloc(struct cgroup *cont)
{
	struct cgroup_subsys_state *css = kzalloc(sizeof(*css), GFP_KERNEL);

	if (!css)
		return ERR_PTR(-ENOMEM);

	return css;
}

static void debug_css_free(struct cgroup *cont)
{
	kfree(cont->subsys[debug_subsys_id]);
}

static u64 cgroup_refcount_read(struct cgroup *cont, struct cftype *cft)
{
	return atomic_read(&cont->count);
}

static u64 debug_taskcount_read(struct cgroup *cont, struct cftype *cft)
{
	return cgroup_task_count(cont);
}

static u64 current_css_set_read(struct cgroup *cont, struct cftype *cft)
{
	return (u64)(unsigned long)current->cgroups;
}

static u64 current_css_set_refcount_read(struct cgroup *cont,
					   struct cftype *cft)
{
	u64 count;

	rcu_read_lock();
	count = atomic_read(&current->cgroups->refcount);
	rcu_read_unlock();
	return count;
}

static int current_css_set_cg_links_read(struct cgroup *cont,
					 struct cftype *cft,
					 struct seq_file *seq)
{
	struct cg_cgroup_link *link;
	struct css_set *cg;

	read_lock(&css_set_lock);
	rcu_read_lock();
	cg = rcu_dereference(current->cgroups);
	list_for_each_entry(link, &cg->cg_links, cg_link_list) {
		struct cgroup *c = link->cgrp;
		const char *name;

		if (c->dentry)
			name = c->dentry->d_name.name;
		else
			name = "?";
		seq_printf(seq, "Root %d group %s\n",
			   c->root->hierarchy_id, name);
	}
	rcu_read_unlock();
	read_unlock(&css_set_lock);
	return 0;
}

#define MAX_TASKS_SHOWN_PER_CSS 25
static int cgroup_css_links_read(struct cgroup *cont,
				 struct cftype *cft,
				 struct seq_file *seq)
{
	struct cg_cgroup_link *link;

	read_lock(&css_set_lock);
	list_for_each_entry(link, &cont->css_sets, cgrp_link_list) {
		struct css_set *cg = link->cg;
		struct task_struct *task;
		int count = 0;
		seq_printf(seq, "css_set %p\n", cg);
		list_for_each_entry(task, &cg->tasks, cg_list) {
			if (count++ > MAX_TASKS_SHOWN_PER_CSS) {
				seq_puts(seq, "  ...\n");
				break;
			} else {
				seq_printf(seq, "  task %d\n",
					   task_pid_vnr(task));
			}
		}
	}
	read_unlock(&css_set_lock);
	return 0;
}

static u64 releasable_read(struct cgroup *cgrp, struct cftype *cft)
{
	return test_bit(CGRP_RELEASABLE, &cgrp->flags);
}

static struct cftype debug_files[] =  {
	{
		.name = "cgroup_refcount",
		.read_u64 = cgroup_refcount_read,
	},
	{
		.name = "taskcount",
		.read_u64 = debug_taskcount_read,
	},

	{
		.name = "current_css_set",
		.read_u64 = current_css_set_read,
	},

	{
		.name = "current_css_set_refcount",
		.read_u64 = current_css_set_refcount_read,
	},

	{
		.name = "current_css_set_cg_links",
		.read_seq_string = current_css_set_cg_links_read,
	},

	{
		.name = "cgroup_css_links",
		.read_seq_string = cgroup_css_links_read,
	},

	{
		.name = "releasable",
		.read_u64 = releasable_read,
	},

	{ }	 
};

struct cgroup_subsys debug_subsys = {
	.name = "debug",
	.css_alloc = debug_css_alloc,
	.css_free = debug_css_free,
	.subsys_id = debug_subsys_id,
	.base_cftypes = debug_files,
};
#endif  
