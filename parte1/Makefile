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
REFERENCE_HEADERS = $(TEST_DIR)/reference_problems.h $(TEST_DIR)/text_report.h
MODEL_SOURCES = $(SRC_DIR)/fraction.c $(SRC_DIR)/problem.c
ENGINE_SOURCES = $(MODEL_SOURCES) $(SRC_DIR)/simplex.c
REPORT_SOURCES = $(ENGINE_SOURCES) $(TEST_DIR)/text_report.c

# Compila y corre las pruebas del modelo y del motor (sin GTK)
test: $(TEST_MODEL) $(TEST_SIMPLEX)
	./$(TEST_MODEL)
	./$(TEST_SIMPLEX)

$(TEST_MODEL): $(TEST_DIR)/test_model.c $(MODEL_SOURCES) $(HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) $(TEST_DIR)/test_model.c $(MODEL_SOURCES) -o $@

$(TEST_SIMPLEX): $(TEST_DIR)/test_simplex.c $(REFERENCE_SOURCES) $(ENGINE_SOURCES) $(HEADERS) $(REFERENCE_HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) -I$(TEST_DIR) $(TEST_DIR)/test_simplex.c $(REFERENCE_SOURCES) $(ENGINE_SOURCES) -o $@

# Prueba de punta a punta sin ventana: genera y compila con pdflatex muchos documentos
# (requiere pdflatex). Uso: make test_report REPORT_DIR=/ruta/de/salida
TEST_REPORT = test_report
REPORT_DIR ?= /tmp/test_report_output
# Con libasan instalado: make test_report SANITIZE="-fsanitize=address,undefined"
SANITIZE ?=
PIPELINE_SOURCES = $(ENGINE_SOURCES) $(SRC_DIR)/latex_report.c $(SRC_DIR)/launcher.c

$(TEST_REPORT): $(TEST_DIR)/test_report.c $(REFERENCE_SOURCES) $(PIPELINE_SOURCES) $(HEADERS) $(REFERENCE_HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) $(SANITIZE) -I$(SRC_DIR) -I$(TEST_DIR) $(GTK_CFLAGS) $(TEST_DIR)/test_report.c $(REFERENCE_SOURCES) $(PIPELINE_SOURCES) -o $@ $(GTK_LIBS) -lm

test_report_run: $(TEST_REPORT)
	./$(TEST_REPORT) $(REPORT_DIR)

# Prueba que maneja la ventana real (requiere pantalla y pdflatex): llena campos, presiona Resolver
# y cierra los dialogos solos. Poner un "evince" falso en el PATH evita abrir visores.
# Uso: make test_gui GUI_DIR=/ruta/de/salida
TEST_GUI = test_gui
GUI_DIR ?= /tmp/test_gui_output
GUI_SOURCES = $(SRC_DIR)/fraction.c $(SRC_DIR)/problem.c $(SRC_DIR)/simplex.c $(SRC_DIR)/latex_report.c $(SRC_DIR)/launcher.c $(SRC_DIR)/main_window.c

$(TEST_GUI): $(TEST_DIR)/test_gui.c $(GUI_SOURCES) $(HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) $(SANITIZE) -I$(SRC_DIR) $(GTK_CFLAGS) $(TEST_DIR)/test_gui.c $(GUI_SOURCES) -o $@ $(GTK_LIBS) -lm

test_gui_run: $(TEST_GUI)
	./$(TEST_GUI) ui/main_window.glade $(GUI_DIR)

# Ventana de prueba para ver el modelo y el motor (requiere GTK; se abre con ./ui_test).
# El objetivo se llama igual que el binario, asi que no es .PHONY
$(UI_TEST): $(TEST_DIR)/ui_test.c $(REFERENCE_SOURCES) $(REPORT_SOURCES) $(HEADERS) $(REFERENCE_HEADERS)
	$(CC) $(WARNING_FLAGS) $(DEBUG_FLAGS) -I$(SRC_DIR) -I$(TEST_DIR) $(GTK_CFLAGS) $(TEST_DIR)/ui_test.c $(REFERENCE_SOURCES) $(REPORT_SOURCES) -o $@ $(GTK_LIBS)

# =============================================
# Delivery package
# ---------------------------------------------
DIST_NAME = proyecto1-simplex

# Crea ../proyecto1-simplex.tgz solo con fuentes, .glade, logo, Makefile y README (sin tests ni salida)
dist: clean
	tar -czf ../$(DIST_NAME).tgz --transform 's,^,$(DIST_NAME)/,' Makefile README.md $(SRC_DIR) ui assets

# =============================================
# Cleanup
# ---------------------------------------------

# Los objetos son intermedios: make los borra solo tras enlazar y deja unicamente el binario
.INTERMEDIATE: $(OBJECTS) $(DEBUG_OBJECTS) $(ASAN_OBJECTS)

# Deja la carpeta sin ejecutables, objetos ni archivos auxiliares de LaTeX
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET) $(TARGET)_debug $(TARGET)_asan $(TEST_MODEL) $(TEST_SIMPLEX) $(UI_TEST) $(TEST_REPORT) $(TEST_GUI)
	rm -f $(OUTPUT_DIR)/*.aux $(OUTPUT_DIR)/*.log

.PHONY: all debug asan test test_report_run test_gui_run clean dist
