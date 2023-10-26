################################################################################
#
# io-uring-perf
#
################################################################################

IO_URING_PERF_LICENSE = Public Domain

define IO_URING_PERF_EXTRACT_CMDS
	cp package/certikos/io-uring-perf/io_uring_echo.c $(@D)/
endef

define IO_URING_PERF_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -Os -luring -s io_uring_echo.c -o io_uring_echo)
endef

define IO_URING_PERF_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/io_uring_echo $(TARGET_DIR)/usr/bin/io_uring_echo
endef

$(eval $(generic-package))
