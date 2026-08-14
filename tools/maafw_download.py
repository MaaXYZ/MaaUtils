#!/usr/bin/env python3
import hashlib
import json
import os
import shutil
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

basedir = Path(__file__).parent.parent.parent.parent
target_dir = Path(basedir, "build", "bin", "Release")
download_dir = Path(basedir, "build", "MaaFramework")


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


def format_size(num, suffix="B"):
    for unit in ["", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi"]:
        if abs(num) < 1024.0:
            return f"{num:3.1f}{unit}{suffix}"
        num /= 1024.0
    return f"{num:.1f}Yi{suffix}"


class ProgressHook:
    def __init__(self):
        self.downloaded = 0
        self.last_print = 0

    def __call__(self, block, chunk, total):
        self.downloaded += chunk
        t = time.monotonic()
        if t - self.last_print >= 0.5 or self.downloaded == total:
            self.last_print = t
            if total > 0:
                print(
                    f"\r [{self.downloaded / total * 100.0:3.1f}%] {format_size(self.downloaded)} / {format_size(total)}      \r",
                    end="",
                )
        if self.downloaded == total:
            print("")


def sanitize_filename(filename: str):
    import platform

    system = platform.system()
    if system == "Windows":
        filename = filename.translate(str.maketrans('/\\:"?*|\0', "________")).rstrip(
            "."
        )
    elif system == "Darwin":
        filename = filename.translate(str.maketrans("/:\0", "___"))
    else:
        filename = filename.translate(str.maketrans("/\0", "__"))
    return filename


def retry_urlopen(*args, **kwargs):
    import http.client

    for _ in range(5):
        try:
            resp: http.client.HTTPResponse = urllib.request.urlopen(*args, **kwargs)
            return resp
        except urllib.error.HTTPError as e:
            if e.status == 403 and e.headers.get("x-ratelimit-remaining") == "0":
                # rate limit
                t0 = time.time()
                reset_time = t0 + 10
                try:
                    reset_time = int(e.headers.get("x-ratelimit-reset", 0))
                except ValueError:
                    pass
                reset_time = max(reset_time, t0 + 10)
                print(
                    f"rate limit exceeded, retrying after {reset_time - t0:.1f} seconds"
                )
                time.sleep(reset_time - t0)
                continue
            raise


def main(platform: str, repo: str, version: str, cache_asset: bool = False):
    print("about to download MaaFramework control units for", platform)
    if version:
        req = urllib.request.Request(
            f"https://api.github.com/repos/{repo}/releases/tags/{version}"
        )
    else:
        req = urllib.request.Request(
            f"https://api.github.com/repos/{repo}/releases/latest"
        )
    token = os.environ.get("GH_TOKEN", os.environ.get("GITHUB_TOKEN", None))
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    resp = retry_urlopen(req).read()
    release = json.loads(resp)

    asset = None
    for candidate in release["assets"]:
        name = candidate["name"]
        if not name.startswith(f"MAA-{platform}-"):
            continue
        if name.endswith(".zip"):
            asset = candidate
            break
    if asset is None:
        raise Exception(
            f"no MAA-{platform}-* archive found in release {release['tag_name']}"
        )

    if cache_asset and check_asset_cache(asset, download_dir):
        print("using cached asset", asset["name"])
        return
    url = asset["browser_download_url"]
    print("downloading from", url)
    download_dir.mkdir(parents=True, exist_ok=True)
    local_file = download_dir / sanitize_filename(asset["name"])
    if check_local_digest(local_file, asset["digest"]):
        print("reusing matched digest", asset["digest"])
    else:
        urllib.request.urlretrieve(url, local_file, reporthook=ProgressHook())
    print("extracting", asset["name"])
    extract_dir = download_dir / sanitize_filename(Path(asset["name"]).stem)
    if extract_dir.exists():
        shutil.rmtree(extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=True)
    shutil.unpack_archive(local_file, extract_dir)
    bin_dir = extract_dir / "bin"
    if not bin_dir.is_dir():
        raise Exception(f"no `bin` directory found in archive {asset['name']}")
    target_dir.mkdir(parents=True, exist_ok=True)
    for src in sorted(bin_dir.glob("*ControlUnit*")):
        dst = target_dir / src.name
        shutil.copy2(src, dst)
        print(f"installed {src.name} -> {dst}")
    if cache_asset:
        set_asset_cache(asset, download_dir)


def check_local_digest(file: Path, digest: str):
    if not file.exists():
        return False
    if not digest.startswith("sha256:"):
        print("unsupported digest format:", digest)
        return False
    hasher = hashlib.sha256()
    with file.open("rb") as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            hasher.update(chunk)
    local_digest = hasher.hexdigest()
    return local_digest == digest[len("sha256:") :]


def check_asset_cache(asset, extract_dir: Path):
    name = asset["name"]
    digest = asset["digest"]
    asset_cache_file = extract_dir / ".cache_digest.json"
    if not asset_cache_file.exists():
        return False
    try:
        with open(asset_cache_file, "r") as f:
            asset_caches = json.load(f)
            return asset_caches.get(name) == digest
    except (json.JSONDecodeError, AttributeError, KeyError):
        return False


def set_asset_cache(asset, extract_dir: Path):
    name = asset["name"]
    digest = asset["digest"]
    asset_cache_file = extract_dir / ".cache_digest.json"
    try:
        with open(asset_cache_file, "r") as f:
            asset_caches = json.load(f)
            asset_caches[name] = digest
    except (FileNotFoundError, json.JSONDecodeError, TypeError):
        asset_caches = {name: digest}
    with open(asset_cache_file, "w") as f:
        json.dump(asset_caches, f)
