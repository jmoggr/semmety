all:
	ninja -C build
	rsync build/libsemmety.so ananke:/repos/semmety/build/

setup:
	meson setup build

reset: clean setup all

clean:
	rm -rf build
