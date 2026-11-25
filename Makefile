WINCXX ?= i686-w64-mingw32-g++
FLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
NAME := TTWVRClutterDistractions
AUDIO := TTWVRClutterAudio
TRAPS := TTWVRBearTraps
VERSION := 0.3.0
COMMON := Makefile src/Plugin.cpp src/Addon.h src/GameScript.h src/ControllerInput.h src/Impacts.h src/World.h
.PHONY: all test package audio-stage
all: build/$(AUDIO).dll build/$(TRAPS).dll build/$(NAME).esp
build:
	mkdir -p $@
build/$(AUDIO).dll: $(COMMON) src/ClutterAudio.cpp src/ClutterInput.cpp src/ClutterInput.h src/ThrowMotion.h | build
	$(WINCXX) $(FLAGS) -DTTWVR_VERSION='"$(VERSION)"' -static -shared -s $(filter %.cpp,$^) -o $@
build/$(TRAPS).dll: $(COMMON) src/BearTrapPlugin.cpp src/BearTraps.cpp src/TrapInput.cpp src/DeepCrouch.cpp src/LegPose.cpp src/Kneeling.h src/DeepCrouch.h src/BearTraps.h src/TrapInput.h src/ClutterInput.h src/TrapGesture.h src/TrapCondition.h src/TrapCarrier.h src/HandheldTrap.h src/TrapVisual.h | build
	$(WINCXX) $(FLAGS) -DTTWVR_VERSION='"$(VERSION)"' -static -shared -s $(filter %.cpp,$^) -o $@
build/$(NAME).esp: tools/trap_records.py | build
	python3 $< $@
build/impacts: tests/Impacts.cpp src/Impacts.h src/ThrowMotion.h | build
	$(CXX) $(FLAGS) $< -o $@
build/world-test: tests/World.cpp src/World.h src/Impacts.h | build
	$(CXX) $(FLAGS) -m32 $< -o $@
build/trap-test: tests/TrapGesture.cpp src/TrapGesture.h src/TrapCondition.h src/HandheldTrap.h src/Impacts.h | build
	$(CXX) $(FLAGS) $< -o $@
build/crouch-test: tests/DeepCrouch.cpp src/DeepCrouch.h src/Kneeling.h src/Impacts.h | build
	$(CXX) $(FLAGS) $< -o $@
test: build/crouch-test build/impacts build/world-test build/trap-test build/$(NAME).esp
	./build/crouch-test
	./build/impacts
	./build/world-test
	./build/trap-test
	python3 tests/test_records.py build/$(NAME).esp
package: all test config/$(NAME).ini assets/meshes/TTWVRClutter/Portable.nif assets/meshes/TTWVRClutter/Held.nif
	python3 tools/package.py $(VERSION)

audio-stage: package
	python3 -B tools/audio_stage.py $(VERSION)
