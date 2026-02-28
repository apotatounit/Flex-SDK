# Cursor / Host Integration for Autonomous Debugging

To let the Cursor agent **build**, **upload**, and **monitor** the FlexSense device from your Mac (using your `gh` auth, SSH keys, Python venv, and USB) without you running each command manually:

## 1. Let the agent run in your environment (host / non-sandbox)

Cursor’s UI changes between versions. Try these in order:

**Option A – Settings (if you see it)**  
Open **Cursor Settings** (`Cmd + ,`), then search the settings search box for:
- **"Agent"** or **"Agents"** → look for **Terminal** or **Legacy Terminal Tool**
- **"Chat"** → **Auto Run** → look for options to run commands **outside the sandbox** or use a **Legacy** terminal
- **"Sandbox"** → turn off “Auto-run in sandbox” or set “Ask every time” so you can choose **Run** (no sandbox) when the agent runs a command

**Option B – Approve “Run” when the agent runs a command**  
You don’t have to find a setting. When the agent runs a command and it fails (e.g. network or device blocked by the sandbox), Cursor usually offers:
- **Run** (or “Run without sandbox”) – runs in your real terminal with your `gh`, venv, and USB. Click this so build/upload/serial work.
- **Add to allowlist** – optional, so future runs of that command can be auto-approved.

So: ask the agent to build or flash; when it suggests a terminal command, approve it and choose **Run** (not sandbox) so it uses your host environment.

When commands run in your real environment:
- `gh codespace` uses your GitHub login and SSH keys.
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

## 6. Autonomous build → flash → capture → iterate

To have the agent iterate (build, flash, read output, change code, repeat until “enough”):

**Option A – You run the cycle, agent iterates on the capture**  
1. In your terminal (with `ssh-add` and venv active):  
   `./autonomous-build-flash-capture.sh [--push] [PORT] [SECONDS]`  
   Example: `./autonomous-build-flash-capture.sh --push /dev/cu.usbmodem1101 90`  
2. Serial output is written to `./serial_capture.txt`.  
3. Ask the agent to “read serial_capture.txt and iterate” (or “evaluate and suggest changes”).  
4. Agent suggests code changes; you apply them (or switch to Agent mode to apply).  
5. Run the script again; repeat until the output meets your criteria.

**Option B – Agent runs the cycle with no intervention (requires host terminal)**  
For the agent to run build → flash → capture → evaluate → edit → repeat **without you doing anything**, Cursor must run the agent’s terminal commands in your **host** shell (same one where `gh` and `ssh-add` work). If the agent’s commands run in a sandbox or another environment, the Codespace SSH step will always fail with “Permission denied (publickey)”.  
When host terminal is active, the agent can run `./autonomous-build-flash-capture.sh --push /dev/cu.usbmodem1101 90`, read `serial_capture.txt`, stop when output is good enough (e.g. “stable=yes”, “RESULT: Minimum reliable”) or after a few iterations, and edit code and re-run in between. Non-interactive push uses `COMMIT_MSG` so there is no commit prompt.
