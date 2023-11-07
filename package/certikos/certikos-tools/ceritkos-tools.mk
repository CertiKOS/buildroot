################################################################################
#
# certikos-tools
#
################################################################################

CERTIKOS_TOOLS_LICENSE = Public Domain

define CERTIKOS_TOOLS_EXTRACT_CMDS
	cp package/certikos/certikos-tools/S99shared $(@D)/
endef

define CERTIKOS_TOOLS_BUILD_CMDS
	echo "none"
endef

define CERTIKOS_TOOLS_INSTALL_TARGET_CMDS
	install -m 0755 -D $(@D)/S99shared $(TARGET_DIR)/etc/init.d/S99shared
endef

$(eval $(generic-package))
