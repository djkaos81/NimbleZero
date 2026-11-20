################################################################################
#
# nz-drivers - M5Stack Cardputer Zero kernel modules, overlays, firmware
#
################################################################################

NZ_DRIVERS_VERSION = 4f7aeee9fc103ebb03d22cac476bb191ecfb321e
NZ_DRIVERS_SITE = $(call github,m5stack,m5stack-linux-dtoverlays,$(NZ_DRIVERS_VERSION))
NZ_DRIVERS_LICENSE = GPL-2.0
NZ_DRIVERS_LICENSE_FILES = COPYING

NZ_DRIVERS_MODULE_SUBDIRS = \
	modules/m5ioe1-1.0 \
	modules/st7789v-1.0 \
	modules/tca8418-1.0 \
	modules/es8389-1.0 \
	modules/bq27220-1.0 \
	modules/CardputerZero \
	modules/imu-1.0/bmi270 \
	modules/imu-1.0/magnetometer

# The IMU Makefiles key off Kconfig symbols; the production board is "v0.5".
NZ_DRIVERS_MODULE_MAKE_OPTS = \
	CONFIG_CARDPUTERO_V0_5=y \
	CONFIG_BMI270=m CONFIG_BMI270_I2C=m \
	CONFIG_BMC150_MAGN=m CONFIG_BMC150_MAGN_I2C=m CONFIG_BMC150_MAGN_BMI270=m

NZ_DRIVERS_OVERLAYS = \
	modules/CardputerZero/cardputerzero-v5-overlay \
	modules/CardputerZero/cardputerzero-v3-overlay \
	modules/CardputerZero/spk-gpio24-high-overlay \
	modules/CardputerZero/spi0-spidev2-gpio22-overlay \
	modules/CardputerZero/camera-gpio16-high-overlay \
	modules/CardputerZero/camera-py12-high-overlay \
	modules/CardputerZero/mic-pyg4-high-overlay \
	modules/bq27220-1.0/bq27220_v5 \
	modules/bq27220-1.0/bq27220_v3 \
	modules/imu-1.0/bmi270_bmm150_overlay \
	modules/imu-1.0/bmi270_overlay

# Same recipe as the upstream Makefiles (cpp + dtc -@), using the kernel's dtc.
define NZ_DRIVERS_BUILD_OVERLAYS
	$(foreach o,$(NZ_DRIVERS_OVERLAYS),\
		$(HOSTCC) -E -nostdinc -undef -x assembler-with-cpp -DCONFIG_CARDPUTERO_V0_5=1 \
			-o $(@D)/$(o).dts.tmp $(@D)/$(o).dts && \
		$(LINUX_DIR)/scripts/dtc/dtc -@ -q -I dts -O dtb \
			-o $(@D)/$(o).dtbo $(@D)/$(o).dts.tmp
	)
endef
NZ_DRIVERS_POST_BUILD_HOOKS += NZ_DRIVERS_BUILD_OVERLAYS

# NimbleZero's own overlays for the LCD: panel clock, TE (sync) pin.  See docs/DISPLAY.md.
NZ_DRIVERS_NZ_OVERLAYS = nz-panel-100m nz-panel-70m nz-panel-te
define NZ_DRIVERS_BUILD_NZ_OVERLAYS
	$(foreach o,$(NZ_DRIVERS_NZ_OVERLAYS),\
		$(LINUX_DIR)/scripts/dtc/dtc -@ -q -I dts -O dtb -o $(@D)/$(o).dtbo \
			$(NZ_DRIVERS_PKGDIR)/overlays/$(o).dts
	)
endef
NZ_DRIVERS_POST_BUILD_HOOKS += NZ_DRIVERS_BUILD_NZ_OVERLAYS

NZ_DRIVERS_INSTALL_IMAGES = YES
define NZ_DRIVERS_INSTALL_IMAGES_CMDS
	mkdir -p $(BINARIES_DIR)/nz-overlays
	$(foreach o,$(NZ_DRIVERS_OVERLAYS),\
		$(INSTALL) -m 0644 $(@D)/$(o).dtbo $(BINARIES_DIR)/nz-overlays/
	)
	$(foreach o,$(NZ_DRIVERS_NZ_OVERLAYS),\
		$(INSTALL) -m 0644 $(@D)/$(o).dtbo $(BINARIES_DIR)/nz-overlays/
	)
endef

define NZ_DRIVERS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 "$(@D)/modules/st7789v-1.0/cardputerzero,st7789v_lcd.bin" \
		"$(TARGET_DIR)/lib/firmware/cardputerzero,st7789v_lcd.bin"
	$(INSTALL) -D -m 0644 $(@D)/modules/imu-1.0/bmi270-init-data.fw \
		$(TARGET_DIR)/lib/firmware/bmi270-init-data.fw
	$(INSTALL) -D -m 0755 $(@D)/modules/bq27220-1.0/bqcontrol.py \
		$(TARGET_DIR)/usr/share/nz/bqcontrol.py
endef

$(eval $(kernel-module))
$(eval $(generic-package))
