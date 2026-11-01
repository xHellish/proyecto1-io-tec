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
# Tests (fuera de la entrega)
# ---------------------------------------------
TEST_DIR = tests
TEST_MODEL = test_model
TEST_SIMPLEX = test_simplex
UI_TEST = ui_test
REFERENCE_SOURCES = $(TEST_DIR)/reference_problems.c
REFERENCE_HEADERS = $(TEST_DIR)/reference_problems.h
MODEL_SOURCES = $(SRC_DIR)/fraction.c $(SRC_DIR)/problem.c
ENGINE_SOURCES = $(MODEL_SOURCES) $(SRC_DIR)/simplex.c
REPORT_SOURCES = $(ENGINE_SOURCES) $(SRC_DIR)/text_report.c

# Compila y corre las pruebas del modelo y del motor (sin GTK)
test: $(TEST_MODEL) $(TEST_SIMPLEX)
	./$(TEST_MODEL)
	./$(TEST_SIMPLEX)

$(TEST_MODEL): $(TEST_DIR)/test_model.c $(MODEL_SOURCES) $(HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) $(TEST_DIR)/test_model.c $(MODEL_SOURCES) -o $@

$(TEST_SIMPLEX): $(TEST_DIR)/test_simplex.c $(REFERENCE_SOURCES) $(ENGINE_SOURCES) $(HEADERS) $(REFERENCE_HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) -I$(TEST_DIR) $(TEST_DIR)/test_simplex.c $(REFERENCE_SOURCES) $(ENGINE_SOURCES) -o $@

# Ventana de prueba para ver el modelo y el motor (requiere GTK; se abre con ./ui_test).
# El objetivo se llama igual que el binario, asi que no es .PHONY
$(UI_TEST): $(TEST_DIR)/ui_test.c $(REFERENCE_SOURCES) $(REPORT_SOURCES) $(HEADERS) $(REFERENCE_HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) -I$(TEST_DIR) $(GTK_CFLAGS) $(TEST_DIR)/ui_test.c $(REFERENCE_SOURCES) $(REPORT_SOURCES) -o $@ $(GTK_LIBS)

# =============================================
# Cleanup
# ---------------------------------------------

# Los objetos son intermedios: make los borra solo tras enlazar y deja unicamente el binario
.INTERMEDIATE: $(OBJECTS) $(DEBUG_OBJECTS) $(ASAN_OBJECTS)

# Deja la carpeta sin ejecutables, objetos ni archivos auxiliares de LaTeX
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET) $(TARGET)_debug $(TARGET)_asan $(TEST_MODEL) $(TEST_SIMPLEX) $(UI_TEST)
	rm -f $(OUTPUT_DIR)/*.aux $(OUTPUT_DIR)/*.log

.PHONY: all debug asan test clean
