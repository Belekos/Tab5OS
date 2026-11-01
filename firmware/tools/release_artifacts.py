"""Exclude observed host copy temporaries; never filter application filenames."""
from pathlib import PurePosixPath
import re
from release_config import SKETCH

# Only generated verification evidence and one read-only distributed index path.
# Preserve normal dotfiles (.gitignore/.gitlab-ci.yml), logs, PNGs and SD content.
_SUFFIX = r"(?:\.[A-Za-z0-9]{6})+"
_VERIFICATION = re.compile(r"\.(?:build\.log|[^/]+\.ppm)" + _SUFFIX)
_INDEX = re.compile(r"\.cities\.t5geo" + _SUFFIX)

def copy_temporary(relative):
    path = PurePosixPath(relative)
    if relative.startswith("tools/verification/"):
        return _VERIFICATION.fullmatch(path.name) is not None
    if path.parent.as_posix() == SKETCH + "/sdcard/System/maps/gazetteer":
        return _INDEX.fullmatch(path.name) is not None
    return False

# Exact copy artifacts already accidentally shipped in the authenticated .47.
# The baseline manifest remains untouched; these bytes are recorded, not erased.
HISTORICAL_COPY_ARTIFACTS = {'build-0830/.build.log.3SL9oz': {'sha256': 'f89fa627e3cb28901cda6b485a5b2631313fed09380280772aa7de5af906818a', 'bytes': 7602176}, 'build-0833/.build.log.4b9nPi': {'sha256': 'fdd1dce058ad7931d3aad767e5090027af7d33a7d7522347511b6223b47e0ffd', 'bytes': 7077888}, 'build-0833/.build.log.DeHTEJ': {'sha256': 'f99f2f82fd212b1e6a5e9f79b4a65f26989450922fb65206e7ecb0ab07461afc', 'bytes': 7864320}, 'build-0833/.build.log.DeHTEJ.6THTQG': {'sha256': '83b5ada12d54b3f0140492cdaa5b857939db8082cb9335f2560eef63675c0426', 'bytes': 7602176}, 'build-0837/.build.log.ADWnwJ': {'sha256': 'e8879a3854d4243aef9e00ee2e1869222c25e37c5fa471c1806f89e2a6577443', 'bytes': 7864320}, 'build-0837/.build.log.ADWnwJ.UaIBAZ': {'sha256': '55223a97a549f5b4a4729454dac8560529f8dfc683177e66f325655158a2e725', 'bytes': 2359296}, 'history-0821/.build.log.0R0uze': {'sha256': '98cbf671415dbfea8cb875e346d63d49717a6978b9a2a7267ae4a1a714358c52', 'bytes': 7807990}, 'history-0821/.build.log.UCJwuM': {'sha256': '13b541484ac110c70fca634896046c57876a51b565134909fc56b50e1b8d258e', 'bytes': 6029312}, 'history-0822/.build.log.BARbmJ': {'sha256': 'c76ff8076876325396582c702a278b09c796efbe50e99b27c0d982d31538da07', 'bytes': 3670016}, 'history-0827/evidence/.build.log.igc9bg': {'sha256': '9000328b102ef6c4fede840ad4676eb2fd01c0d7a561ab20ddd718ff67be1cfe', 'bytes': 4980736}, 'root-ui-0839/.usb-waiting-24.ppm.mJGdB1': {'sha256': 'cb7e4230672ef0bd8e3b5b7a074b5e5726b2e1813abd3da8e81b8a6fa0c1d508', 'bytes': 1048576}, 'settings-ui-0840/.font-picker-fr-light-24.ppm.Rpvxel': {'sha256': 'e8d26fd21fde75362cdfebaf2309049fcaeaac836d6895e9c6ac30167b138167', 'bytes': 2359296}, 'settings-ui-0840/.system-info-fr-dark-24.ppm.DBdlZ5': {'sha256': 'd5ac1d03d12529f207856e02bcc61009d2fa20853bb46ff1febc480429f2603e', 'bytes': 1835008}, 'settings-ui-0842/.font-picker-fr-dark-24.ppm.BSZ7Ng': {'sha256': 'c4d86c01037ed9daecf24e22b71449494ca2b1a0249fdb91ef1f7c7586cc18c3', 'bytes': 1835008}}
