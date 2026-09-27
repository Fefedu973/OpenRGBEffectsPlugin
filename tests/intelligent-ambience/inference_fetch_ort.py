"""Fetch the pinned Microsoft CPU runtime; never downloads trained models.

Only this explicit build/test tool accesses the network. Production never does.
"""
import argparse
import hashlib
from pathlib import Path
import urllib.request
import zipfile

VERSION = "1.30.0"
ARCHIVE = f"onnxruntime-win-x64-{VERSION}.zip"
URL = f"https://github.com/microsoft/onnxruntime/releases/download/v{VERSION}/{ARCHIVE}"
SHA256 = "c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949"
SIZE = 82645522

def fetch(output):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / ARCHIVE
    if not archive.exists():
        temporary = archive.with_suffix(".download")
        with urllib.request.urlopen(URL, timeout=60) as response, temporary.open("wb") as target:
            count = 0
            while block := response.read(1024 * 1024):
                count += len(block)
                if count > SIZE:
                    raise ValueError("Runtime archive exceeds pinned size")
                target.write(block)
        temporary.replace(archive)
    if archive.stat().st_size != SIZE or hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise ValueError("Official runtime archive hash/size mismatch")
    with zipfile.ZipFile(archive) as package:
        for entry in package.infolist():
            target = (output / entry.filename).resolve()
            if not target.is_relative_to(output) or entry.file_size > 512 * 1024 * 1024:
                raise ValueError("Unsafe archive entry")
        package.extractall(output)
    return output / f"onnxruntime-win-x64-{VERSION}"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[2] / "build" / "onnxruntime")
    print(fetch(parser.parse_args().output))
