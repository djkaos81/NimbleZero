# bmi270-init-data.fw (IMU configuration data)

The image installs `/lib/firmware/bmi270-init-data.fw` from M5Stack's driver repository
(m5stack-linux-dtoverlays, commit 4f7aeee9fc103ebb03d22cac476bb191ecfb321e,
`modules/imu-1.0/bmi270-init-data.fw`). That repository is labelled GPL-2.0, but the file is
Bosch's.

Checked on 2026-11-19:

- The file (8192 bytes, SHA-256 beginning `2d75e68e343a13ff`) is byte-for-byte identical to
  the array `bmi270_config_file` in `bmi270.c` of Bosch Sensortec's own driver package,
  github.com/boschsensortec/BMI270_SensorAPI, commit 41129fcfe39c583ee5462d79195741945d51c1fe
  (file version v2.86.1).
- That file and the package carry the BSD-3-Clause licence, copyright 2023 Bosch Sensortec
  GmbH (`LICENSE` in this folder is the package's licence file, unchanged).

BSD-3-Clause allows redistribution in binary form if the copyright notice, the conditions
and the disclaimer are reproduced in the documentation or other materials provided with
the distribution, and Bosch's name is not used to endorse the product.

To do in the release build: ship this licence text in `/usr/share/licenses` and list the
file in the licence check as a reviewed item (rc11 counts it under nz-drivers' GPL-2.0 label).

This is a record of what was compared, not legal advice.
