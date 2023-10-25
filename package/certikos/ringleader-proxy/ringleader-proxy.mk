################################################################################
#
# ringleader-proxy
#
################################################################################

RINGLEADER_PROXY_LICENSE = Public Domain

define RINGLEADER_PROXY_EXTRACT_CMDS
	cp package/certikos/ringleader-proxy/proxy.c $(@D)/
endef

define RINGLEADER_PROXY_BUILD_CMDS
	(cd $(@D); $(TARGET_CC) -Wall -Os -s proxy.c -o ringleader_proxy)
endef

define RINGLEADER_PROXY_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/ringleader_proxy $(TARGET_DIR)/usr/bin/ringleader_proxy
endef

$(eval $(generic-package))
