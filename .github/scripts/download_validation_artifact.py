#!/usr/bin/env python3
"""Download one GitHub Actions artifact without forwarding credentials to storage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import urllib.error
import urllib.parse
import urllib.request

REPO = "Belekos/Tab5OS"
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifact-id", type=int, required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    expected = args.sha256.removeprefix("sha256:")
    if len(expected) != 64 or any(c not in "0123456789abcdef" for c in expected):
        raise ValueError("A complete SHA-256 is required")
    url = f"https://api.github.com/repos/{REPO}/actions/artifacts/{args.artifact_id}/zip"
    req = urllib.request.Request(url, headers={
        "Authorization": "Bearer " + os.environ["GH_TOKEN"],
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "Tab5OS-artifact-validation",
    })
    opener = urllib.request.build_opener(NoRedirect)
    try:
        response = opener.open(req, timeout=90)
    except urllib.error.HTTPError as error:
        if error.code not in (301, 302, 303, 307, 308):
            raise RuntimeError(f"Artifact download failed: HTTP {error.code}") from None
        location = error.headers.get("Location", "")
        target = urllib.parse.urlparse(location)
        if target.scheme != "https" or not target.hostname or not (
            target.hostname.endswith(".blob.core.windows.net") or
            target.hostname.endswith(".githubusercontent.com")
        ):
            raise RuntimeError("Unexpected artifact storage destination")
        # A fresh request deliberately omits the GitHub Authorization header.
        response = urllib.request.urlopen(urllib.request.Request(location), timeout=90)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    size = 0
    with response, args.output.open("wb") as output:
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            output.write(block)
            digest.update(block)
            size += len(block)
            if size > 512 * 1024 * 1024:
                raise RuntimeError("Artifact exceeds the expected size limit")
    if digest.hexdigest() != expected:
        raise RuntimeError("Artifact SHA-256 mismatch")
    print(json.dumps({"artifactId": args.artifact_id, "bytes": size, "sha256": expected}))
if __name__ == "__main__":
    main()
