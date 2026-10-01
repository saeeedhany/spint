CC ?= cc
BUILD ?= build
TARGET ?= spint
OPT ?= -O2

WARN = -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
CFLAGS += -std=c11 $(OPT) $(WARN) -isystem third_party -MMD -MP
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
LDLIBS += -lm

CORE = canvas draw history io
APP = main app input render ui font view

CORE_OBJ = $(CORE:%=$(BUILD)/%.o)
APP_OBJ = $(APP:%=$(BUILD)/%.o)

PREFIX ?= /usr/local

.PHONY: all run test bench debug clean install uninstall

all: $(TARGET)

$(TARGET): $(CORE_OBJ) $(APP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(SDL_LIBS) $(LDLIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/test_core: tests/test_core.c $(CORE_OBJ) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CORE_OBJ) $(LDLIBS)

$(BUILD):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

test: $(BUILD)/test_core
	./$(BUILD)/test_core

bench: $(BUILD)/test_core
	./$(BUILD)/test_core --bench

debug:
	$(MAKE) BUILD=build/debug TARGET=spint-debug OPT="-O0 -g -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined"

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -rf build spint spint-debug

-include $(wildcard $(BUILD)/*.d)
