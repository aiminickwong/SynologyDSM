/* memset.S: optimised assembly memset
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

        .text
        .p2align	4

###############################################################################
#
# void *memset(void *p, char ch, size_t count)
#
# - NOTE: must not use any stack. exception detection performs function return
#         to caller's fixup routine, aborting the remainder of the set
#         GR4, GR7, GR8, and GR11 must be managed
#
###############################################################################
        .globl		memset,__memset_end
        .type		memset,@function
memset:
	orcc.p		gr10,gr0,gr5,icc3		; GR5 = count
	andi		gr9,#0xff,gr9
	or.p		gr8,gr0,gr4			; GR4 = address
	beqlr		icc3,#0

	# conditionally write a byte to 2b-align the address
	setlos.p	#1,gr6
	andicc		gr4,#1,gr0,icc0
	ckne		icc0,cc7
	cstb.p		gr9,@(gr4,gr0)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cadd.p		gr4,gr6,gr4		,cc7,#1
	beqlr		icc3,#0

	# conditionally write a word to 4b-align the address
	andicc.p	gr4,#2,gr0,icc0
	subicc		gr5,#2,gr0,icc1
	setlos.p	#2,gr6
	ckne		icc0,cc7
	slli.p		gr9,#8,gr12			; need to double up the pattern
	cknc		icc1,cc5
	or.p		gr9,gr12,gr12
	andcr		cc7,cc5,cc7

	csth.p		gr12,@(gr4,gr0)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cadd.p		gr4,gr6,gr4		,cc7,#1
	beqlr		icc3,#0

	# conditionally write a dword to 8b-align the address
	andicc.p	gr4,#4,gr0,icc0
	subicc		gr5,#4,gr0,icc1
	setlos.p	#4,gr6
	ckne		icc0,cc7
	slli.p		gr12,#16,gr13			; need to quadruple-up the pattern
	cknc		icc1,cc5
	or.p		gr13,gr12,gr12
	andcr		cc7,cc5,cc7

	cst.p		gr12,@(gr4,gr0)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cadd.p		gr4,gr6,gr4		,cc7,#1
	beqlr		icc3,#0

	or.p		gr12,gr12,gr13			; need to octuple-up the pattern

	# the address is now 8b-aligned - loop around writing 64b chunks
	setlos		#8,gr7
	subi.p		gr4,#8,gr4			; store with update index does weird stuff
	setlos		#64,gr6

	subicc		gr5,#64,gr0,icc0
0:	cknc		icc0,cc7
	cstdu		gr12,@(gr4,gr7)		,cc7,#1
	cstdu		gr12,@(gr4,gr7)		,cc7,#1
	cstdu		gr12,@(gr4,gr7)		,cc7,#1
	cstdu		gr12,@(gr4,gr7)		,cc7,#1
	cstdu		gr12,@(gr4,gr7)		,cc7,#1
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	subicc		gr5,#64,gr0,icc0
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	beqlr		icc3,#0
	bnc		icc0,#2,0b

	# now do 32-byte remnant
	subicc.p	gr5,#32,gr0,icc0
	setlos		#32,gr6
	cknc		icc0,cc7
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	setlos		#16,gr6
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	subicc		gr5,#16,gr0,icc0
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	beqlr		icc3,#0

	# now do 16-byte remnant
	cknc		icc0,cc7
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr6,gr5		,cc7,#1	; also set ICC3
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	beqlr		icc3,#0

	# now do 8-byte remnant
	subicc		gr5,#8,gr0,icc1
	cknc		icc1,cc7
	cstdu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr7,gr5		,cc7,#1	; also set ICC3
	setlos.p	#4,gr7
	beqlr		icc3,#0

	# now do 4-byte remnant
	subicc		gr5,#4,gr0,icc0
	addi.p		gr4,#4,gr4
	cknc		icc0,cc7
	cstu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr7,gr5		,cc7,#1	; also set ICC3
	subicc.p	gr5,#2,gr0,icc1
	beqlr		icc3,#0

	# now do 2-byte remnant
	setlos		#2,gr7
	addi.p		gr4,#2,gr4
	cknc		icc1,cc7
	csthu.p		gr12,@(gr4,gr7)		,cc7,#1
	csubcc		gr5,gr7,gr5		,cc7,#1	; also set ICC3
	subicc.p	gr5,#1,gr0,icc0
	beqlr		icc3,#0

	# now do 1-byte remnant
	setlos		#0,gr7
	addi.p		gr4,#2,gr4
	cknc		icc0,cc7
	cstb.p		gr12,@(gr4,gr0)		,cc7,#1
	bralr
__memset_end:

	.size		memset, __memset_end-memset

###############################################################################
#
# clear memory in userspace
# - return the number of bytes that could not be cleared (0 on complete success)
#
# long __memset_user(void *p, size_t count)
#
###############################################################################
        .globl		__memset_user, __memset_user_error_lr, __memset_user_error_handler
        .type		__memset_user,@function
__memset_user:
	movsg		lr,gr11

	# abuse memset to do the dirty work
	or.p		gr9,gr9,gr10
	setlos		#0,gr9
	call		memset
__memset_user_error_lr:
	jmpl.p		@(gr11,gr0)
	setlos		#0,gr8

	# deal any exception generated by memset
	# GR4  - memset's address tracking pointer
	# GR7  - memset's step value (index register for store insns)
	# GR8  - memset's original start address
	# GR10 - memset's original count
__memset_user_error_handler:
	add.p		gr4,gr7,gr4
	add		gr8,gr10,gr8
	jmpl.p		@(gr11,gr0)
	sub		gr8,gr4,gr8		; we return the amount left uncleared

	.size		__memset_user, .-__memset_user
