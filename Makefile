all:
	ninja -C build
	scp build/libsemmety.so ananke:/repos/semmety/build/libsemmety.so

setup:
	meson setup build

reset: clean setup all

clean:
	rm -rf build
