# Cursor / Host Integration for Autonomous Debugging

To let the Cursor agent **build**, **upload**, and **monitor** the FlexSense device from your Mac (using your `gh` auth, SSH keys, Python venv, and USB) without you running each command manually:

## 1. Enable Legacy Terminal (run in host)

1. Open **Cursor Settings**: `Cmd + ,` (or **Cursor → Settings**).
2. Go to the **Agents** section (sidebar or search "Agents").
3. Under **Terminal** / **Inline Editing & Terminal**, enable **"Legacy Terminal Tool"** (or equivalent option that runs agent commands in the **host** terminal instead of the sandbox).

When enabled, the agent’s terminal commands run in your real environment, so:

- `gh codespace` uses your GitHub login and SSH keys (no repeated passphrase).
- `./build-via-codespace.sh` can list codespaces, SSH, and download binaries.
- `./flash-and-listen.sh` or `./scripts/updater.py` can use your venv and `/dev/cu.usbmodem*`.

## 2. One-time setup in your terminal

In a terminal that Cursor will use (or before asking the agent to build/upload):

```bash
# SSH: enter passphrase once per session
eval $(ssh-agent)
ssh-add

# Optional: activate venv so updater has pyserial
cd /path/to/Flex-SDK
source .venv/bin/activate
```

## 3. What the agent can do autonomously

With Legacy Terminal enabled and the above setup:

- **Build**: run `./build-via-codespace.sh` or `./build-via-codespace.sh --push` (commit, push, pull in Codespace, build, download).
- **Upload + listen**: run `./flash-and-listen.sh /dev/cu.usbmodem1101` (or your port).
- **Read output**: the agent can read the terminal buffer (e.g. serial dump) after you say "read terminal" or point it at the right terminal.

## 4. If Legacy Terminal is unreliable

Cursor’s Legacy Terminal can sometimes freeze or misbehave. If that happens:

- Run the commands yourself in the integrated terminal and ask the agent to **"read terminal"** or **"read output"** when done.
- Or run a single pipeline and log to a file, e.g.  
  `./flash-and-listen.sh /dev/cu.usbmodem1101 2>&1 | tee serial.log`  
  then ask the agent to analyze `serial.log`.

## 5. Port and paths

- Serial port: set `UPDATER_PORT=/dev/cu.usbmodem1101` or pass it as the first argument to `flash-and-listen.sh`.
- Repo root: agent commands assume the current workspace is the Flex-SDK repo root (where `build-via-codespace.sh` and `flash-and-listen.sh` live).
