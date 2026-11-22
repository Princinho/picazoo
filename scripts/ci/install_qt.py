#!/usr/bin/env python3
"""Installe un kit Qt 6 depuis le dépôt officiel download.qt.io (CI Windows).

Depuis Qt 6.12, le dépôt « qtsdkrepository » range chaque kit dans son propre
dossier (qt6_6121/qt6_6121_msvc2022_64/), format qu'aqtinstall ne lit pas encore.
Ce script lit Updates.xml, télécharge les archives du kit et des modules demandés,
vérifie leur empreinte SHA-1 publiée, puis les extrait avec 7-Zip.

Bibliothèque standard uniquement. Exemple :
    python install_qt.py --version 6.12.1 --arch win64_msvc2022_64 \
        --modules qtmultimedia qtimageformats qtshadertools --out C:\\Qt
Le kit est alors dans C:\\Qt\\6.12.1\\msvc2022_64.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop"


def fetch(url: str, attempts: int = 4) -> bytes:
    for i in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=120) as r:
                return r.read()
        except Exception as e:  # réseau capricieux des miroirs : on réessaie
            if i == attempts - 1:
                raise RuntimeError(f"Téléchargement impossible : {url} ({e})") from e
            time.sleep(5 * (i + 1))
    raise AssertionError


def download(url: str, dest: Path, attempts: int = 4) -> None:
    for i in range(attempts):
        try:
            with urllib.request.urlopen(url, timeout=300) as r, open(dest, "wb") as f:
                shutil.copyfileobj(r, f, 1 << 20)
            return
        except Exception as e:
            if i == attempts - 1:
                raise RuntimeError(f"Téléchargement impossible : {url} ({e})") from e
            time.sleep(5 * (i + 1))


def sha1(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def extract(archive: Path, out: Path, sevenzip: str) -> None:
    tool = shutil.which(sevenzip)
    if tool:
        subprocess.run([tool, "x", "-y", "-bso0", "-bsp0", f"-o{out}", str(archive)], check=True)
    else:
        # tar.exe de Windows (libarchive) sait aussi lire le format 7z.
        subprocess.run(["tar", "-xf", str(archive), "-C", str(out)], check=True)


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--version", required=True, help="ex. 6.12.1")
    ap.add_argument("--arch", default="win64_msvc2022_64")
    ap.add_argument("--modules", nargs="*", default=[])
    ap.add_argument("--out", required=True, help="dossier racine, ex. C:\\Qt")
    ap.add_argument("--sevenzip", default="7z")
    args = ap.parse_args()

    v = args.version.replace(".", "")
    kit = args.arch.removeprefix("win64_")
    base = f"{REPO}/qt6_{v}/qt6_{v}_{kit}"
    updates = ET.fromstring(fetch(f"{base}/Updates.xml"))
    packages = {p.findtext("Name"): p for p in updates.findall("PackageUpdate")}

    wanted = [f"qt.qt6.{v}.{args.arch}"] + [f"qt.qt6.{v}.addons.{m}.{args.arch}" for m in args.modules]
    missing = [w for w in wanted if w not in packages]
    if missing:
        print("Paquets introuvables :", ", ".join(missing), file=sys.stderr)
        print("Modules disponibles :", file=sys.stderr)
        for name in sorted(packages):
            if name.startswith(f"qt.qt6.{v}.addons.") and name.endswith(args.arch) and "debug" not in name:
                print("  ", name.split(".")[4], file=sys.stderr)
        return 1

    # Les archives de ce format se décompressent à la racine du kit.
    prefix = Path(args.out) / args.version / kit
    (prefix / "bin").mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for name in wanted:
            p = packages[name]
            version = p.findtext("Version")
            archives = [a.strip() for a in (p.findtext("DownloadableArchives") or "").split(",") if a.strip()]
            for archive in archives:
                # Documentation et traductions : inutiles pour compiler.
                if archive.startswith(("qtdoc-", "qttranslations-")):
                    continue
                url = f"{base}/{name}/{version}{archive}"
                dest = Path(tmp) / archive
                print(f"↓ {archive}", flush=True)
                download(url, dest)
                expected = fetch(url + ".sha1").decode().split()[0].lower()
                actual = sha1(dest)
                if actual != expected:
                    raise RuntimeError(f"Empreinte SHA-1 incorrecte pour {archive} : {actual} ≠ {expected}")
                # opengl32sw (rendu logiciel de secours) va dans bin/, comme avec l'installateur officiel.
                extract(dest, prefix / "bin" if archive.startswith("opengl32sw") else prefix, args.sevenzip)
                dest.unlink()

    # Comme l'installateur officiel : les outils (windeployqt, qtpaths…) retrouvent le kit.
    (prefix / "bin" / "qt.conf").write_text("[Paths]\nPrefix=..\n", encoding="utf-8")
    if not (prefix / "bin" / "Qt6Core.dll").exists():
        print(f"Installation incomplète : {prefix}\\bin\\Qt6Core.dll absent", file=sys.stderr)
        return 1
    print(f"Qt {args.version} installé dans {prefix}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
