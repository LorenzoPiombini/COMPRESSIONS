.DEFAULT_GOAL := pack

.PHONY: debug clean

DISTRO := $(shell . /etc/os-release && echo $$ID)

SRC = $(wildcard src/*.c)

PACK_OBJ = $(patsubst src/%.c, obj/pack/%.o, $(SRC))
PACK_DEBUG_OBJ = $(patsubst src/%.c, obj/pack-debug/%.o, $(SRC))

CFLAGS= -Iinclude -Wall -Wextra
RELEASE_CFLAGS= -O2 -fhardened
DEBUG_FLAGS = -O0 -g3 -fsanitize=address,undefined
SECURITY_FLAGS = -Wl,-z,noexecstack


ifeq ($(DISTRO),fedora)
	LIBDIR = /usr/lib64
else
	LIBDIR = /usr/local/lib
endif

OBJlibpack = obj/lz77.o  obj/os_operation.o 

LIBNAMEpack = pack
INCLUDEDIR = /usr/local/include
STATICpack = lib$(LIBNAMEpack).a
SHAREDpack = lib$(LIBNAMEpack).so


library: $(STATICpack)

$(SHAREDpack): $(OBJlibpack)
	gcc -shared -o $@ $^

$(STATICpack): $(OBJlibpack)
	ar rcs -o $@ $^


default: $(TARGET)

obj/%.o: src/%.c
	@mkdir -p $(@D)
	gcc $(CFLAGS) -fPIC -c $< -o $@

# Normal pack utility
pack: $(PACK_OBJ)
	gcc -o $@ $^ $(RELEASE_CFLAGS) $(SECURITY_FLAGS)

pack-debug: $(PACK_DEBUG_OBJ)
	gcc -o $@ $^  $(DEBUG_FLAGS)


obj/pack/%.o: src/%.c
	@mkdir -p $(@D)
	gcc  $(CFLAGS) $(RELEASE_CFLAGS) -c $< -o $@

obj/pack-debug/%.o : src/%.c
	@mkdir -p $(@D)
	gcc $(CFLAGS) $(DEBUG_FLAGS) -c $< -o $@

clean: 
	rm -rf obj/pack
	rm -rf obj/pack-debug obj/ez-debug
	rm -f pack 
	rm -f *.so
	rm -f *.a
