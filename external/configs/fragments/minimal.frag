# Minimal image: BusyBox init, mdev, no package manager
BR2_ROOTFS_DEVICE_CREATION_DYNAMIC_MDEV=y
BR2_TARGET_GENERIC_GETTY=n
# read-only root filesystem (data partition holds everything persistent)
# BR2_TARGET_GENERIC_REMOUNT_ROOTFS_RW is not set
