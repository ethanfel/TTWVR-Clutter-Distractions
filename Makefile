WINCXX ?= i686-w64-mingw32-g++
FLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
NAME := TTWVRClutterDistractions
.PHONY: all test package
all: build/$(NAME).dll
build:
	mkdir -p $@
build/$(NAME).dll: src/Plugin.cpp src/Impacts.h src/World.h | build
	$(WINCXX) $(FLAGS) -static -shared -s $< -o $@
build/impacts: tests/Impacts.cpp src/Impacts.h | build
	$(CXX) $(FLAGS) $< -o $@
build/world-test: tests/World.cpp src/World.h src/Impacts.h | build
	$(CXX) $(FLAGS) -m32 $< -o $@
test: build/impacts build/world-test
	./build/impacts
	./build/world-test
package: all test
	python3 -c "from zipfile import ZipFile, ZIP_DEFLATED; from pathlib import Path; p=Path('build/$(NAME)-0.1.0.zip'); z=ZipFile(p,'w',ZIP_DEFLATED); z.write('build/$(NAME).dll','NVSE/Plugins/$(NAME).dll'); z.close(); print(p)"
