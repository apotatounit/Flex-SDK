#!/usr/bin/env bash
# Build the Flex SDK (skip-GNSS) in a GitHub Codespace and download binaries to local.
# Use this on macOS (or anywhere) when the build is only supported in the devcontainer.
#
# Prerequisites:
#   - GitHub CLI: brew install gh && gh auth login
#   - A Codespace for this repo (create one in the browser or: gh codespace create)
#
# Usage:
#   ./build-via-codespace.sh              # build in Codespace, download to ./build/
#   ./build-via-codespace.sh --push        # git add, commit, push, then build
#   CODESPACE_NAME=my-codespace ./build-via-codespace.sh
#
set -euo pipefail

REMOTE_WORKSPACE="${REMOTE_WORKSPACE:-/workspaces/Flex-SDK}"
LOCAL_BUILD_DIR="${LOCAL_BUILD_DIR:-./build}"
PUSH=

while [[ $# -gt 0 ]]; do
  case "$1" in
    --push)   PUSH=1; shift ;;
    -h|--help)
      echo "Usage: $0 [--push]"
      echo "  --push   Commit all changes, push, then build in Codespace and download binaries."
      echo "  Env:     CODESPACE_NAME, REMOTE_WORKSPACE, LOCAL_BUILD_DIR"
      exit 0
      ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

if ! command -v gh &>/dev/null; then
  echo "Error: GitHub CLI (gh) is required. Install: brew install gh && gh auth login"
  exit 1
fi

if [[ -n "${PUSH:-}" ]]; then
  echo "==> Staging and committing..."
  git add -A
  if git diff --staged --quiet; then
    echo "No changes to commit."
  else
    read -r -p "Commit message [build via Codespace]: " msg
    git commit -m "${msg:-build via Codespace}"
  fi
  echo "==> Pushing..."
  git push
fi

# Resolve Codespace name
if [[ -z "${CODESPACE_NAME:-}" ]]; then
  echo "==> Listing Codespaces..."
  REPO=$(git remote get-url origin 2>/dev/null | sed -n 's|.*github\.com[:/]\([^/]*/[^/]*\)\.git\?$|\1|p' | tr -d '\n')
  CHOICES=()
  if [[ -n "$REPO" ]]; then
    CHOICES=($(gh codespace list -R "$REPO" --json name -q '.[].name' 2>/dev/null || true))
  fi
  if [[ ${#CHOICES[@]} -eq 0 ]]; then
    # Fall back to all codespaces (e.g. created from fork or different repo)
    CHOICES=($(gh codespace list --json name -q '.[].name' 2>/dev/null || true))
  fi
  if [[ ${#CHOICES[@]} -eq 0 ]]; then
    echo "No Codespace found. Create one at: https://github.com/codespaces (open this repo, then Codespaces → New)."
    exit 1
  fi
  if [[ ${#CHOICES[@]} -eq 1 ]]; then
    CODESPACE_NAME="${CHOICES[0]}"
    echo "Using Codespace: $CODESPACE_NAME"
  else
    echo "Select a Codespace:"
    select CODESPACE_NAME in "${CHOICES[@]}"; do
      [[ -n "$CODESPACE_NAME" ]] && break
    done
  fi
fi

echo "==> Running build in Codespace ($CODESPACE_NAME)..."
gh codespace ssh -c "$CODESPACE_NAME" -- "cd $REMOTE_WORKSPACE && ./clean_build_skipgnss.sh"

echo "==> Downloading binaries to $LOCAL_BUILD_DIR..."
mkdir -p "$LOCAL_BUILD_DIR"
gh codespace cp -c "$CODESPACE_NAME" "remote:$REMOTE_WORKSPACE/build/user_application.bin" "$LOCAL_BUILD_DIR/"
gh codespace cp -c "$CODESPACE_NAME" "remote:$REMOTE_WORKSPACE/build/user_application.nonetwork.bin" "$LOCAL_BUILD_DIR/" 2>/dev/null || true

echo "==> Done. Binaries in $LOCAL_BUILD_DIR:"
ls -la "$LOCAL_BUILD_DIR"/user_application*.bin 2>/dev/null || true
