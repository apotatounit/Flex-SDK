---
name: flex-build-codespace-rpi-upload
description: >-
  Builds Flex-SDK firmware via GitHub Codespace using build-via-codespace.sh,
  then flashes from a Raspberry Pi over USB serial to the Myriota FlexSense
  device. Covers gh auth on the Pi, CODESPACE_NAME when no codespace is found,
  wake-and-listen and reboot-and-listen helper scripts, flash-and-listen, or
  building on a dev machine and copying binaries before updater.py. Use when
  the user mentions Flex-SDK, build-via-codespace, Codespace build, RPi upload,
  Raspberry Pi flash, UPDATER_PORT, wake, reboot, or serial listen for this
  repo. For ESP32 over Pi, use esp32-pio-rpi-upload in esp32_newcommence.
---

# Flex-SDK: Codespace build → Raspberry Pi → device upload

**ESP32 on the Pi** (PlatformIO, different repo): **`esp32-pio-rpi-upload`** — `esp32_newcommence/.cursor/skills/esp32-pio-rpi-upload/SKILL.md`.

## Pi serial basics (this stack)

- User in **`dialout`**; prefer **`/dev/serial/by-id/...`**; do **not** use macOS `/dev/cu.*` on the Pi.
- If upload/listen fails, check **`fuser`** / **`lsof`** on the TTY for a stuck monitor.

## Script of record

Repo root: [build-via-codespace.sh](../../../build-via-codespace.sh)

It uses **`gh codespace ssh`** to compile in the Codespace, **`gh codespace cp`** to pull `user_application*.bin` into **`LOCAL_BUILD_DIR`** (default `./build`), then optionally runs **`./scripts/updater.py`** with **`pyserial`**.

## Codespace must exist

If **`gh codespace list`** returns nothing, create a Codespace for this repo in the browser (GitHub → **Code** → **Codespaces** → **New**), then re-run the script. On the Pi, **`gh auth login`** (scopes including **codespace** and **repo**) is required before `gh codespace` works.

## Pi helper scripts (no flash unless noted)

Repo root wrappers around **`scripts/updater.py`**. Prefer **`/dev/serial/by-id/*FlexSense*if00`** on Linux; scripts auto-detect when possible.

| Script | Purpose |
|--------|---------|
| [wake-and-listen.sh](../../../wake-and-listen.sh) | Start app and listen (`-s -l`); optional **`UPDATER_PORT`** or first arg as device path. |
| [reboot-and-listen.sh](../../../reboot-and-listen.sh) | Bootloader capture / reset (`-v`), then start and listen (`-s -l`); same port env/arg. |
| [flash-and-listen.sh](../../../flash-and-listen.sh) | Flash **`./build/user_application.bin`**, start, listen (`-m … -s -l`). |
| [listen-serial.sh](../../../listen-serial.sh) | Listen only (`-l`), no start/flash. |

Sync repo to the Pi (`rsync` or `git pull`), then `chmod +x` if needed and run from **`~/Flex-SDK`**.

## Flow A — Everything on the Raspberry Pi

Use when the USB device is attached to the Pi and the agent should not assume a Mac serial port.

1. **On the Pi:** clone or sync this repo; install **GitHub CLI** and authenticate (`gh auth login`). SSH agent forwarding is optional; avoid repeated passphrase prompts the same way as on macOS (`eval "$(ssh-agent -s)"`; `ssh-add`).
2. **Python:** `python3 -c "import serial"` must succeed. If not: `pip install --user 'pyserial==3.5'` or a venv with `pip install -r requirements.txt` from the repo root.
3. **Serial permissions:** Pi user in **`dialout`**: `groups` should list `dialout`; if not, `sudo usermod -aG dialout "$USER"` and re-login.
4. **Device node:** set **`UPDATER_PORT`** to a Linux TTY, for example:
   - `/dev/serial/by-id/<stable-id>` (preferred in scripts)
   - `/dev/ttyACM0` or `/dev/ttyUSB0` if verified
   Do **not** use macOS `/dev/cu.*` on the Pi.
5. **Non-interactive Codespace pick:** export **`CODESPACE_NAME`** when more than one Codespace exists so `select` is not required.
6. **Run:**

```bash
cd /path/to/Flex-SDK
export CODESPACE_NAME="your-codespace-name"   # optional but recommended for agents
export UPDATER_PORT="/dev/serial/by-id/..."
./build-via-codespace.sh --upload
```

Flash and **read serial** after upload (matches script’s `--listen` path):

```bash
export UPDATER_PORT="/dev/serial/by-id/..."
./build-via-codespace.sh --upload --listen
```

**Branch-aware build** (same as local `git branch`): the script checks out that branch in the Codespace before `clean_build_*.sh` / `meson`. **`diagnostics`** branch selects diagnostics meson options; **`--gps`** enables GNSS build instead of skip-GNSS default.

**Optional push before build:** `./build-via-codespace.sh --push --upload` (commits all changes with a default message — only use when the user explicitly wants that).

## Flow B — Build on dev machine, upload only on the Pi

Use when **`gh`** / Codespace access is only on the Mac (or CI), but hardware is on the Pi.

1. On the dev machine (repo root): `./build-via-codespace.sh` (omit `--upload` if no local device).
2. Copy artifacts to the Pi, for example:

```bash
scp ./build/user_application.bin ./build/user_application.nonetwork.bin \
  user@pi-host:/path/to/Flex-SDK/build/
```

3. SSH to the Pi and run **`updater.py`** explicitly (equivalent to what `--upload` does after download):

```bash
cd /path/to/Flex-SDK
python3 scripts/updater.py -m ./build/user_application.bin -p "$UPDATER_PORT"
```

With start + listen + wait for port (aligned with `--upload --listen` and `UPDATER_PORT` set):

```bash
python3 scripts/updater.py -m ./build/user_application.bin -p "$UPDATER_PORT" -s -l -w
```

## Environment variables (quick reference)

| Variable | Role |
|----------|------|
| `CODESPACE_NAME` | Target Codespace when listing would be interactive |
| `REMOTE_WORKSPACE` | Remote path (default `/workspaces/Flex-SDK`) |
| `LOCAL_BUILD_DIR` | Where binaries are stored locally on the machine running the script (default `./build`) |
| `UPDATER_PORT` | Serial device for `updater.py` (**Linux** path on Pi) |
| `GPS` | Set non-empty with `--gps` flag for GNSS-enabled build |

## Agent checklist

```
- [ ] Confirm branch (diagnostics vs default) and whether user wants --gps.
- [ ] Codespace: set CODESPACE_NAME if multiple codespaces exist.
- [ ] Pi: pyserial import OK; user in dialout; UPDATER_PORT exists (ls /dev/serial/by-id).
- [ ] Run flow A or B consistently (do not mix Mac cu.* with Pi).
- [ ] On failure: capture updater stderr and verify no other process holds the TTY (fuser / lsof).
- [ ] After flash or for logs only: use **wake-and-listen** or **reboot-and-listen** on the Pi as appropriate.
```

## Related serial workflow

For generic **SSH → Pi → serial** capture (tmux, `stty`, `socat`), use the **`rpi-serial-agent-dev`** skill in **pi5-setup** if present in the workspace; this skill stays Flex-SDK and **`build-via-codespace.sh`** specific.
