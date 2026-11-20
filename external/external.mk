include $(sort $(wildcard $(BR2_EXTERNAL_NZ_PATH)/package/*/*.mk))

# Build the Raspberry Pi overlays from the very kernel tree we boot (instead of
# taking prebuilt ones from the firmware repository, which belong to another
# kernel build) and stage them for the boot partition.
define NZ_LINUX_BUILD_OVERLAYS
	$(LINUX_MAKE_ENV) $(BR2_MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) dtbs
	mkdir -p $(BINARIES_DIR)/rpi-firmware/overlays
	cp $(LINUX_DIR)/arch/arm64/boot/dts/overlays/*.dtb* $(BINARIES_DIR)/rpi-firmware/overlays/
	cp $(LINUX_DIR)/arch/arm64/boot/dts/overlays/README $(BINARIES_DIR)/rpi-firmware/overlays/ 2>/dev/null || true
endef
LINUX_POST_INSTALL_IMAGES_HOOKS += NZ_LINUX_BUILD_OVERLAYS

# Debian programs link libjpeg.so.62 (libjpeg62-turbo).  Buildroot builds libjpeg-turbo with
# the "JPEG 8" interface (libjpeg.so.8); the later option wins, giving Debian's library name.
JPEG_TURBO_CONF_OPTS += -DWITH_JPEG8=OFF
