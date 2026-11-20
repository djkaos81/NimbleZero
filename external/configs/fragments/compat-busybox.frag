# Configuration A: BusyBox init + minimal service shims
BR2_TARGET_GENERIC_GETTY=n
BR2_PACKAGE_NZ_COMPAT_SHIMS=y
BR2_ROOTFS_DEVICE_CREATION_DYNAMIC_EUDEV=y
# read-only root filesystem (data partition holds everything persistent)
# BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW is not set
