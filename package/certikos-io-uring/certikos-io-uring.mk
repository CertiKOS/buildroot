################################################################################
#
# certikos-io-uring
#
################################################################################

CERTIKOS_IO_URING_LICENSE = Public Domain

define CERTIKOS_IO_URING_EXTRACT_CMDS
	cp package/certikos-io-uring/io_uring_test.c $(@D)/
endef

define CERTIKOS_IO_URING_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -Os -s io_uring_test.c -o io_uring_test)
endef

define CERTIKOS_IO_URING_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/io_uring_test $(TARGET_DIR)/usr/bin/io_uring_test
endef

$(eval $(generic-package))
