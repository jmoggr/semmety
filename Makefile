                                                                                                                                                                                                                                                                                                                            
all:
	rm -rf build
	mkdir -p build
	cd build && meson setup ..
	cd build && ninja
	cp build/libSemmety.so ~/hyprland-semmety/build/
clean:
	rm -rf build
