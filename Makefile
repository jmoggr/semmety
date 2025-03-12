all:
	ninja -C build

setup:
	meson setup build

reset: clean setup all

clean:
	rm -rf build


