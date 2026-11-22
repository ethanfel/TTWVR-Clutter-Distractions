WINCXX ?= i686-w64-mingw32-g++
FLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
NAME := TTWVRClutterDistractions
.PHONY: all test package
all: build/$(NAME).dll build/$(NAME).esp
build:
	mkdir -p $@
build/$(NAME).dll: src/Plugin.cpp src/BearTraps.cpp src/TrapInput.cpp src/BearTraps.h src/TrapInput.h src/TrapGesture.h src/GameScript.h src/Impacts.h src/World.h | build
	$(WINCXX) $(FLAGS) -static -shared -s $(filter %.cpp,$^) -o $@
build/$(NAME).esp: tools/trap_records.py | build
	python3 $< $@
build/impacts: tests/Impacts.cpp src/Impacts.h | build
	$(CXX) $(FLAGS) $< -o $@
build/world-test: tests/World.cpp src/World.h src/Impacts.h | build
	$(CXX) $(FLAGS) -m32 $< -o $@
build/trap-test: tests/TrapGesture.cpp src/TrapGesture.h src/Impacts.h | build
	$(CXX) $(FLAGS) $< -o $@
test: build/impacts build/world-test build/trap-test build/$(NAME).esp
	./build/impacts
	./build/world-test
	./build/trap-test
	python3 tests/test_records.py build/$(NAME).esp
package: all test
	python3 -c "from zipfile import ZipFile, ZIP_DEFLATED; from pathlib import Path; p=Path('build/$(NAME)-0.2.0.zip'); z=ZipFile(p,'w',ZIP_DEFLATED); z.write('build/$(NAME).dll','NVSE/Plugins/$(NAME).dll'); z.write('build/$(NAME).esp','$(NAME).esp'); z.close(); print(p)"
