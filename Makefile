CC ?= cc
CFLAGS ?= -std=c11 -O3 -pipe -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?= -pthread -lm

TARGET = quickbench
SOURCES = src/main.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test debug sanitize

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

debug: CFLAGS += -O0 -g3
debug: clean all

sanitize: CFLAGS += -O1 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: clean all

test: $(TARGET)
	sh tests/smoke.sh

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d)

-include $(OBJECTS:.o=.d)
