#!/usr/bin/env python3
"""Package a complete Windows portable ZIP as a full-trust MSIX.

Usage: python scripts/build-windows-msix.py ARCHIVE VERSION ARCH OUTPUT
       [--package-name NAME --publisher SUBJECT] [--stage-only]

The default identity is for development only. A Store submission must pass the
exact package name and publisher subject reserved in Partner Center. This script
does not sign packages; Store distribution uses Microsoft's package signature.
"""

import argparse
import os
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


PACKAGE_NS = "http://schemas.microsoft.com/appx/manifest/foundation/windows10"
ET.register_namespace("", PACKAGE_NS)
ET.register_namespace("uap", "http://schemas.microsoft.com/appx/manifest/uap/windows10")
ET.register_namespace("uap10", "http://schemas.microsoft.com/appx/manifest/uap/windows10/10")
ET.register_namespace("rescap", "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities")


def msix_version(value: str) -> str:
    parts = value.removeprefix("v").split(".")
    if len(parts) not in (3, 4) or not all(part.isdecimal() for part in parts):
        raise ValueError(f"Invalid release version: {value}")
    numbers = [int(part) for part in parts]
    major, minor, patch = numbers[:3]
    iteration = numbers[3] if len(numbers) == 4 else 0
    combined_patch = patch * 1000 + iteration
    if major == 0 or major > 65535 or minor > 65535 or iteration > 999 or combined_patch > 65535:
        raise ValueError(f"Version is outside Microsoft Store MSIX limits: {value}")
    # The Store reserves the fourth field as zero. Fold AstraPlay's release
    # iteration into the third field so each GitHub release still upgrades.
    return f"{major}.{minor}.{combined_patch}.0"


def find_makeappx() -> str:
    configured = os.environ.get("MAKEAPPX_PATH")
    if configured:
        return configured
    located = shutil.which("makeappx.exe") or shutil.which("makeappx")
    if located:
        return located
    program_files = os.environ.get("ProgramFiles(x86)")
    if program_files:
        sdk_bin = Path(program_files) / "Windows Kits" / "10" / "bin"
        candidates = sorted(sdk_bin.glob("*/x64/makeappx.exe"), reverse=True)
        if candidates:
            return str(candidates[0])
    raise FileNotFoundError("makeappx.exe was not found in the Windows SDK")


def stage_package(archive: Path, stage: Path, version: str, arch: str, name: str, publisher: str) -> int:
    template = Path(__file__).resolve().parent.parent / "packaging" / "windows-msix"
    with zipfile.ZipFile(archive) as source:
        members = [member for member in source.infolist() if not member.is_dir()]
        if not members:
            raise ValueError("Portable archive is empty")
        for member in members:
            parts = Path(member.filename.replace("\\", "/")).parts
            if len(parts) < 2 or parts[0] != "astraplay" or ".." in parts or Path(member.filename).is_absolute():
                raise ValueError(f"Unexpected or unsafe archive path: {member.filename}")
            relative = Path(*parts[1:])
            destination = stage / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            with source.open(member) as input_file, destination.open("wb") as output_file:
                shutil.copyfileobj(input_file, output_file)

    if not (stage / "astraplay.exe").is_file() or len(members) < 2:
        raise ValueError("Portable archive lacks the application or bundled runtime")
    shutil.copytree(template / "Assets", stage / "Assets")

    tree = ET.parse(template / "AppxManifest.xml")
    identity = tree.find(f"{{{PACKAGE_NS}}}Identity")
    if identity is None:
        raise ValueError("MSIX template has no package identity")
    identity.set("Name", name)
    identity.set("Publisher", publisher)
    identity.set("Version", version)
    identity.set("ProcessorArchitecture", arch)
    tree.write(stage / "AppxManifest.xml", encoding="utf-8", xml_declaration=True)
    return len(members)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="Windows portable ZIP with astraplay/ at its root")
    parser.add_argument("release_version", help="Release version, for example v2.11.4.15")
    parser.add_argument("architecture", choices=("x64", "arm64"))
    parser.add_argument("output", type=Path, help="Output MSIX, or staging directory with --stage-only")
    parser.add_argument("--package-name", default="AstraPlay.Development")
    parser.add_argument("--publisher", default="CN=AstraPlay Development")
    parser.add_argument("--stage-only", action="store_true", help="Create an unpacked package for inspection")
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.\-]{0,49}", args.package_name):
        parser.error("--package-name must be a valid MSIX identity name")
    if not args.publisher.strip():
        parser.error("--publisher must be a non-empty certificate subject")
    version = msix_version(args.release_version)

    with tempfile.TemporaryDirectory(prefix="astraplay-msix-") as temp_dir:
        stage = Path(temp_dir) / "package"
        stage.mkdir()
        count = stage_package(args.archive, stage, version, args.architecture, args.package_name, args.publisher)
        output = args.output.resolve()
        if args.stage_only:
            if output.exists():
                raise FileExistsError(f"Staging directory already exists: {output}")
            shutil.copytree(stage, output)
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run([find_makeappx(), "pack", "/d", str(stage), "/p", str(output), "/o"], check=True)
            if not output.is_file() or output.stat().st_size == 0:
                raise RuntimeError("makeappx did not produce an MSIX")
        print(f"Built {output} from {count} runtime files; MSIX version {version}, architecture {args.architecture}")


if __name__ == "__main__":
    main()
