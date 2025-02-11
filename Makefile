                                                                                                                                                                                                                                                                                                                            
all:
	mkdir -p build
	cd build && meson setup ..
	cd build && ninja

clean:
	rm -rf build
