#!/usr/bin/env python3
import json
import os
import sys

def main():
    if len(sys.argv) < 5:
        print("Usage: generate_version_json.py <dist_dir> <tag> <repo> <output_json>")
        sys.exit(1)

    dist_dir = sys.argv[1]
    tag = sys.argv[2]
    repo = sys.argv[3]
    output_path = sys.argv[4]

    assets = []
    if os.path.exists(dist_dir):
        for name in sorted(os.listdir(dist_dir)):
            if name.startswith(".") or name == "version.json" or name == "SHA256SUMS.txt":
                continue
            file_path = os.path.join(dist_dir, name)
            if os.path.isfile(file_path):
                assets.append({
                    "name": name,
                    "browser_download_url": f"https://github.com/{repo}/releases/download/{tag}/{name}"
                })

    data = {
        "tag_name": tag,
        "html_url": f"https://github.com/{repo}/releases/tag/{tag}",
        "body": f"AstraPlay {tag} release",
        "assets": assets
    }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.write("\n")

if __name__ == "__main__":
    main()
