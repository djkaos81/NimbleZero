################################################################################
#
# nz-diag
#
################################################################################

NZ_DIAG_VERSION = 1.0
NZ_DIAG_SITE = $(BR2_EXTERNAL_NZ_PATH)/package/nz-diag/src
NZ_DIAG_SITE_METHOD = local
NZ_DIAG_LICENSE = GPL-2.0
NZ_DIAG_DEPENDENCIES = linux

define NZ_DIAG_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) all $(TARGET_CONFIGURE_OPTS) \
		KERNEL_FONT=$(LINUX_DIR)/lib/fonts/font_6x8.c \
		EXTRA_INC=-I$(BR2_EXTERNAL_NZ_PATH)/../sdk/nzui
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -o $(@D)/nz-paneltest \
		$(BR2_EXTERNAL_NZ_PATH)/package/nz-paneltest/src/nz-paneltest.c $(TARGET_LDFLAGS)
endef

define NZ_DIAG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/nz-diag $(TARGET_DIR)/usr/bin/nz-diag
	$(INSTALL) -D -m 0755 $(@D)/nz-growpart $(TARGET_DIR)/usr/sbin/nz-growpart
	$(INSTALL) -D -m 0755 $(@D)/nz-paneltest $(TARGET_DIR)/usr/bin/nz-paneltest
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_NZ_PATH)/package/nz-paneltest/nz-panel $(TARGET_DIR)/usr/sbin/nz-panel
endef

$(eval $(generic-package))
