#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/crc32.h>
#include <linux/jffs2.h>
#include "nodelist.h"

static int jffs2_write_end(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *pg, void *fsdata);
static int jffs2_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata);
static int jffs2_readpage (struct file *filp, struct page *pg);

int jffs2_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	int ret;

	ret = filemap_write_and_wait_range(inode->i_mapping, start, end);
	if (ret)
		return ret;

	mutex_lock(&inode->i_mutex);
	 
	jffs2_flush_wbuf_gc(c, inode->i_ino);
	mutex_unlock(&inode->i_mutex);

	return 0;
}

const struct file_operations jffs2_file_operations =
{
	.llseek =	generic_file_llseek,
	.open =		generic_file_open,
 	.read =		do_sync_read,
 	.aio_read =	generic_file_aio_read,
 	.write =	do_sync_write,
 	.aio_write =	generic_file_aio_write,
	.unlocked_ioctl=jffs2_ioctl,
	.mmap =		generic_file_readonly_mmap,
	.fsync =	jffs2_fsync,
	.splice_read =	generic_file_splice_read,
};

const struct inode_operations jffs2_file_inode_operations =
{
	.get_acl =	jffs2_get_acl,
	.setattr =	jffs2_setattr,
	.setxattr =	jffs2_setxattr,
	.getxattr =	jffs2_getxattr,
	.listxattr =	jffs2_listxattr,
	.removexattr =	jffs2_removexattr
};

const struct address_space_operations jffs2_file_address_operations =
{
	.readpage =	jffs2_readpage,
	.write_begin =	jffs2_write_begin,
	.write_end =	jffs2_write_end,
};

static int jffs2_do_readpage_nolock (struct inode *inode, struct page *pg)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	unsigned char *pg_buf;
	int ret;

#ifdef MY_DEF_HERE
	jffs2_dbg(2, "%s(): ino #%lu, page at offset 0x%llx\n",
		  __func__, inode->i_ino, (unsigned long long)pg->index << PAGE_CACHE_SHIFT);
#else  
	jffs2_dbg(2, "%s(): ino #%lu, page at offset 0x%lx\n",
		  __func__, inode->i_ino, pg->index << PAGE_CACHE_SHIFT);
#endif  

	BUG_ON(!PageLocked(pg));

	pg_buf = kmap(pg);
	 
	ret = jffs2_read_inode_range(c, f, pg_buf, pg->index << PAGE_CACHE_SHIFT, PAGE_CACHE_SIZE);

	if (ret) {
		ClearPageUptodate(pg);
		SetPageError(pg);
	} else {
		SetPageUptodate(pg);
		ClearPageError(pg);
	}

	flush_dcache_page(pg);
	kunmap(pg);

	jffs2_dbg(2, "readpage finished\n");
	return ret;
}

int jffs2_do_readpage_unlock(struct inode *inode, struct page *pg)
{
	int ret = jffs2_do_readpage_nolock(inode, pg);
	unlock_page(pg);
	return ret;
}

static int jffs2_readpage (struct file *filp, struct page *pg)
{
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(pg->mapping->host);
	int ret;

	mutex_lock(&f->sem);
	ret = jffs2_do_readpage_unlock(pg->mapping->host, pg);
	mutex_unlock(&f->sem);
	return ret;
}

static int jffs2_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	struct page *pg;
	struct inode *inode = mapping->host;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	uint32_t pageofs = index << PAGE_CACHE_SHIFT;
	int ret = 0;

	pg = grab_cache_page_write_begin(mapping, index, flags);
	if (!pg)
		return -ENOMEM;
	*pagep = pg;

	jffs2_dbg(1, "%s()\n", __func__);

	if (pageofs > inode->i_size) {
		 
		struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
		struct jffs2_raw_inode ri;
		struct jffs2_full_dnode *fn;
		uint32_t alloc_len;

		jffs2_dbg(1, "Writing new hole frag 0x%x-0x%x between current EOF and new page\n",
			  (unsigned int)inode->i_size, pageofs);

		ret = jffs2_reserve_space(c, sizeof(ri), &alloc_len,
					  ALLOC_NORMAL, JFFS2_SUMMARY_INODE_SIZE);
		if (ret)
			goto out_page;

		mutex_lock(&f->sem);
		memset(&ri, 0, sizeof(ri));

		ri.magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
		ri.nodetype = cpu_to_je16(JFFS2_NODETYPE_INODE);
		ri.totlen = cpu_to_je32(sizeof(ri));
		ri.hdr_crc = cpu_to_je32(crc32(0, &ri, sizeof(struct jffs2_unknown_node)-4));

		ri.ino = cpu_to_je32(f->inocache->ino);
		ri.version = cpu_to_je32(++f->highest_version);
		ri.mode = cpu_to_jemode(inode->i_mode);
		ri.uid = cpu_to_je16(i_uid_read(inode));
		ri.gid = cpu_to_je16(i_gid_read(inode));
		ri.isize = cpu_to_je32(max((uint32_t)inode->i_size, pageofs));
		ri.atime = ri.ctime = ri.mtime = cpu_to_je32(get_seconds());
		ri.offset = cpu_to_je32(inode->i_size);
		ri.dsize = cpu_to_je32(pageofs - inode->i_size);
		ri.csize = cpu_to_je32(0);
		ri.compr = JFFS2_COMPR_ZERO;
		ri.node_crc = cpu_to_je32(crc32(0, &ri, sizeof(ri)-8));
		ri.data_crc = cpu_to_je32(0);

		fn = jffs2_write_dnode(c, f, &ri, NULL, 0, ALLOC_NORMAL);

		if (IS_ERR(fn)) {
			ret = PTR_ERR(fn);
			jffs2_complete_reservation(c);
			mutex_unlock(&f->sem);
			goto out_page;
		}
		ret = jffs2_add_full_dnode_to_inode(c, f, fn);
		if (f->metadata) {
			jffs2_mark_node_obsolete(c, f->metadata->raw);
			jffs2_free_full_dnode(f->metadata);
			f->metadata = NULL;
		}
		if (ret) {
			jffs2_dbg(1, "Eep. add_full_dnode_to_inode() failed in write_begin, returned %d\n",
				  ret);
			jffs2_mark_node_obsolete(c, fn->raw);
			jffs2_free_full_dnode(fn);
			jffs2_complete_reservation(c);
			mutex_unlock(&f->sem);
			goto out_page;
		}
		jffs2_complete_reservation(c);
		inode->i_size = pageofs;
		mutex_unlock(&f->sem);
	}

	if (!PageUptodate(pg)) {
		mutex_lock(&f->sem);
		ret = jffs2_do_readpage_nolock(inode, pg);
		mutex_unlock(&f->sem);
		if (ret)
			goto out_page;
	}
	jffs2_dbg(1, "end write_begin(). pg->flags %lx\n", pg->flags);
	return ret;

