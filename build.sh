#!/bin/sh
# One-command build for the NimbleZero images.
#
#   ./build.sh [VARIANT...]        build images (default: minimal). Variants: minimal compat
#   ./build.sh fetch [VARIANT...]  download every source into ./dl (enables offline builds)
#   ./build.sh sdk [VARIANT]       build the relocatable cross-compilation SDK tarball
#   NZ_OFFLINE=1 ./build.sh ...    forbid any network access during the build
#
# Layout: ./buildroot (pinned checkout), ./dl (source cache), ./out/<variant>
# (build trees), ./artifacts (images, checksums, manifests, test reports).
# Host: Linux or WSL2 on a case-sensitive filesystem (NOT under /mnt/c), ~25 GB free.
set -eu
TOP=$(cd "$(dirname "$0")" && pwd)
. "$TOP/versions.env"
BR="${NZ_BUILDROOT_DIR:-$TOP/buildroot}"
DL="${BR2_DL_DIR:-$TOP/dl}"
OUT="${NZ_OUT_DIR:-$TOP/out}"
ART="${NZ_ARTIFACTS_DIR:-$TOP/artifacts}"
MODE=build
case "${1:-}" in fetch|sdk) MODE=$1; shift ;; esac
[ $# -gt 0 ] || set -- minimal

if [ ! -d "$BR/.git" ]; then
	[ "${NZ_OFFLINE:-0}" = 1 ] && { echo "offline: $BR is missing" >&2; exit 1; }
	git clone --depth 1 -b "$BUILDROOT_TAG" "$BUILDROOT_URL" "$BR"
fi
have=$(git -C "$BR" rev-parse HEAD)
[ "$have" = "$BUILDROOT_COMMIT" ] || { echo "Buildroot checkout is $have, expected $BUILDROOT_COMMIT ($BUILDROOT_TAG)" >&2; exit 1; }

mk() { v=$1; shift; make --no-print-directory -C "$BR" O="$OUT/$v" BR2_EXTERNAL="$TOP/external" BR2_DL_DIR="$DL" "$@"; }
mkdir -p "$DL" "$OUT" "$ART"
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$TOP" log -1 --format=%ct 2>/dev/null || date +%s)}"

for v in "$@"; do
	cfg="nz_${v}_defconfig"
	[ -f "$TOP/external/configs/$cfg" ] || { echo "unknown variant '$v'" >&2; exit 1; }
	mk "$v" "$cfg" >/dev/null
	if [ "${NZ_OFFLINE:-0}" = 1 ]; then
		# make Buildroot fail instead of reaching for the network
		sed -i 's|^BR2_PRIMARY_SITE=.*|BR2_PRIMARY_SITE="file:///nonexistent"|; s|^# BR2_PRIMARY_SITE_ONLY is not set|BR2_PRIMARY_SITE_ONLY=y|' "$OUT/$v/.config"
		mk "$v" olddefconfig >/dev/null
	fi
	case "$MODE" in
	fetch) mk "$v" source ;;
	sdk)   mk "$v" sdk; cp "$OUT/$v"/images/*sdk-buildroot.tar.gz "$ART/nz-sdk-$v-$(uname -m).tar.gz"; echo "SDK: $ART/nz-sdk-$v-$(uname -m).tar.gz" ;;
	build)
		NZ_VARIANT="$v" mk "$v"
		# licences: collect texts and sources, check them, then rebuild the image so it carries them
		mk "$v" legal-info >/dev/null
		"$TOP/scripts/check-licenses.py" "$OUT/$v" --json "$ART/nimblezero-$v.licenses.json"
		NZ_VARIANT="$v" mk "$v"
		"$TOP/tests/check-kernel-config.sh" "$OUT/$v"
		img="$ART/nimblezero-$v.img"
		cp "$OUT/$v/images/sdcard.img" "$img"
		"$TOP/tests/check-image.py" "$img" --json "$ART/nimblezero-$v.image-check.json"
		"$TOP/scripts/make-manifest.sh" "$v" "$BR" "$OUT/$v" "$DL" > "$ART/nimblezero-$v.manifest.txt"
		xz -T0 -6 -k -f "$img"
		( cd "$ART" && sha256sum "nimblezero-$v.img" "nimblezero-$v.img.xz" > "nimblezero-$v.sha256" )
		echo "Image: $img.xz"
		;;
	esac
done
