#
# This is a simple Makefile to test some of the RAID-6 code
# from userspace.
#

CC	 = gcc
OPTFLAGS = -O2			# Adjust as desired
CFLAGS	 = -I.. -I ../../../include -g $(OPTFLAGS)
LD	 = ld
AWK	 = awk -f
AR	 = ar
RANLIB	 = ranlib
OBJS	 = int1.o int2.o int4.o int8.o int16.o int32.o recov.o algos.o tables.o

ARCH := $(shell uname -m 2>/dev/null | sed -e /s/i.86/i386/)
ifeq ($(ARCH),i386)
        CFLAGS += -DCONFIG_X86_32
        IS_X86 = yes
endif
ifeq ($(ARCH),x86_64)
        CFLAGS += -DCONFIG_X86_64
        IS_X86 = yes
endif

ifeq ($(CONFIG_SYNO_BACKPORT_ARM_CRYPTO),y)
ifeq ($(ARCH),arm)
        CFLAGS += -I../../../arch/arm/include -mfpu=neon
        HAS_NEON = yes
endif
ifeq ($(ARCH),arm64)
        CFLAGS += -I../../../arch/arm64/include
        HAS_NEON = yes
endif
endif # CONFIG_SYNO_BACKPORT_ARM_CRYPTO

ifeq ($(CONFIG_SYNO_BACKPORT_ARM_CRYPTO),y)
ifeq ($(IS_X86),yes)
        OBJS   += mmx.o sse1.o sse2.o avx2.o recov_ssse3.o recov_avx2.o
        CFLAGS += $(shell echo "vpbroadcastb %xmm0, %ymm1" |	\
                    gcc -c -x assembler - >&/dev/null &&	\
                    rm ./-.o && echo -DCONFIG_AS_AVX2=1)
else ifeq ($(HAS_NEON),yes)
        OBJS   += neon.o neon1.o neon2.o neon4.o neon8.o
        CFLAGS += -DCONFIG_KERNEL_MODE_NEON=1
else
        HAS_ALTIVEC := $(shell printf '\#include <altivec.h>\nvector int a;\n' |\
                         gcc -c -x c - >&/dev/null && \
                         rm ./-.o && echo yes)
        ifeq ($(HAS_ALTIVEC),yes)
                OBJS += altivec1.o altivec2.o altivec4.o altivec8.o
        endif
endif
ifeq ($(ARCH),tilegx)
OBJS += tilegx8.o
endif
else # CONFIG_SYNO_BACKPORT_ARM_CRYPTO
ifeq ($(IS_X86),yes)
        OBJS   += mmx.o sse1.o sse2.o avx2.o recov_ssse3.o recov_avx2.o
        CFLAGS += $(shell echo "vpbroadcastb %xmm0, %ymm1" |	\
                    gcc -c -x assembler - >&/dev/null &&	\
                    rm ./-.o && echo -DCONFIG_AS_AVX2=1)
else
        HAS_ALTIVEC := $(shell echo -e '\#include <altivec.h>\nvector int a;' |\
                         gcc -c -x c - >&/dev/null && \
                         rm ./-.o && echo yes)
        ifeq ($(HAS_ALTIVEC),yes)
                OBJS += altivec1.o altivec2.o altivec4.o altivec8.o
        endif
endif
endif # CONFIG_SYNO_BACKPORT_ARM_CRYPTO

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: ../%.c
	cp -f $< $@

%.uc: ../%.uc
	cp -f $< $@

all:	raid6.a raid6test

raid6.a: $(OBJS)
	 rm -f $@
	 $(AR) cq $@ $^
	 $(RANLIB) $@

raid6test: test.c raid6.a
	$(CC) $(CFLAGS) -o raid6test $^

ifeq ($(CONFIG_SYNO_BACKPORT_ARM_CRYPTO),y)
neon1.c: neon.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=1 < neon.uc > $@

neon2.c: neon.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=2 < neon.uc > $@

neon4.c: neon.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=4 < neon.uc > $@

neon8.c: neon.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=8 < neon.uc > $@
endif # CONFIG_SYNO_BACKPORT_ARM_CRYPTO

altivec1.c: altivec.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=1 < altivec.uc > $@

altivec2.c: altivec.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=2 < altivec.uc > $@

altivec4.c: altivec.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=4 < altivec.uc > $@

altivec8.c: altivec.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=8 < altivec.uc > $@

int1.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=1 < int.uc > $@

int2.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=2 < int.uc > $@

int4.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=4 < int.uc > $@

int8.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=8 < int.uc > $@

int16.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=16 < int.uc > $@

int32.c: int.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=32 < int.uc > $@

ifeq ($(CONFIG_SYNO_BACKPORT_ARM_CRYPTO),y)
tilegx8.c: tilegx.uc ../unroll.awk
	$(AWK) ../unroll.awk -vN=8 < tilegx.uc > $@
endif # CONFIG_SYNO_BACKPORT_ARM_CRYPTO

tables.c: mktables
	./mktables > tables.c

ifeq ($(CONFIG_SYNO_BACKPORT_ARM_CRYPTO),y)
clean:
	rm -f *.o *.a mktables mktables.c *.uc int*.c altivec*.c neon*.c tables.c raid6test
	rm -f tilegx*.c
else # CONFIG_SYNO_BACKPORT_ARM_CRYPTO
clean:
	rm -f *.o *.a mktables mktables.c *.uc int*.c altivec*.c tables.c raid6test
endif # CONFIG_SYNO_BACKPORT_ARM_CRYPTO

spotless: clean
	rm -f *~