out_page:
	unlock_page(pg);
	page_cache_release(pg);
	return ret;
}

static int jffs2_write_end(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *pg, void *fsdata)
{
	 
	struct inode *inode = mapping->host;
	struct jffs2_inode_info *f = JFFS2_INODE_INFO(inode);
	struct jffs2_sb_info *c = JFFS2_SB_INFO(inode->i_sb);
	struct jffs2_raw_inode *ri;
	unsigned start = pos & (PAGE_CACHE_SIZE - 1);
	unsigned end = start + copied;
	unsigned aligned_start = start & ~3;
	int ret = 0;
	uint32_t writtenlen = 0;

#ifdef MY_DEF_HERE
	jffs2_dbg(1, "%s(): ino #%lu, page at 0x%llx, range %d-%d, flags %lx\n",
		  __func__, inode->i_ino, (unsigned long long)pg->index << PAGE_CACHE_SHIFT,
		  start, end, pg->flags);
#else  
	jffs2_dbg(1, "%s(): ino #%lu, page at 0x%lx, range %d-%d, flags %lx\n",
		  __func__, inode->i_ino, pg->index << PAGE_CACHE_SHIFT,
		  start, end, pg->flags);
#endif  

	BUG_ON(!PageUptodate(pg));

	if (end == PAGE_CACHE_SIZE) {
		 
		aligned_start = 0;
	}

	ri = jffs2_alloc_raw_inode();

	if (!ri) {
		jffs2_dbg(1, "%s(): Allocation of raw inode failed\n",
			  __func__);
		unlock_page(pg);
		page_cache_release(pg);
		return -ENOMEM;
	}

	ri->ino = cpu_to_je32(inode->i_ino);
	ri->mode = cpu_to_jemode(inode->i_mode);
	ri->uid = cpu_to_je16(i_uid_read(inode));
	ri->gid = cpu_to_je16(i_gid_read(inode));
	ri->isize = cpu_to_je32((uint32_t)inode->i_size);
	ri->atime = ri->ctime = ri->mtime = cpu_to_je32(get_seconds());

	kmap(pg);

	ret = jffs2_write_inode_range(c, f, ri, page_address(pg) + aligned_start,
				      (pg->index << PAGE_CACHE_SHIFT) + aligned_start,
				      end - aligned_start, &writtenlen);

	kunmap(pg);

	if (ret) {
		 
		SetPageError(pg);
	}

	writtenlen -= min(writtenlen, (start - aligned_start));

	if (writtenlen) {
		if (inode->i_size < pos + writtenlen) {
			inode->i_size = pos + writtenlen;
			inode->i_blocks = (inode->i_size + 511) >> 9;

			inode->i_ctime = inode->i_mtime = ITIME(je32_to_cpu(ri->ctime));
		}
	}

	jffs2_free_raw_inode(ri);

	if (start+writtenlen < end) {
		 
		jffs2_dbg(1, "%s(): Not all bytes written. Marking page !uptodate\n",
			__func__);
		SetPageError(pg);
		ClearPageUptodate(pg);
	}

	jffs2_dbg(1, "%s() returning %d\n",
		  __func__, writtenlen > 0 ? writtenlen : ret);
	unlock_page(pg);
	page_cache_release(pg);
	return writtenlen > 0 ? writtenlen : ret;
}
