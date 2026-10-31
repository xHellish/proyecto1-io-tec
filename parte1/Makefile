# =============================================
# Configuration
# ---------------------------------------------
CC = gcc
TARGET = simplex
SRC_DIR = src
OUTPUT_DIR = output

SOURCES = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(SRC_DIR)/*.h)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

WARNING_FLAGS = -Wall -Wextra -std=c11
DEBUG_FLAGS = -g -O0

OBJECTS = $(SOURCES:.c=.o)
DEBUG_OBJECTS = $(SOURCES:.c=.debug.o)
ASAN_OBJECTS = $(SOURCES:.c=.asan.o)

# =============================================
# Release build (default)
# ---------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(GTK_LIBS) -lm

$(OBJECTS): %.o: %.c $(HEADERS)
	$(CC) $(WARNING_FLAGS) -O2 $(GTK_CFLAGS) -c $< -o $@

# =============================================
# Debug builds
# ---------------------------------------------

# Debug con simbolos de depuracion (para gdb)
debug: $(TARGET)_debug

$(TARGET)_debug: $(DEBUG_OBJECTS)
	$(CC) $(DEBUG_OBJECTS) -o $@ $(GTK_LIBS) -lm

$(DEBUG_OBJECTS): %.debug.o: %.c $(HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) $(GTK_CFLAGS) -c $< -o $@

# Debug con AddressSanitizer (requiere el paquete libasan)
asan: $(TARGET)_asan

$(TARGET)_asan: $(ASAN_OBJECTS)
	$(CC) -fsanitize=address $(ASAN_OBJECTS) -o $@ $(GTK_LIBS) -lm

$(ASAN_OBJECTS): %.asan.o: %.c $(HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -fsanitize=address $(GTK_CFLAGS) -c $< -o $@

# =============================================
# Cleanup
# ---------------------------------------------

# Los objetos son intermedios: make los borra solo tras enlazar y deja unicamente el binario
.INTERMEDIATE: $(OBJECTS) $(DEBUG_OBJECTS) $(ASAN_OBJECTS)

# Deja la carpeta sin ejecutables, objetos ni archivos auxiliares de LaTeX
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET) $(TARGET)_debug $(TARGET)_asan
	rm -f $(OUTPUT_DIR)/*.aux $(OUTPUT_DIR)/*.log

.PHONY: all debug asan clean
