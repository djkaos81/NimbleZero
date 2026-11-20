#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Licence compliance check for a NimbleZero image (run after `make legal-info`).

usage: check-licenses.py OUTPUT_DIR [--json report.json]

Fails (exit 1) when the image could not be published as it stands:
  1. a package in the image has no licence recorded ("unknown" or empty);
  2. a package is proprietary / non-free / not redistributable and is not on the
     REVIEWED list below (binary blobs whose redistribution terms were read);
  3. Buildroot could not save a package's licence text or source and said so;
  4. ffmpeg was configured with "nonfree" parts (the result may not be distributed);
  5. a copyleft (GPL/LGPL/MPL) package has no source archive in legal-info/sources
     and is not built from this repository;
  6. one of the project's own packages does not declare GPL-2.0.
It is a mechanical check of what Buildroot recorded, not legal advice.
"""
import csv, json, os, re, sys, glob

O = sys.argv[1]; L = os.path.join(O, "legal-info")
results = []
def check(name, ok, detail=""):
    results.append({"check": name, "ok": bool(ok), "detail": detail if isinstance(detail, (list, dict)) else str(detail)})
    print(("PASS " if ok else "FAIL ") + name + (f"  - {str(detail)[:300]}" if detail else ""))

# Binary-only components: redistribution terms read on the date given.
REVIEWED = {
    "rpi-firmware": "Raspberry Pi boot firmware; LICENCE.broadcom: redistribution in binary form permitted for use with Raspberry Pi devices",
    "brcmfmac_sdio-firmware-rpi": "Wi-Fi/Bluetooth firmware (LibreELEC/brcmfmac_sdio-firmware-rpi, taken from RPi-Distro/firmware-nonfree); Cypress/Broadcom binary redistribution licence, text saved by Buildroot (LICENSE)",
    "nz-wifi-nvram": "CYW43439 NVRAM text for the CM0 from RPi-Distro/firmware-nonfree; same Cypress binary redistribution terms as the firmware",
}
OWN = {"nz-apps", "nz-compat", "nz-diag", "nz-launcher"}
LOCAL_SOURCE = OWN | {"nz-drivers"}      # nz-drivers: source archive is saved by Buildroot

man = os.path.join(L, "manifest.csv")
if not os.path.isfile(man):
    check("legal-info was generated (make legal-info)", False, man); rows = []
else:
    rows = list(csv.DictReader(open(man, newline="")))
    check(f"legal-info manifest lists the target packages ({len(rows)})", len(rows) > 20)
lic = {r["PACKAGE"]: (r.get("LICENSE") or "").strip() for r in rows}

# The prebuilt toolchain has no licence in Buildroot's manifest.  Its runtime libraries are in
# the image, so it needs its own record: Bootlin's manifest for exactly this version plus the
# licence texts, kept in docs/third-party/toolchain and shipped in the image.
TOP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
tc = [r for r in rows if r["PACKAGE"].startswith("toolchain-external")]
if tc:
    d = os.path.join(TOP, "docs", "third-party", "toolchain")
    summ = open(os.path.join(d, "summary.csv")).read() if os.path.isfile(os.path.join(d, "summary.csv")) else ""
    ver = tc[0]["VERSION"]; src = tc[0].get("SOURCE ARCHIVE", "")
    readme = open(os.path.join(d, "README")).read() if os.path.isfile(os.path.join(d, "README")) else ""
    ok = bool(summ) and ver in readme and '"glibc"' in summ and '"gcc-final"' in summ and "unknown" not in [x.split('","')[2].lower() for x in summ.splitlines() if x.count('","') > 2 and not x.startswith('"skeleton')] \
         and all(os.path.isfile(os.path.join(d, f)) for f in ("COPYING.LIB", "COPYING3", "COPYING.RUNTIME"))
    check(f"prebuilt toolchain {ver}: component manifest and runtime-library licence texts on record", ok, src)
    if ok: lic[tc[0]["PACKAGE"]] = "see docs/third-party/toolchain (glibc LGPL-2.1+, GCC runtime GPL-3.0-with-GCC-exception)"

# Free components for which Buildroot records a licence text but no licence name.
# Accepted only while that text is actually saved; the name used is given here.
NAMED = {"dejavu": ("Bitstream Vera Fonts licence, Arev Fonts licence, public domain (DejaVu changes)", "LICENSE", "Bitstream Vera Fonts Copyright")}
for pkg, (name, fn, marker) in NAMED.items():
    if pkg in lic and lic[pkg].lower() in ("", "unknown"):
        hit = [f for f in glob.glob(os.path.join(L, "licenses", pkg + "-*", fn)) if marker in open(f, errors="replace").read()]
        check(f"{pkg}: licence text saved (free licence without a name in Buildroot)", bool(hit), name)
        if hit: lic[pkg] = name

unknown = sorted(p for p, l in lic.items() if not l or l.lower() in ("unknown", "none", "not saved"))
check("every package has a licence recorded", not unknown, unknown)

BAD = re.compile(r"propriet|non-?free|nonfree|binary-|no redistribution|not redistributable|unknown", re.I)
flagged = sorted(p for p, l in lic.items() if BAD.search(l))
unreviewed = [f"{p} [{lic[p]}]" for p in flagged if p not in REVIEWED]
check("no proprietary / non-free package outside the reviewed binary blobs", not unreviewed, unreviewed)
check("reviewed binary blobs (redistributable, terms on record)", True, {p: REVIEWED[p] for p in flagged if p in REVIEWED})

warn = []
for f in glob.glob(os.path.join(L, "*.warnings")) + [os.path.join(L, "README")]:
    if os.path.isfile(f):
        warn += [l.strip() for l in open(f, errors="replace") if "WARNING" in l or "cannot save" in l.lower()]
# sources Buildroot was told not to save are only acceptable for the reviewed blobs and local packages
warn = [w for w in warn if not any(p in w for p in list(REVIEWED) + list(LOCAL_SOURCE) + ["rootfs-overlay", "toolchain-external"])]
# Buildroot never saves its own tree; acceptable only because the exact public revision is pinned
env = open(os.path.join(TOP, "versions.env")).read() if os.path.isfile(os.path.join(TOP, "versions.env")) else ""
if re.search(r"^BUILDROOT_COMMIT=[0-9a-f]{40}", env, re.M) and "BUILDROOT_TAG=" in env:
    warn = [w for w in warn if "Buildroot source code has not been saved" not in w]
check("Buildroot reported no unsaved licence or source", not warn, warn[:10])

cfg = open(os.path.join(O, ".config")).read() if os.path.isfile(os.path.join(O, ".config")) else ""
if "BR2_PACKAGE_FFMPEG=y" in cfg:
    ch = (glob.glob(os.path.join(O, "build", "ffmpeg-*", "config.h")) or [""])[0]
    c = open(ch).read() if ch else ""
    m = re.search(r'#define FFMPEG_LICENSE "(.*?)"', c)
    nonfree = "BR2_PACKAGE_FFMPEG_NONFREE=y" in cfg or re.search(r"#define CONFIG_NONFREE 1", c) or "--enable-nonfree" in c
    check("ffmpeg: no nonfree parts; resulting licence is a GPL", bool(c) and not nonfree and bool(m) and "GPL" in m.group(1) and "nonfree" not in m.group(1).lower(), m.group(1) if m else "config.h not found")

srcdir = os.path.join(L, "sources")
missing = []
for p, l in sorted(lic.items()):
    if not re.search(r"GPL|MPL|EPL|CDDL", l) or p in OWN: continue
    d = glob.glob(os.path.join(srcdir, p + "-*")) + glob.glob(os.path.join(srcdir, p))
    if not any(os.listdir(x) for x in d if os.path.isdir(x)): missing.append(p)
check("source archives saved for every copyleft package", not missing, missing)

own = {p: lic.get(p) for p in OWN if p in lic}
check("the project's own packages declare GPL-2.0", bool(own) and all(l.startswith("GPL-2.0") for l in own.values()), own)
top = os.path.join(TOP, "COPYING")
check("GPL-2.0 text (COPYING) present in the source tree", os.path.isfile(top) and "Version 2, June 1991" in open(top).read())

by = {}
for p, l in lic.items(): by.setdefault(l, []).append(p)
fails = [r for r in results if not r["ok"]]
print(f"\n{len(results) - len(fails)}/{len(results)} licence checks passed; {len(lic)} packages, {len(by)} distinct licence strings")
if "--json" in sys.argv:
    json.dump({"output": O, "passed": len(results) - len(fails), "total": len(results), "results": results,
               "packages": lic}, open(sys.argv[sys.argv.index("--json") + 1], "w"), indent=1, sort_keys=True)
sys.exit(1 if fails else 0)
