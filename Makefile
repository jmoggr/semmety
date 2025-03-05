                                                                                                                                                                                                                                                                                                                            
all:
	mkdir -p build
	cd build && meson setup ..
	cd build && ninja
	cp build/libSemmety.so ~/hyprland-semmety/build/
	scp build/libSemmety.so ananke:~/hyprland-semmety/build
clean:
	rm -rf build
