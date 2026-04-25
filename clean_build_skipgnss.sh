rm -rf ./build
python3 scripts/download_binaries.py
meson -Dskip_gnss=true --cross-file ./flex-crossfile.ini build
meson compile -C build
