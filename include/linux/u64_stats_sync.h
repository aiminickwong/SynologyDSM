#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _LINUX_U64_STATS_SYNC_H
#define _LINUX_U64_STATS_SYNC_H

#include <linux/seqlock.h>

#if defined(MY_DEF_HERE)
struct u64_stats_sync {
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	seqcount_t	seq;
#endif
};

static inline void u64_stats_update_begin(struct u64_stats_sync *syncp)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_begin(&syncp->seq);
#endif
}

static inline void u64_stats_update_end(struct u64_stats_sync *syncp)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_end(&syncp->seq);
#endif
}

static inline unsigned int u64_stats_fetch_begin(const struct u64_stats_sync *syncp)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32
	preempt_disable();
#endif
	return 0;
#endif
}

static inline bool u64_stats_fetch_retry(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32
	preempt_enable();
#endif
	return false;
#endif
}

static inline unsigned int u64_stats_fetch_begin_bh(const struct u64_stats_sync *syncp)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32
	local_bh_disable();
#endif
	return 0;
#endif
}

static inline bool u64_stats_fetch_retry_bh(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if !defined(CONFIG_ARCH_LONG_LONG_ATOMIC) && BITS_PER_LONG==32
	local_bh_enable();
#endif
	return false;
#endif
}

#else  

struct u64_stats_sync {
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	seqcount_t	seq;
#endif
};

static inline void u64_stats_update_begin(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_begin(&syncp->seq);
#endif
}

static inline void u64_stats_update_end(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_end(&syncp->seq);
#endif
}

static inline unsigned int u64_stats_fetch_begin(const struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if BITS_PER_LONG==32
	preempt_disable();
#endif
	return 0;
#endif
}

static inline bool u64_stats_fetch_retry(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if BITS_PER_LONG==32
	preempt_enable();
#endif
	return false;
#endif
}

static inline unsigned int u64_stats_fetch_begin_bh(const struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if BITS_PER_LONG==32
	local_bh_disable();
#endif
	return 0;
#endif
}

static inline bool u64_stats_fetch_retry_bh(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if BITS_PER_LONG==32
	local_bh_enable();
#endif
	return false;
#endif
}
#endif  

#endif  
