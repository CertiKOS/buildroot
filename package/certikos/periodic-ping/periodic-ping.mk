################################################################################
#
# periodic-ping
#
################################################################################

PERIODIC_PING_LICENSE = Public Domain

define PERIODIC_PING_EXTRACT_CMDS
	cp package/certikos/periodic-ping/periodic_ping.c $(@D)/
endef

define PERIODIC_PING_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -Os -s periodic_ping.c -o periodic_ping)
endef

define PERIODIC_PING_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/periodic_ping $(TARGET_DIR)/usr/bin/periodic_ping
endef

$(eval $(generic-package))
