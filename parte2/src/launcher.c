#include "launcher.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>

// =============================================
// Constants
// ---------------------------------------------
#define PDFLATEX_PROGRAM "pdflatex"
#define DEFAULT_FILE_NAME "problema"
#define MAX_FILE_NAME_LENGTH 64
#define OUTPUT_DIR_MODE 0755

// Visores en orden de preferencia; el enunciado pide evince o un equivalente
static const char *VIEWER_PROGRAMS[] = {"evince", "okular"};

// =============================================
// Public functions
// ---------------------------------------------

// Deja solo letras, numeros, "-" y "_" del nombre del problema, sin tildes
void launcher_file_name(const char *problem_name, char *buffer, size_t buffer_size) {
    size_t limit = buffer_size - 1;
    size_t length = 0;
    if (limit > MAX_FILE_NAME_LENGTH) {
        limit = MAX_FILE_NAME_LENGTH;
    }

    // Quitar tildes y enes (á pasa a a, ñ a n); lo que no se puede convertir queda como "?"
    gchar *ascii_name = g_str_to_ascii(problem_name, NULL);

    for (const char *cursor = ascii_name; *cursor != '\0' && length < limit; cursor++) {
        char character = *cursor;
        if (g_ascii_isalnum(character) || character == '-' || character == '_') {
            buffer[length] = character;
            length++;
        } else if (g_ascii_isspace(character)) {
            // Un solo "_" por grupo de espacios y nunca al inicio
            if (length > 0 && buffer[length - 1] != '_') {
                buffer[length] = '_';
                length++;
            }
        }
    }
    g_free(ascii_name);

    // Sin "_" al final
    while (length > 0 && buffer[length - 1] == '_') {
        length--;
    }
    buffer[length] = '\0';

    if (length == 0) {
        snprintf(buffer, buffer_size, "%s", DEFAULT_FILE_NAME);
    }
}

// Crea la carpeta de salida con sus carpetas padre
int launcher_prepare_output_dir(const char *output_dir, char *message, size_t message_size) {
    if (g_mkdir_with_parents(output_dir, OUTPUT_DIR_MODE) != 0) {
        snprintf(message, message_size, "No se pudo crear la carpeta de salida '%s': %s", output_dir,
                 g_strerror(errno));
        return 0;
    }
    return 1;
}

// Ejecuta pdflatex en la carpeta de salida y revisa que termine bien
int launcher_compile(const char *output_dir, const char *tex_file_name, char *message, size_t message_size) {
    const gchar *arguments[] = {PDFLATEX_PROGRAM, "-interaction=nonstopmode", "-halt-on-error", tex_file_name, NULL};
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint wait_status = 0;
    GError *error = NULL;

    // La salida de pdflatex se captura para que nunca llegue a la terminal; el detalle queda en su .log
    gboolean is_started = g_spawn_sync(output_dir, (gchar **)arguments, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                       &standard_output, &standard_error, &wait_status, &error);
    g_free(standard_output);
    g_free(standard_error);

    // pdflatex no se pudo ejecutar (por ejemplo, no esta instalado)
    if (!is_started) {
        snprintf(message, message_size, "No se pudo ejecutar pdflatex: %s", error->message);
        g_error_free(error);
        return 0;
    }

    // pdflatex corrio pero el documento tiene un error
    if (!g_spawn_check_wait_status(wait_status, &error)) {
        snprintf(message, message_size,
                 "pdflatex no pudo compilar el documento. Revise el archivo .log en '%s' (%s).", output_dir,
                 error->message);
        g_error_free(error);
        return 0;
    }
    return 1;
}

// Busca un visor instalado y abre el PDF sin esperar a que se cierre
int launcher_open_pdf(const char *pdf_path, char *message, size_t message_size) {
    int viewer_count = (int)(sizeof(VIEWER_PROGRAMS) / sizeof(VIEWER_PROGRAMS[0]));
    const char *viewer = NULL;
    GError *error = NULL;

    // Primer visor disponible en el PATH
    for (int index = 0; index < viewer_count && viewer == NULL; index++) {
        gchar *viewer_path = g_find_program_in_path(VIEWER_PROGRAMS[index]);
        if (viewer_path != NULL) {
            viewer = VIEWER_PROGRAMS[index];
            g_free(viewer_path);
        }
    }
    if (viewer == NULL) {
        snprintf(message, message_size, "No se encontró evince ni okular. El PDF quedó en '%s'.", pdf_path);
        return 0;
    }

    // Modo presentacion, con la salida del visor descartada para que no llegue a la terminal
    const gchar *arguments[] = {viewer, "--presentation", pdf_path, NULL};
    GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;
    if (!g_spawn_async(NULL, (gchar **)arguments, NULL, flags, NULL, NULL, NULL, &error)) {
        snprintf(message, message_size, "No se pudo abrir %s: %s. El PDF quedó en '%s'.", viewer, error->message,
                 pdf_path);
        g_error_free(error);
        return 0;
    }
    return 1;
}
