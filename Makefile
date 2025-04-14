all:
	ninja -C build
	scp build/libsemmety.so ananke:/repos/semmety/build/libsemmety-actual.so

setup:
	meson setup build

reset: clean setup all

clean:
	rm -rf build
