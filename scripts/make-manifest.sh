#!/bin/sh
# Build manifest: exact inputs of an image (tree revision, Buildroot revision,
# every package with version and source archive hash).
v=$1; BR=$2; O=$3; DL=$4; TOP=$(cd "$(dirname "$0")/.." && pwd)
echo "NimbleZero build manifest"
echo "variant: $v"
echo "tree: $(git -C "$TOP" describe --tags --always --dirty 2>/dev/null) ($(git -C "$TOP" rev-parse HEAD 2>/dev/null))"
echo "image_release: $(tr "\n" " " < "$O/target/etc/nz-release" 2>/dev/null)"
echo "buildroot: $(git -C "$BR" describe --tags --always) ($(git -C "$BR" rev-parse HEAD))"
echo "host: $(uname -srm)"
echo "source_date_epoch: ${SOURCE_DATE_EPOCH:-unset}"
echo "defconfig_sha256: $(sha256sum "$TOP/external/configs/nz_${v}_defconfig" | cut -d' ' -f1)"
echo "kernel_config_sha256: $(sha256sum "$O"/build/linux-custom/.config 2>/dev/null | cut -d' ' -f1)"
echo
echo "# package version license | source archive sha256"
make --no-print-directory -s -C "$BR" O="$O" show-info 2>/dev/null | python3 -c '
import json,sys,os,hashlib
d=json.load(sys.stdin); dl=sys.argv[1]
for k,v in sorted(d.items()):
    if v.get("virtual"): continue
    srcs=[]
    for x in v.get("downloads") or []:
        p=os.path.join(dl,os.path.basename(v.get("dl_dir") or k),x["source"])
        h=hashlib.sha256(open(p,"rb").read()).hexdigest() if os.path.exists(p) else "(not in dl)"
        srcs.append(x["source"]+" "+h)
    print(k, v.get("version",""), "["+(v.get("licenses") or "?")+"]", "|", "; ".join(srcs))' "$DL"
