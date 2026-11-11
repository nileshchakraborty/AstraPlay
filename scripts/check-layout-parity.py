#!/usr/bin/env python3
# =============================================================================
# AstraPlay Android Layout Parity & Null-Safety Linter
#
# Audits all layout qualifiers (layout, layout-land, layout-sw600dp,
# layout-sw600dp-land, etc.) for view ID parity and detects unsafe
# non-null `lateinit var` view bindings in Kotlin source files.
#
# Usage:
#   python3 scripts/check-layout-parity.py
# =============================================================================

import os
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
RES_DIR = REPO_ROOT / "android" / "app" / "src" / "main" / "res"
JAVA_DIR = REPO_ROOT / "android" / "app" / "src" / "main" / "java"

ID_REGEX = re.compile(r'android:id="@\+?id/([^"]+)"')
LATEINIT_VIEW_REGEX = re.compile(
    r'private\s+lateinit\s+var\s+(\w+)\s*:\s*([A-Z]\w*(?:View|Layout|Button|Text|Bar|Image|Card|Toolbar|EditText|Spinner))'
)
FIND_VIEW_REGEX = re.compile(r'(\w+)\s*=\s*(?:[^\n]+\.)?findViewById\s*(?:<[^>]+>)?\s*\(\s*R\.id\.(\w+)\s*\)')


def audit_layout_id_parity():
    """Checks that all qualifiers for a layout XML define an identical set of IDs."""
    layout_dirs = [d for d in RES_DIR.iterdir() if d.is_dir() and d.name.startswith("layout")]
    if not layout_dirs:
        print("[SKIP] No layout directories found.")
        return 0

    layouts_by_name = defaultdict(dict)  # filename -> {qualifier: set(ids)}

    for ldir in layout_dirs:
        qualifier = ldir.name
        for xml_file in ldir.glob("*.xml"):
            content = xml_file.read_text(encoding="utf-8")
            ids = set(ID_REGEX.findall(content))
            layouts_by_name[xml_file.name][qualifier] = ids

    errors = 0
    multi_qualifier_count = 0

    print("==================================================================")
    print(" 1. AUDITING ANDROID LAYOUT QUALIFIER ID PARITY")
    print("==================================================================")

    for layout_name, qualifiers in sorted(layouts_by_name.items()):
        if len(qualifiers) <= 1:
            continue

        multi_qualifier_count += 1
        base_qualifier = "layout" if "layout" in qualifiers else sorted(qualifiers.keys())[0]
        base_ids = qualifiers[base_qualifier]

        # Union of all IDs across all qualifiers for this layout
        all_ids = set()
        for q_ids in qualifiers.values():
            all_ids.update(q_ids)

        mismatches = False
        for qual, q_ids in qualifiers.items():
            missing = all_ids - q_ids
            if missing:
                if not mismatches:
                    print(f"\n[FAIL] Layout ID discrepancy in '{layout_name}':")
                    mismatches = True
                print(f"  - In qualifier '{qual}', MISSING {len(missing)} ID(s):")
                for mid in sorted(missing):
                    print(f"      * R.id.{mid}")
                errors += 1

        if not mismatches:
            print(f"  [PASS] {layout_name:30s} (Qualifiers: {', '.join(sorted(qualifiers.keys()))}) -> {len(base_ids)} IDs matching.")

    print(f"\nTotal multi-qualifier layouts checked: {multi_qualifier_count}")
    return errors


def audit_kotlin_unsafe_view_bindings():
    """Detects unsafe non-null `lateinit var` view declarations bound via `findViewById`."""
    print("\n==================================================================")
    print(" 2. AUDITING KOTLIN ACTIVITIES & FRAGMENTS FOR UNSAFE VIEW BINDINGS")
    print("==================================================================")

    errors = 0
    kt_files = list(JAVA_DIR.rglob("*.kt"))

    for kt_file in kt_files:
        content = kt_file.read_text(encoding="utf-8")
        lateinits = dict(LATEINIT_VIEW_REGEX.findall(content))
        find_views = dict(FIND_VIEW_REGEX.findall(content))

        # Check for intersection: declared as non-null lateinit var AND assigned via findViewById
        unsafe = set(lateinits.keys()).intersection(find_views.keys())
        # Filter out ViewBinding properties (like `binding: ActivityMainBinding`)
        unsafe = {var: lateinits[var] for var in unsafe if not var.endswith("Binding") and not lateinits[var].endswith("Binding")}

        if unsafe:
            rel_path = kt_file.relative_to(REPO_ROOT)
            print(f"\n[FAIL] Unsafe non-null `lateinit var` view bindings in {rel_path}:")
            for var_name, var_type in sorted(unsafe.items()):
                print(f"  - `lateinit var {var_name}: {var_type}` assigned from `findViewById(R.id.{find_views[var_name]})`")
                print("    Fix: Use nullable `View? = null` with safe-calls (`?.`) or use Android ViewBinding.")
                errors += 1

    if errors == 0:
        print("  [PASS] All Kotlin activities and fragments use safe ViewBinding or nullable view references.")

    return errors


def main():
    print(f"Starting AstraPlay Layout Parity & Null-Safety Audit in: {RES_DIR}\n")
    parity_errors = audit_layout_id_parity()
    binding_errors = audit_kotlin_unsafe_view_bindings()

    total_errors = parity_errors + binding_errors
    print("\n==================================================================")
    if total_errors == 0:
        print(" AUDIT RESULT: ALL CHECKS PASSED (0 ERRORS)")
        print("==================================================================")
        return 0
    else:
        print(f" AUDIT RESULT: FAILED WITH {total_errors} ERROR(S)")
        print("==================================================================")
        return 1


if __name__ == "__main__":
    sys.exit(main())
