cmd_scripts/kconfig/kconfig_load.o := gcc -Wp,-MD,scripts/kconfig/.kconfig_load.o.d -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer   -DCURSES_LOC="<ncurses.h>" -DLOCALE   -c -o scripts/kconfig/kconfig_load.o scripts/kconfig/kconfig_load.c

deps_scripts/kconfig/kconfig_load.o := \
  scripts/kconfig/kconfig_load.c \
  /usr/include/dlfcn.h \
  /usr/include/features.h \
  /usr/include/bits/predefs.h \
  /usr/include/sys/cdefs.h \
  /usr/include/bits/wordsize.h \
  /usr/include/gnu/stubs.h \
  /usr/include/gnu/stubs-32.h \
  /usr/lib/gcc/i486-linux-gnu/4.4.3/include/stddef.h \
  /usr/include/bits/dlfcn.h \
  /usr/include/stdio.h \
  /usr/include/bits/types.h \
  /usr/include/bits/typesizes.h \
  /usr/include/libio.h \
  /usr/include/_G_config.h \
  /usr/include/wchar.h \
  /usr/lib/gcc/i486-linux-gnu/4.4.3/include/stdarg.h \
  /usr/include/bits/stdio_lim.h \
  /usr/include/bits/sys_errlist.h \
  /usr/include/bits/stdio.h \
  /usr/include/bits/stdio2.h \
  /usr/include/stdlib.h \
  /usr/include/sys/types.h \
  /usr/include/time.h \
  /usr/include/endian.h \
  /usr/include/bits/endian.h \
  /usr/include/bits/byteswap.h \
  /usr/include/sys/select.h \
  /usr/include/bits/select.h \
  /usr/include/bits/sigset.h \
  /usr/include/bits/time.h \
  /usr/include/sys/sysmacros.h \
  /usr/include/bits/pthreadtypes.h \
  /usr/include/alloca.h \
  /usr/include/bits/stdlib.h \
  scripts/kconfig/lkc.h \
    $(wildcard include/config/list.h) \
  scripts/kconfig/expr.h \
  /usr/lib/gcc/i486-linux-gnu/4.4.3/include/stdbool.h \
  /usr/include/libintl.h \
  /usr/include/locale.h \
  /usr/include/bits/locale.h \
  /usr/include/xlocale.h \
  scripts/kconfig/lkc_defs.h \
  scripts/kconfig/lkc_proto.h \

scripts/kconfig/kconfig_load.o: $(deps_scripts/kconfig/kconfig_load.o)

$(deps_scripts/kconfig/kconfig_load.o):
