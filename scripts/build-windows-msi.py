#!/usr/bin/env python3
"""Build an x64 MSI from the complete Windows portable release archive."""

import argparse
import hashlib
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


WIX_NS = "http://schemas.microsoft.com/wix/2006/wi"
ET.register_namespace("", WIX_NS)
COMPONENT_NAMESPACE = uuid.UUID("B4F8E9C2-1A5D-4E7B-9C3F-8D2A1B4E6C9D")


def wix_version(release_version: str) -> str:
    parts = release_version.removeprefix("v").split(".")
    if len(parts) not in (3, 4) or not all(part.isdecimal() for part in parts):
        raise ValueError(f"Invalid release version: {release_version}")
    major, minor, patch = map(int, parts[:3])
    build = int(parts[3]) if len(parts) == 4 else 0
    if build > 999 or patch * 1000 + build > 65535 or major > 255 or minor > 255:
        raise ValueError(f"Version is outside MSI limits: {release_version}")
    return f"{major}.{minor}.{patch * 1000 + build}"


def wix_id(prefix: str, relative_path: Path) -> str:
    digest = hashlib.sha256(relative_path.as_posix().encode()).hexdigest()[:20]
    return f"{prefix}_{digest}"


def build_descriptor(template: Path, runtime_root: Path, version: str, output: Path) -> int:
    tree = ET.parse(template)
    ns = {"w": WIX_NS}
    product = tree.find("w:Product", ns)
    install_folder = tree.find(".//w:Directory[@Id='INSTALLFOLDER']", ns)
    feature = tree.find(".//w:Feature[@Id='MainFeature']", ns)
    if product is None or install_folder is None or feature is None:
        raise ValueError("MSI template is missing its product, install folder, or feature")
    product.set("Version", version)

    for component in list(install_folder.findall("w:Component", ns)):
        install_folder.remove(component)
    for component_ref in list(feature.findall("w:ComponentRef", ns)):
        if component_ref.get("Id", "").startswith("Component_"):
            feature.remove(component_ref)

    folders = {Path("."): install_folder}
    files = sorted(path for path in runtime_root.rglob("*") if path.is_file())
    if not (runtime_root / "astraplay.exe").is_file():
        raise ValueError("Portable archive has no astraplay.exe")
    for path in files:
        relative = path.relative_to(runtime_root)
        parent = relative.parent
        if parent not in folders:
            current = Path(".")
            for part in parent.parts:
                child = current / part
                if child not in folders:
                    folders[child] = ET.SubElement(
                        folders[current], f"{{{WIX_NS}}}Directory",
                        {"Id": wix_id("Dir", child), "Name": part},
                    )
                current = child

        component_id = wix_id("Component", relative)
        component = ET.SubElement(
            folders[parent], f"{{{WIX_NS}}}Component",
            {"Id": component_id, "Guid": str(uuid.uuid5(COMPONENT_NAMESPACE, relative.as_posix())), "Win64": "yes"},
        )
        ET.SubElement(
            component, f"{{{WIX_NS}}}File",
            {"Id": "AstraPlayEXE" if relative == Path("astraplay.exe") else wix_id("File", relative),
             "Source": str(path.resolve()), "KeyPath": "yes"},
        )
        ET.SubElement(feature, f"{{{WIX_NS}}}ComponentRef", {"Id": component_id})

    tree.write(output, encoding="utf-8", xml_declaration=True)
    return len(files)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="Windows x64 portable zip")
    parser.add_argument("release_version", help="Full release version, for example 2.11.4.15")
    parser.add_argument("output", type=Path, help="Output MSI path")
    args = parser.parse_args()
    template = Path(__file__).with_name("astraplay.wxs")

    with tempfile.TemporaryDirectory(prefix="astraplay-msi-") as temp_dir:
        temp = Path(temp_dir)
        with zipfile.ZipFile(args.archive) as archive:
            for member in archive.infolist():
                relative = Path(member.filename)
                if relative.is_absolute() or ".." in relative.parts:
                    raise ValueError(f"Unsafe archive path: {member.filename}")
            archive.extractall(temp)
        runtime_root = temp / "astraplay"
        descriptor = temp / "astraplay-generated.wxs"
        file_count = build_descriptor(template, runtime_root, wix_version(args.release_version), descriptor)
        if file_count < 2:
            raise ValueError("Portable archive has no bundled runtime files")
        subprocess.run(
            ["wixl", "--arch", "x64", "-o", str(args.output.resolve()), str(descriptor)],
            check=True,
        )
        if not args.output.is_file() or args.output.stat().st_size == 0:
            raise RuntimeError("wixl did not produce an MSI")
        # Apply shortcut conditions directly to Component table for compatibility across all wixl versions
        subprocess.run(
            ["msibuild", str(args.output.resolve()), "-q",
             "UPDATE Component SET Condition='CREATE_START_MENU_SHORTCUT=\"1\"' WHERE Component='StartMenuShortcutComponent'"],
            check=True,
        )
        subprocess.run(
            ["msibuild", str(args.output.resolve()), "-q",
             "UPDATE Component SET Condition='CREATE_DESKTOP_SHORTCUT=\"1\"' WHERE Component='DesktopShortcutComponent'"],
            check=True,
        )
        # wixl does not honor Property Secure="yes". Preserve the upgrade
        # properties it emits and pass shortcut choices to the elevated engine.
        subprocess.run(
            ["msibuild", str(args.output.resolve()), "-q",
             "UPDATE Property SET Value='CREATE_START_MENU_SHORTCUT;CREATE_DESKTOP_SHORTCUT;WIX_DOWNGRADE_DETECTED;WIX_UPGRADE_DETECTED' WHERE Property='SecureCustomProperties'"],
            check=True,
        )
        print(f"Built {args.output} with {file_count} files (MSI version {wix_version(args.release_version)})")


if __name__ == "__main__":
    main()
