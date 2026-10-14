#!/usr/bin/env python3
"""Deterministic audit of ARCHITECTURE.md claims against source code."""
import re
from pathlib import Path

doc = Path("ARCHITECTURE.md").read_text()
src = {p.name: p.read_text() for p in Path("src").glob("*") if p.suffix in (".cpp", ".h", ".hpp")}
settings = Path("bin/data/settings.xml").read_text()
addons = Path("addons.make").read_text()

common_dir = Path("../common/src")  # Pinopticap lives in core/, common/ is a sibling
common = {}
for p in common_dir.glob("*.hpp"):
    common[p.name] = p.read_text()

checks = []


def check(name, cond, detail=""):
    checks.append((name, bool(cond), detail))


# 1. settings.xml keys claimed in doc table (section 6 only; exclude OSC type tags, and only first column)
sec6 = doc.split("## 6. Configuration Reference")[1].split("## 7.")[0] if "## 6." in doc else doc
# Only first column of the table, and only alphabetic keys (not numeric fallbacks)
table_lines = [l for l in sec6.split("\n") if l.strip().startswith("| `")]
doc_keys = set()
for l in table_lines:
    m = re.match(r'\|\s*`([^`]+)`\s*\|', l)
    if m:
        k = m.group(1)
        if k.isalpha() or '_' in k:  # skip numeric fallbacks
            doc_keys.add(k)
xml_keys = set(re.findall(r'<(\w+)>', settings)) - {"settings"}  # 'settings' is XML root
osc_host_ok = "osc_host" in settings
missing_in_xml = sorted(k for k in doc_keys if k not in xml_keys and k != "osc_host")
check("doc config keys exist in settings.xml", not missing_in_xml,
      f"doc-only keys: {missing_in_xml}; osc_host present: {osc_host_ok}")
xml_only = sorted(k for k in xml_keys if k not in doc_keys and k not in ("width", "height", "debug"))
check("settings.xml keys documented", not xml_only, f"undocumented keys: {xml_only[:10]}")

# 2. addon list (bold text may contain several ofx names per line)
sec7 = doc.split("## 7. Build System")[1].split("## 8.")[0] if "## 7." in doc else doc
doc_addons = set(re.findall(r'`(ofx\w+)`', sec7))
make_addons = set(addons.split())
check("addons match addons.make", doc_addons == make_addons,
      f"doc-only: {sorted(doc_addons - make_addons)}, make-only: {sorted(make_addons - doc_addons)}")

# 3. function/method claims
for fn in ["checkHostName", "checkSessionId", "wsClientConnect", "triangulateBlobs",
           "sendOscBlobs", "drawWorld", "drawVideo", "addBlob", "addVideo", "bufferToImage",
           "imageToBuffer", "loadEyeCalibrations", "fitRays", "intersectRays",
           "getSessionId", "getHostName", "rawDepthToMeters"]:
    base = fn.rstrip("(")
    found_files = [f for f, s2 in src.items() if base in s2]
    if not found_files:
        found_files = [f"common/{f}" for f, s2 in common.items() if base in s2]
    check(f"symbol {fn}", found_files, ",".join(found_files) or "NOT FOUND")

# 4. port numbers
for port in ["7110", "7111", "7112", "7113", "7114", "7115"]:
    check(f"port {port} in settings.xml", port in settings)

# 5. LOC claims
loc = {name: text.count("\n") + 1 for name, text in src.items()}
total = sum(loc.values())
check("src file set matches doc directory listing",
      set(loc) == {"main.cpp", "ofApp.h", "ofApp.cpp", "Eye.h", "Eye.cpp", "Triangulation.hpp"},
      f"files: {sorted(loc)}, total LOC {total}")

# 6. class/member claims
for member in ["hostName", "sessionId", "idColor", "bgColor", "EyeBlobSequence", "EyeVideo",
               "EyeCalibration", "RayIntersector", "blobs3d"]:
    found_files = [f for f, s2 in src.items() if member in s2]
    check(f"member {member}", found_files, ",".join(found_files) or "NOT FOUND")

# 7. constants
for const in ["numSamples", "numBlobSequences"]:
    found = [f for f, s2 in src.items() if const in s2]
    check(f"constant {const}", found, ",".join(found) or "NOT FOUND")

# 8. common/ headers existence
for h in ["Pinopticon.hpp", "Pinopticon_Osc.hpp", "Pinopticon_Http.hpp"]:
    check(f"common header {h}", (common_dir / h).exists())

# 9. platform files existence
for f in ["Pinopticap.sln", "Pinopticap.xcodeproj", "Pinopticap.vcxproj", "setup.sh", "get-bin.sh"]:
    check(f"file {f}", Path(f).exists())

# 10. tools/camera_sim existence
check("tools/camera_sim exists", Path("tools/camera_sim").exists() and Path("tools/camera_sim").is_dir())

out = Path("docs/architecture/audit_report.txt")
lines = ["PINOPTICAP ARCHITECTURE.md AUDIT - deterministic checks",
         f"files: {len(src)}, total src LOC: {total}", ""]
for name, ok, detail in checks:
    status = "PASS" if ok else "FAIL"
    line = f"[{status}] {name}"
    if detail and (not ok or len(str(detail)) < 60):
        line += f"  -> {detail}"
    lines.append(line)
fails = sum(1 for _, ok, _ in checks if not ok)
lines.append(f"\n{len(checks)} checks, {fails} failures")
out.write_text("\n".join(lines))
print("\n".join(lines))
