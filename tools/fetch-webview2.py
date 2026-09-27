#!/usr/bin/env python3
"""Download the pinned official WebView2 SDK only (not a browser runtime)."""
import argparse
import hashlib
from pathlib import Path
import urllib.request
import zipfile
import io

VERSION = '1.0.4191.47'
SHA256 = 'f492bbf547d0da329553b6727435b677579b1e9f91cc9e4a1ad029366d5f23d0'
URL = f'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/{VERSION}/microsoft.web.webview2.{VERSION}.nupkg'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'build/webview2-sdk')
    args = parser.parse_args()
    with urllib.request.urlopen(URL, timeout=60) as response:
        data = response.read(64*1024*1024+1)
    if len(data)>64*1024*1024 or hashlib.sha256(data).hexdigest()!=SHA256:
        raise SystemExit('WebView2 SDK digest or size mismatch; no files extracted')
    target = args.output.resolve()
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for entry in archive.infolist():
            destination = (target / entry.filename).resolve()
            if not destination.is_relative_to(target):
                raise SystemExit('Invalid archive path')
        target.mkdir(parents=True, exist_ok=True)
        archive.extractall(target)
    print(f'Official Microsoft.Web.WebView2 {VERSION}: {target}')
    print('Copy build/native/x64/WebView2Loader.dll beside the plugin DLL. Keep the SDK license with redistributions.')

if __name__ == '__main__':
    main()
