#!/usr/bin/env python3
import argparse, subprocess, sys
from pathlib import Path


def run(cmd, cwd=None):
    print(f"$ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=cwd, check=True)


def has_pio():
    try:
        subprocess.run(["pio", "--version"], check=True, capture_output=True)
        return True
    except Exception:
        return False


def ensure_pio():
    if has_pio():
        print("✅ PlatformIO detected")
        return
    print("⚙️  Installing PlatformIO Core ...")
    run([sys.executable, "-m", "pip", "install", "-U", "--user", "platformio"])


def process_project(path: Path, force_init: bool):
    print(f"\n📁 Project: {path}")
    if not path.exists():
        raise SystemExit(f"❌ Not found: {path}")

    ini = path / "platformio.ini"
    if ini.exists():
        print("📝 platformio.ini found (will NOT modify).")
        # Lewati init. Hanya build supaya CLion dapat compile_commands.json
        run(["pio", "run"], cwd=path)
    else:
        if force_init:
            print("🆕 No platformio.ini. Creating a new PlatformIO project (safe init).")
            run(["pio", "project", "init", "--ide", "clion"], cwd=path)
            run(["pio", "run"], cwd=path)
        else:
            print("⚠️  Skipping: platformio.ini not found. Use --force-init to initialize.")


def main():
    ap = argparse.ArgumentParser(description="Jacktor Audio — PlatformIO + CLion setup (safe)")
    ap.add_argument("--projects", nargs="+", default=[
        "./firmware/amplifier",
        "./firmware/panel"
    ], help="List folder proyek PlatformIO (default: amplifier & panel)")
    ap.add_argument("--force-init", action="store_true",
                    help="Inisialisasi proyek baru HANYA jika tidak ada platformio.ini")
    args = ap.parse_args()

    ensure_pio()
    for p in args.projects:
        process_project(Path(p).resolve(), args.force_init)

    print("\n✅ Done. Open in CLion → Tools → PlatformIO → Reload Project.")


if __name__ == "__main__":
    main()
