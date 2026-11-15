// =============================================
// Includes
// ---------------------------------------------
#include <gtk/gtk.h>

#include "main_window.h"

// =============================================
// Constants
// ---------------------------------------------
#define GLADE_RELATIVE_PATH "ui/main_window.glade"
#define MAIN_WINDOW_ID "main_window"
#define EXECUTABLE_LINK "/proc/self/exe"
#define OUTPUT_DIR_NAME "output"

// =============================================
// Helpers
// ---------------------------------------------

// Muestra un dialogo modal de error con el mensaje recibido
static void show_error_dialog(const char *message) {
    // Crear el dialogo sin ventana padre (puede ocurrir antes de abrir la ventana)
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");

    // Mostrarlo hasta que el usuario lo cierre y liberarlo
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Devuelve la carpeta del ejecutable, o NULL si no se pudo resolver (liberar con g_free)
static gchar *get_executable_dir(GError **error) {
    // Resolver la ruta real del ejecutable en ejecucion
    gchar *executable_path = g_file_read_link(EXECUTABLE_LINK, error);
    if (executable_path == NULL) {
        return NULL;
    }

    // Quedarse solo con la carpeta que lo contiene
    gchar *executable_dir = g_path_get_dirname(executable_path);
    g_free(executable_path);
    return executable_dir;
}

// =============================================
// Signal handlers
// ---------------------------------------------

// Termina el programa al cerrar la ventana principal
void on_main_window_destroy(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    gtk_main_quit();
}

// =============================================
// Main
// ---------------------------------------------

// Carga la ventana desde el .glade (ruta relativa al ejecutable) y corre el bucle de GTK
int main(int argc, char *argv[]) {
    GError *error = NULL;

    // Inicializar GTK
    gtk_init(&argc, &argv);

    // Localizar el .glade junto al ejecutable, sin depender de la carpeta actual
    gchar *executable_dir = get_executable_dir(&error);
    if (executable_dir == NULL) {
        gchar *message = g_strdup_printf("No se pudo ubicar la carpeta del programa: %s", error->message);
        show_error_dialog(message);
        g_free(message);
        g_error_free(error);
        return 1;
    }
    gchar *glade_path = g_build_filename(executable_dir, GLADE_RELATIVE_PATH, NULL);
    gchar *output_dir = g_build_filename(executable_dir, OUTPUT_DIR_NAME, NULL);
    g_free(executable_dir);

    // Cargar la interfaz con GtkBuilder
    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_callback_symbol(
        builder, "on_main_window_destroy", G_CALLBACK(on_main_window_destroy));

    if (!gtk_builder_add_from_file(builder, glade_path, &error)) {
        gchar *message = g_strdup_printf(
            "No se pudo cargar el archivo de la interfaz '%s': %s", glade_path, error->message);

        show_error_dialog(message);
        g_free(message);

        g_error_free(error);
        g_free(glade_path);
        g_free(output_dir);

        g_object_unref(builder);

        return 1;
    }
    g_free(glade_path);

    // Obtener la ventana principal y verificar que exista en el .glade
    GtkWidget *main_window = GTK_WIDGET(gtk_builder_get_object(builder, MAIN_WINDOW_ID));

    if (main_window == NULL) {
        show_error_dialog("El archivo de la interfaz no define la ventana principal.");
        g_free(output_dir);
        g_object_unref(builder);
        return 1;
    }

    // Widgets de la ventana; si falta alguno en el .glade se avisa cual
    const char *missing_id = main_window_setup(builder, main_window, output_dir);
    g_free(output_dir);
    if (missing_id != NULL) {
        gchar *message = g_strdup_printf("Al archivo de la interfaz le falta el widget '%s'.", missing_id);
        show_error_dialog(message);
        g_free(message);
        g_object_unref(builder);
        return 1;
    }

    // Conectar las senales declaradas en el .glade y liberar el builder
    gtk_builder_connect_signals(builder, NULL);
    g_object_unref(builder);

    // Mostrar la ventana y entrar al bucle de eventos
    gtk_widget_show_all(main_window);
    gtk_main();

    return 0;
}
