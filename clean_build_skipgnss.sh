rm -rf ./build
meson -Dskip_gnss=true --cross-file ./flex-crossfile.ini build
meson compile -C build
