                                                                                                                                                                                                                                                                                                                            
all:
	rm -rf build
	mkdir -p build
	cd build && meson setup ..
	cd build && ninja
	rsync -avz --delete ./ ananke:~/hyprland-semmety/
clean:
	rm -rf build
