# SPDX-License-Identifier: GPL-3.0-only
"""Project-local build helpers. No package installation or OS configuration."""
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / ".cache" / "ps5"


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def fetch(url, path, expected=None, size=None):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if (size is None or path.stat().st_size == size) and (expected is None or sha256(path) == expected):
            return path
        raise RuntimeError(f"Cached download fails its pin; inspect it before removing: {path}")
    if not url.startswith("https://"):
        raise ValueError("Only HTTPS downloads are accepted")
    print(f"Download {path.name}", flush=True)
    temp = path.with_name(path.name + ".download")
    request = urllib.request.Request(url, headers={"User-Agent": "Console-Shelf-build/0.1"})
    with urllib.request.urlopen(request, timeout=90) as response, open(temp, "wb") as out:
        if not response.url.startswith("https://"):
            raise RuntimeError("Download redirected away from HTTPS")
        shutil.copyfileobj(response, out, 1024 * 1024)
    if size is not None and temp.stat().st_size != size:
        raise RuntimeError(f"Unexpected byte count: {path.name}")
    if expected is not None and sha256(temp) != expected:
        raise RuntimeError(f"SHA-256 mismatch: {path.name}")
    os.replace(temp, path)
    return path


def member_path(name):
    p = PurePosixPath(name)
    if p.is_absolute() or ".." in p.parts or "\\" in name or ":" in name:
        raise ValueError(f"Unsafe archive path: {name}")
    return p


def unpack(archive, target, prefix=""):
    """Hash-pinned archives only; reject escapes and special files before writing."""
    target = Path(target)
    if target.exists():
        raise RuntimeError(f"Extraction destination already exists: {target}")
    target.mkdir(parents=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as z:
            for item in z.infolist():
                member_path(item.filename)
                if (item.external_attr >> 16) & 0o170000 == 0o120000:
                    raise ValueError("ZIP symlinks are not supported")
            bad = z.testzip()
            if bad:
                raise RuntimeError(f"ZIP integrity error: {bad}")
            for item in z.infolist():
                if not item.filename.startswith(prefix):
                    continue
                name = item.filename[len(prefix):]
                if not name:
                    continue
                dest = target / name
                if item.is_dir():
                    dest.mkdir(parents=True, exist_ok=True)
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    with z.open(item) as source, open(dest, "wb") as out:
                        shutil.copyfileobj(source, out)
                    if item.external_attr >> 16:
                        dest.chmod((item.external_attr >> 16) & 0o777)
    else:
        with tarfile.open(archive) as t:
            selected = []
            for item in t:
                member_path(item.name)
                if not item.name.startswith(prefix):
                    continue
                name = item.name[len(prefix):].lstrip("/")
                if not name:
                    continue
                dest = target / name
                if item.issym() or item.islnk():
                    link = PurePosixPath(item.linkname)
                    if item.islnk() and not item.linkname.startswith(prefix):
                        raise ValueError(f"Hard link leaves selected archive prefix: {item.name}")
                    resolved = ((dest.parent / str(link)) if item.issym() else
                                target / item.linkname[len(prefix):]).resolve()
                    if link.is_absolute() or not resolved.is_relative_to(target.resolve()):
                        raise ValueError(f"Archive link escapes selected root: {item.name}")
                elif not item.isfile() and not item.isdir():
                    raise ValueError(f"Special archive member: {item.name}")
                selected.append((item, dest))
            for item, dest in selected:
                dest.parent.mkdir(parents=True, exist_ok=True)
                if item.isdir():
                    dest.mkdir(exist_ok=True)
                elif item.isfile():
                    with t.extractfile(item) as source, open(dest, "wb") as out:
                        shutil.copyfileobj(source, out)
                    dest.chmod(item.mode & 0o777)
            # Links come after regular files so a member cannot redirect extraction.
            for item, dest in selected:
                if item.issym():
                    dest.symlink_to(item.linkname)
                elif item.islnk():
                    source = target / item.linkname[len(prefix):]
                    if not source.is_file() or source.is_symlink():
                        raise ValueError(f"Invalid hard link target: {item.linkname}")
                    os.link(source, dest)


def prepare_archive(record, name, target, prefix=""):
    archive = fetch(record["url"], CACHE / "downloads" / name, record["sha256"], record.get("bytes"))
    stamp = Path(target) / ".shelf-dependency-sha256"
    if stamp.exists() and stamp.read_text().strip() == record["sha256"]:
        return Path(target)
    unpack(archive, target, prefix)
    stamp.write_text(record["sha256"] + "\n")
    return Path(target)
