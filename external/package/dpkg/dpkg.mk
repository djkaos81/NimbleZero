################################################################################
#
# dpkg
#
################################################################################

DPKG_VERSION = 1.22.22
DPKG_SOURCE = dpkg_$(DPKG_VERSION).tar.xz
DPKG_SITE = https://deb.debian.org/debian/pool/main/d/dpkg
DPKG_LICENSE = GPL-2.0+
DPKG_LICENSE_FILES = COPYING
DPKG_DEPENDENCIES = host-pkgconf libmd zlib xz bzip2 zstd
DPKG_CONF_OPTS = \
	--disable-shared \
	--enable-static \
	--disable-dselect \
	--disable-nls \
	--disable-devel-docs \
	--without-libselinux \
	--with-libmd \
	--with-libz \
	--with-liblzma \
	--with-libbz2 \
	--with-libzstd \
	--with-admindir=/var/lib/dpkg
DPKG_CONF_ENV = PERL=/usr/bin/perl

# Keep only the C programs on the target: the dpkg-dev Perl scripts and
# libdpkg-perl need a Perl interpreter the image does not have.
define DPKG_REMOVE_DEV_SCRIPTS
	rm -rf $(TARGET_DIR)/usr/share/perl5/Dpkg $(TARGET_DIR)/usr/share/perl5/Dpkg.pm \
		$(TARGET_DIR)/usr/lib/perl5 $(TARGET_DIR)/usr/share/dpkg/*.mk $(TARGET_DIR)/usr/share/dpkg/sh \
		$(TARGET_DIR)/usr/share/zsh $(TARGET_DIR)/usr/share/polkit-1/actions/org.dpkg.pkexec.update-alternatives.policy
	cd $(TARGET_DIR)/usr/bin && rm -f dpkg-architecture dpkg-buildflags dpkg-buildpackage \
		dpkg-buildapi dpkg-buildtree dpkg-checkbuilddeps dpkg-distaddfile dpkg-genbuildinfo \
		dpkg-genchanges dpkg-gencontrol dpkg-gensymbols dpkg-mergechangelogs dpkg-name \
		dpkg-parsechangelog dpkg-scanpackages dpkg-scansources dpkg-shlibdeps dpkg-source \
		dpkg-vendor dpkg-fsys-usrunmess
endef
DPKG_POST_INSTALL_TARGET_HOOKS += DPKG_REMOVE_DEV_SCRIPTS

define DPKG_INSTALL_ADMINDIR
	mkdir -p $(TARGET_DIR)/var/lib/dpkg/info $(TARGET_DIR)/var/lib/dpkg/updates \
		$(TARGET_DIR)/var/lib/dpkg/alternatives $(TARGET_DIR)/etc/dpkg/dpkg.cfg.d
	touch $(TARGET_DIR)/var/lib/dpkg/status $(TARGET_DIR)/var/lib/dpkg/available
endef
DPKG_POST_INSTALL_TARGET_HOOKS += DPKG_INSTALL_ADMINDIR

$(eval $(autotools-package))
