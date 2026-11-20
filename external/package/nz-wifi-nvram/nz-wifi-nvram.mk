################################################################################
#
# nz-wifi-nvram - CYW43439 NVRAM for the Raspberry Pi CM0
#
################################################################################

# RPi-Distro/firmware-nonfree, branch trixie, 2026-11
NZ_WIFI_NVRAM_VERSION = 5d2295b633d94641253e16ba90fc847436fc9d4a
NZ_WIFI_NVRAM_SITE = https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/$(NZ_WIFI_NVRAM_VERSION)/debian/added-firmware/cypress
NZ_WIFI_NVRAM_SOURCE = cyfmac43439-sdio.txt
# debian/copyright of that repository: "Files: debian/added-firmware/*/*43439*",
# Cypress binary redistribution licence (same terms as the firmware itself).
NZ_WIFI_NVRAM_LICENSE = binary-redist-Cypress
NZ_WIFI_NVRAM_REDISTRIBUTE = YES
NZ_WIFI_NVRAM_DEPENDENCIES = brcmfmac_sdio-firmware-rpi

NZ_WIFI_NVRAM_BOARD = raspberrypi,0-compute-module

define NZ_WIFI_NVRAM_EXTRACT_CMDS
	cp $(NZ_WIFI_NVRAM_DL_DIR)/$(NZ_WIFI_NVRAM_SOURCE) $(@D)/
endef

define NZ_WIFI_NVRAM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/cyfmac43439-sdio.txt \
		"$(TARGET_DIR)/lib/firmware/brcm/brcmfmac43439-sdio.$(NZ_WIFI_NVRAM_BOARD).txt"
	ln -sf ../cypress/cyfmac43439-sdio.bin \
		"$(TARGET_DIR)/lib/firmware/brcm/brcmfmac43439-sdio.$(NZ_WIFI_NVRAM_BOARD).bin"
	ln -sf ../cypress/cyfmac43439-sdio.clm_blob \
		"$(TARGET_DIR)/lib/firmware/brcm/brcmfmac43439-sdio.$(NZ_WIFI_NVRAM_BOARD).clm_blob"
endef

$(eval $(generic-package))
