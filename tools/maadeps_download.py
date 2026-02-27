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

basedir = Path(__file__).parent.parent
maadeps_dir = Path(basedir, "MaaDeps")
download_dir = Path(maadeps_dir, "tarball")


def detect_host_triplet():
    import platform

    machine = platform.machine().lower()
    system = platform.system().lower()
    if machine in {"amd64", "x86_64"}:
        machine = "x64"
    elif machine in {"x86", "i386", "i486", "i586", "i686"}:
        machine = "x86"
    elif machine in {"armv7l", "armv7a", "arm", "arm32"}:
        machine = "arm"
    elif machine in {"arm64", "armv8l", "aarch64"}:
        machine = "arm64"
    else:
        raise Exception("unsupported architecture: " + machine)
    if system in {"windows", "linux"}:
        pass
    elif "mingw" in system or "cygwin" in system:
        system = "windows"
    elif system == "darwin":
        system = "osx"
    else:
        raise Exception("unsupported system: " + system)
    return f"{machine}-{system}"


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
    import time

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


def main(target_triplet: str, repo: str, version: str, cache_asset: bool = False):
    print("about to download prebuilt dependency libraries for", target_triplet)
    # if len(sys.argv) == 1:
    #     print(f"to specify another triplet, run `{sys.argv[0]} <target triplet>`")
    #     print(f"e.g. `{sys.argv[0]} x64-windows`")
    req = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/releases/tags/{version}"
    )
    token = os.environ.get("GH_TOKEN", os.environ.get("GITHUB_TOKEN", None))
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    resp = retry_urlopen(req).read()
    release = json.loads(resp)

    def split_asset_name(name: str):
        *remainder, component_suffix = name.rsplit("-", 1)
        component = component_suffix.split(".", 1)[0]
        if remainder:
            _, *target = remainder[0].split("-", 1)
            if target:
                return target[0], component
        return None, None

    devel_asset = None
    runtime_asset = None
    for asset in release["assets"]:
        target, component = split_asset_name(asset["name"])
        if target == target_triplet:
            if component == "devel":
                devel_asset = asset
            elif component == "runtime":
                runtime_asset = asset
    if devel_asset and runtime_asset:
        print("found assets:")
        print("    " + devel_asset["name"])
        print("    " + runtime_asset["name"])
        download_dir.mkdir(parents=True, exist_ok=True)
        for asset in [devel_asset, runtime_asset]:
            if cache_asset and check_asset_cache(asset, maadeps_dir):
                print("using cached asset", asset["name"])
                continue
            url = asset["browser_download_url"]
            print("downloading from", url)
            local_file = download_dir / sanitize_filename(asset["name"])
            if check_local_digest(local_file, asset["digest"]):
                print("reusing matched digest", asset["digest"])
            else:
                urllib.request.urlretrieve(url, local_file, reporthook=ProgressHook())
            print("extracting", asset["name"])
            shutil.unpack_archive(local_file, maadeps_dir)
            if cache_asset:
                set_asset_cache(asset, maadeps_dir)
    else:
        raise Exception(f"no binary release found for {target_triplet}")


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
