rm -rf ./build
python3 scripts/download_binaries.py
meson --cross-file ./flex-crossfile.ini build
meson compile -C build
