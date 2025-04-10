rm -rf ./build
meson --cross-file ./flex-crossfile.ini build
meson compile -C build
