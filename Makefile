CC ?= aarch64-linux-android36-clang
CFLAGS = -O3 -flto -std=c11
TARGET = extract_f2fs
FILE = main.c read.c extract.c sparse.c buffer_pool.c
OBJECTS = $(FILE:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean