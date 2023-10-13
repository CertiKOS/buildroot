################################################################################
#
# certikos-io-uring
#
################################################################################

CERTIKOS_IO_URING_LICENSE = Public Domain

define CERTIKOS_IO_URING_EXTRACT_CMDS
	cp package/certikos-io-uring/ringleader_proxy.c $(@D)/
	cp package/certikos-io-uring/periodic_ping.c $(@D)/
endef

define CERTIKOS_IO_URING_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -Os -s ringleader_proxy.c -o ringleader_proxy)
	(cd $(@D); $(TARGET_CC) -Wall -Os -s periodic_ping.c -o periodic_ping)
endef

define CERTIKOS_IO_URING_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/ringleader_proxy $(TARGET_DIR)/usr/bin/ringleader_proxy
	install -m 0755 -D $(@D)/periodic_ping $(TARGET_DIR)/usr/bin/periodic_ping
endef

$(eval $(generic-package))
