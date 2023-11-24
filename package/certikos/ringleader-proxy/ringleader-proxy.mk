################################################################################
#
# ringleader-proxy
#
################################################################################

RINGLEADER_PROXY_LICENSE = Public Domain
RINGLEADER_PROXY_DEPENDENCIES = liburing

define RINGLEADER_PROXY_EXTRACT_CMDS
	cp package/certikos/ringleader-proxy/proxy.c $(@D)/
	cp package/certikos/ringleader-proxy/spawn.c $(@D)/
endef

define RINGLEADER_PROXY_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -O2 -s proxy.c -o ringleader_proxy)
	(cd $(@D); $(TARGET_CC) -Wall -O2 -luring -s spawn.c -o ringleader)
endef

define RINGLEADER_PROXY_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/ringleader_proxy $(TARGET_DIR)/usr/bin/ringleader_proxy
	install -m 0755 -D $(@D)/ringleader $(TARGET_DIR)/usr/bin/ringleader
endef

$(eval $(generic-package))
