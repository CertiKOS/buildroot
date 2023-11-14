################################################################################
#
# io-uring-perf
#
################################################################################

IO_URING_PERF_LICENSE = Public Domain
IO_URING_PERF_DEPENDENCIES = liburing

define IO_URING_PERF_EXTRACT_CMDS
	cp package/certikos/io-uring-perf/io_uring_cat.c $(@D)/
	cp package/certikos/io-uring-perf/io_uring_cat_chained.c $(@D)/
endef

define IO_URING_PERF_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -O2 -luring -s io_uring_cat.c -o io_uring_cat)
	(cd $(@D); $(TARGET_CC) -Wall -O2 -luring -s io_uring_cat_chained.c -o io_uring_cat_chained)
endef

define IO_URING_PERF_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/io_uring_cat $(TARGET_DIR)/usr/bin/io_uring_cat
	install -m 0755 -D $(@D)/io_uring_cat_chained $(TARGET_DIR)/usr/bin/io_uring_cat_chained
endef

$(eval $(generic-package))
