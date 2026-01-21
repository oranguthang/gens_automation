# Gens-rr Makefile
# Requires Visual Studio 2022 with MSBuild
# Note: Only Release builds supported (Starscream subprojects lack Debug config)

SHELL = cmd.exe

MSBUILD = "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
PROJECT = Gens.vcxproj
PLATFORM = Win32
TOOLSET = v143

.PHONY: all release clean

all: release

release:
	@echo "Building Release..."
	$(MSBUILD) $(PROJECT) -p:Configuration=Release -p:Platform=$(PLATFORM) -p:PlatformToolset=$(TOOLSET) -v:minimal
	@echo "Output: Output/Gens.exe"

clean:
	@echo "Cleaning..."
	-@if exist Release rmdir /s /q Release
	-@if exist Debug rmdir /s /q Debug
	-@if exist Output rmdir /s /q Output
	-@if exist 7z\Release rmdir /s /q 7z\Release
	-@if exist 7z\Debug rmdir /s /q 7z\Debug
	-@if exist Starscream\Main68k\Release rmdir /s /q Starscream\Main68k\Release
	-@if exist Starscream\Main68k\Debug rmdir /s /q Starscream\Main68k\Debug
	-@if exist Starscream\Sub68k\Release rmdir /s /q Starscream\Sub68k\Release
	-@if exist Starscream\Sub68k\Debug rmdir /s /q Starscream\Sub68k\Debug
	-@if exist Starscream\Main68k\main68k.asm del /q Starscream\Main68k\main68k.asm
	-@if exist Starscream\Sub68k\sub68k.asm del /q Starscream\Sub68k\sub68k.asm
	-@if exist dependencies\gitrev.h del /q dependencies\gitrev.h
	@echo "Clean complete"
