# GPL-3.0.
ROOT := $(CURDIR)
OO_PS4_TOOLCHAIN ?= $(HOME)/OpenOrbis/PS4Toolchain
GOLDHEN_SDK ?= $(HOME)/GoldHEN_Plugins_SDK
PAYLOAD_SDK ?= $(HOME)/ps4-payload-sdk
export OO_PS4_TOOLCHAIN
CC := clang
LD := ld.lld
FSELF := $(OO_PS4_TOOLCHAIN)/bin/linux/create-fself
# GoldHEN can enter plugin callbacks with an 8-byte-aligned stack. Realign every
# compiled function before using SSE stack slots or calling runtime functions.
CFLAGS := --target=x86_64-pc-freebsd12-elf -std=c11 -fPIC -funwind-tables -mstackrealign -O1 -g -Wall -Wextra -Werror -include sched.h -isysroot $(OO_PS4_TOOLCHAIN) -I$(GOLDHEN_SDK)/include -I$(OO_PS4_TOOLCHAIN)/include -Ilayout
LDFLAGS := -m elf_x86_64 -pie --script $(OO_PS4_TOOLCHAIN)/link.x -e _init --eh-frame-hdr -z max-page-size=0x4000 -z common-page-size=0x4000 -L$(OO_PS4_TOOLCHAIN)/lib
COMMON := build/crtprx.o build/GoldHEN.o build/Syscall.o build/probe_common.o
LIBS := -Lbuild -lSceLncUtil  -lSceLibcInternal -lkernel -lmonosgen -lScePad -lSceUserService -lSceSystemService
.PHONY: all
all: dist/pm_shellui_start.bin dist/pm_shellui_marker.prx dist/pm_shellui_keeper.prx
build dist:
	mkdir -p $@
build/crtprx.o: $(GOLDHEN_SDK)/crt/crtprx.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/GoldHEN.o: $(GOLDHEN_SDK)/source/GoldHEN.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/Syscall.o: $(GOLDHEN_SDK)/source/Syscall.c Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/%.o: source/%.c source/probe_common.h source/process_records.h source/hook_record.h source/startup_records.h source/hook_gate.h source/dispatch_gate.h Makefile | build
	$(CC) $(CFLAGS) -c $< -o $@
build/pm_shellui_marker.elf: $(COMMON) build/marker.o build/focus_capture.o build/ui_dispatch.o build/pm_shellui_render.o build/tools_native.o build/files_native.o build/plugins_native.o build/pkg_install.o build/deck-layout-docked.o build/deck-layout.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
build/pm_shellui_keeper.elf: $(COMMON) build/keeper.o
	$(LD) $(LDFLAGS) $^ $(filter-out -lSceLncUtil,$(LIBS)) -o $@
dist/%.prx: build/%.elf | dist
	$(FSELF) -library-path=build -in=$< -out=build/$*.oelf --lib=$@ --paid 0x3800000000000011

# Build payload SDK objects in a separate directory without modifying SDK sources.
PAYLOAD_CFLAGS := -I$(PAYLOAD_SDK)/libPS4/include -Os -std=c11 -ffunction-sections -fdata-sections -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -masm=intel -march=btver2 -mtune=btver2 -m64 -mabi=sysv -mcmodel=small -fpie -fPIC -fvisibility=hidden -mstackrealign
PAYLOAD_SOURCES := $(wildcard $(PAYLOAD_SDK)/libPS4/source/*.c)
PAYLOAD_ASM := $(wildcard $(PAYLOAD_SDK)/libPS4/source/*.s)
PAYLOAD_OBJS := $(patsubst $(PAYLOAD_SDK)/libPS4/source/%.c,build/payload/%.o,$(PAYLOAD_SOURCES)) $(patsubst $(PAYLOAD_SDK)/libPS4/source/%.s,build/payload/%.o,$(PAYLOAD_ASM))
build/payload:
	mkdir -p $@
build/payload/%.o: $(PAYLOAD_SDK)/libPS4/source/%.c Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/payload/%.o: $(PAYLOAD_SDK)/libPS4/source/%.s Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/payload/libPS4.a: $(PAYLOAD_OBJS)
	ar rcs $@ $^
build/payload/start_payload.o: source/start_payload.c source/process_records.h source/startup_records.h source/hook_gate.h source/dispatch_gate.h Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -Werror -c $< -o $@
build/payload/crt0.o: $(PAYLOAD_SDK)/libPS4/crt0.s Makefile | build/payload
	gcc $(PAYLOAD_CFLAGS) -c $< -o $@
build/pm_shellui_start.elf: build/payload/start_payload.o build/payload/libPS4.a build/payload/crt0.o payload.ld
	gcc -nostdlib -nostartfiles -pie -Wl,--no-dynamic-linker -Wl,-Bsymbolic -Wl,--build-id=none -Wl,--gc-sections -Wl,-T,payload.ld -Wl,-z,max-page-size=0x4000 -o $@ build/payload/crt0.o build/payload/start_payload.o build/payload/libPS4.a
dist/pm_shellui_start.bin: build/pm_shellui_start.elf | dist
	objcopy -O binary --set-section-flags .bss=alloc,load,contents $< $@

build/deck-layout-docked.o: source/deck-layout-docked.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/deck-layout.o: layout/deck-layout.c | build
	$(CC) $(CFLAGS) -c $< -o $@


build/ui_dispatch.o: source/ui_dispatch.S | build
	$(CC) --target=x86_64-pc-freebsd12-elf -fPIC -c $< -o $@

build/marker.o: source/fan_display.h source/menu_actions_runtime.h source/menu_actions.h source/menu_themes.h source/catalog_generated.h source/menu_input.h
build/pm_shellui_render.o: source/menu_themes.h

build/libSceShellUIUtil.so: source/shelluiutil_stub.c | build
	$(CC) --target=x86_64-pc-freebsd12-elf -fuse-ld=lld -nostdlib -shared -fPIC -o $@ $<

build/focus_capture.o: source/focus_engine.h

build/tools_native.o: source/fan_profile.h
