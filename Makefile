CC      = gcc
LD      = ld
OBJCOPY = objcopy
CARGO   = cargo

EFI_INCLUDE = /usr/include/efi
EFI_LDS     = /usr/lib/elf_x86_64_efi.lds
EFI_CRT     = /usr/lib/crt0-efi-x86_64.o
ARCH        = x86_64-linux-gnu
EFI_LIBDIR  = /usr/lib

CFLAGS = \
    -I$(EFI_INCLUDE) \
    -I$(EFI_INCLUDE)/x86_64 \
    -fno-stack-protector \
    -fpic \
    -fshort-wchar \
    -mno-red-zone \
    -DEFI_FUNCTION_WRAPPER \
    -ffreestanding \
    -O2 \
    -Wall

LDFLAGS = \
    -nostdlib \
    -znocombreloc \
    -T $(EFI_LDS) \
    -shared \
    -Bsymbolic \
    $(EFI_CRT)

LIBS = -L$(EFI_LIBDIR) -lefi -lgnuefi

BUILD = build

RUST_DIR        = src/parser
RUST_SRC        = $(shell find $(RUST_DIR)/src -name '*.rs')
RUST_TARGET_DIR = $(abspath $(BUILD)/rust-target)
RUST_LIB        = $(BUILD)/libfsparser.a

.PHONY: all clean

all: $(BUILD)/BOOTX64.EFI

$(BUILD)/BOOTX64.EFI: $(BUILD)/main.so
	$(OBJCOPY) \
	    -j .text -j .sdata -j .data -j .dynamic \
	    -j .dynsym -j .rel -j .rela -j .reloc \
	    --target=efi-app-x86_64 $< $@
	@echo "Built: $@"

C_SRCS = $(shell find src -name '*.c' -not -path '$(RUST_DIR)/*')
C_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS))

$(BUILD)/main.so: $(C_OBJS) $(RUST_LIB)
	$(LD) $(LDFLAGS) $(C_OBJS) $(RUST_LIB) $(LIBS) -o $@

$(BUILD)/%.o: src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(RUST_LIB): $(RUST_SRC) $(RUST_DIR)/Cargo.toml | $(BUILD)
	$(CARGO) build --manifest-path $(RUST_DIR)/Cargo.toml --release --target-dir $(RUST_TARGET_DIR)
	cp $(RUST_TARGET_DIR)/release/libfsparser.a $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
