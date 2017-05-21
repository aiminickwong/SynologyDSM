#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef _ASMARM_SHMPARAM_H
#define _ASMARM_SHMPARAM_H

#if (defined(MY_DEF_HERE) && defined(CONFIG_ARM_PAGE_SIZE_LARGE)) || \
	(defined(MY_ABC_HERE) && defined(CONFIG_MV_LARGE_PAGE_SUPPORT))
#define	SHMLBA	(16 << 10)		  
#else
#define	SHMLBA	(4 * PAGE_SIZE)		  
#endif

#define __ARCH_FORCE_SHMLBA

#endif  
