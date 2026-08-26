#!/usr/bin/env python3
"""Rewrites releases.json from whatever is sitting in releases/.

The binaries are gitignored, this manifest is committed, so the repo still records
exactly what each release shipped and what its checksums were.
"""
import hashlib, json, os, pathlib, datetime

ROOT = pathlib.Path(__file__).resolve().parent.parent
VER = (ROOT / "version.txt").read_text().strip()
REL = ROOT / "releases"

PLATFORMS = [
    ("macos",   "-macos",   "macOS 13+, Apple silicon. Unsigned, so the first launch needs right click then Open."),
    ("linux",   "-linux",   "x86_64, built on Ubuntu. Needs SDL2 and SDL2_image installed."),
    ("ios",     "-ios",     "Unsigned .ipa for sideloading with AltStore or Sideloadly."),
    ("android", "-android", "arm64-v8a and x86_64, minSdk 21."),
]

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

artifacts = []
for name, tag, note in PLATFORMS:
    hits = sorted(p for p in REL.glob("*") if p.is_file() and tag in p.name)
    for p in hits:
        artifacts.append({
            "platform": name,
            "file": p.name,
            "bytes": p.stat().st_size,
            "sha256": sha256(p),
            "notes": note,
        })

manifest = {
    "name": "gameboy-emu",
    "version": VER,
    "generated": datetime.datetime.now(datetime.timezone.utc)
                  .replace(microsecond=0).isoformat().replace("+00:00", "Z"),
    "artifacts": artifacts,
}
(ROOT / "releases.json").write_text(json.dumps(manifest, indent=2) + "\n")

print(f"\nreleases.json  —  {VER}, {len(artifacts)} artifact(s)")
for a in artifacts:
    print(f"  {a['platform']:8s} {a['bytes']/1024/1024:7.1f} MB  {a['file']}")
missing = [n for n, _, _ in PLATFORMS if not any(a["platform"] == n for a in artifacts)]
if missing:
    print(f"  missing: {', '.join(missing)}")
