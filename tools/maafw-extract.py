#!/usr/bin/env python3
import sys
from pathlib import Path
import shutil

basedir = Path(__file__).parent.parent.parent.parent  # repository root
target_dir = Path(basedir, "build", "bin", "Release")
archive_dir = Path(basedir, "build", "MaaFramework")


def detect_host_platform():
    import platform

    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "windows":
        if machine in {"amd64", "x86_64"}:
            return "win-x86_64"
        if machine in {"arm64", "aarch64"}:
            return "win-aarch64"
    elif system == "linux":
        if machine in {"amd64", "x86_64"}:
            return "linux-x86_64"
        if machine in {"arm64", "aarch64"}:
            return "linux-aarch64"
    elif system == "darwin":
        if machine in {"amd64", "x86_64"}:
            return "macos-x86_64"
        if machine in {"arm64", "aarch64"}:
            return "macos-aarch64"
    raise Exception(f"unsupported platform: {system}-{machine}")


def main():
    if len(sys.argv) == 2:
        platform = sys.argv[1]
    else:
        platform = detect_host_platform()

    archives = sorted(archive_dir.glob(f"MAA-{platform}-v*.zip"))
    if not archives:
        print(
            f"""Please download the {platform} package from https://github.com/MaaXYZ/MaaFramework/releases/latest, and put it into {archive_dir}"""
        )
        archive_dir.mkdir(parents=True, exist_ok=True)
        return

    # pick the archive the user most recently put into archive_dir
    archive = max(archives, key=lambda p: p.stat().st_mtime)

    print("extracting", archive)
    extract_dir = archive_dir / Path(archive.name).stem
    if extract_dir.exists():
        shutil.rmtree(extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=True)
    shutil.unpack_archive(archive, extract_dir)

    bin_dir = extract_dir / "bin"
    if not bin_dir.is_dir():
        raise Exception(f"no `bin` directory found in {archive}")

    target_dir.mkdir(parents=True, exist_ok=True)
    for src in sorted(bin_dir.glob("*ControlUnit*")):
        dst = target_dir / src.name
        shutil.copy2(src, dst)
        print(f"installed {src.name} -> {dst}")


if __name__ == "__main__":
    main()
