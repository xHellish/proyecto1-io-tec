#include <gtk/gtk.h>
#include <string.h>

#include "fraction.h"
#include "problem.h"
#include "reference_problems.h"
#include "simplex.h"
#include "text_report.h"

// =============================================
// Constants
// ---------------------------------------------
#define GLADE_RELATIVE_PATH "tests/ui_test.glade"
#define EXECUTABLE_LINK "/proc/self/exe"
#define WINDOW_ID "ui_test_window"
#define REFERENCE_DANTZIG 0
#define REFERENCE_WYNDOR 1
#define REFERENCE_MINIMIZATION 2
#define REFERENCE_MULTIPLE 3
#define REFERENCE_DEGENERATE 4

// =============================================
// Widgets
// ---------------------------------------------
typedef struct {
    GtkWidget *number_entry;
    GtkWidget *number_result;
    GtkWidget *name_entry;
    GtkWidget *variables_spin;
    GtkWidget *constraints_spin;
    GtkWidget *names_entry;
    GtkWidget *negative_rhs_check;
    GtkWidget *model_text;
    GtkWidget *problem_combo;
    GtkWidget *solve_button;
    GtkWidget *solver_text;
} Widgets;

// Relaciona el id del .glade con el campo de Widgets que lo guarda
typedef struct {
    const char *id;
    GtkWidget **target;
} WidgetBinding;

static Widgets widgets;

// =============================================
// Helpers
// ---------------------------------------------

// Muestra un dialogo modal de error con el mensaje recibido
static void show_error_dialog(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Devuelve la carpeta del ejecutable, o NULL si no se pudo resolver (liberar con g_free)
static gchar *get_executable_dir(GError **error) {
    gchar *executable_path = g_file_read_link(EXECUTABLE_LINK, error);
    if (executable_path == NULL) {
        return NULL;
    }
    gchar *executable_dir = g_path_get_dirname(executable_path);
    g_free(executable_path);
    return executable_dir;
}

// Reemplaza el contenido de un GtkTextView
static void set_text_view_text(GtkWidget *text_view, const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, text, -1);
}

// =============================================
// Signal handlers
// ---------------------------------------------

// Termina el programa al cerrar la ventana
static void on_window_destroy(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    gtk_main_quit();
}

// Interpreta el numero escrito y muestra su valor exacto o el error
static void on_number_changed(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(widgets.number_entry));
    Fraction value;
    char value_text[FRACTION_STRING_SIZE];

    FractionStatus status = fraction_parse(text, &value);
    if (status != FRACTION_OK) {
        gtk_label_set_text(GTK_LABEL(widgets.number_result), fraction_status_message(status));
        return;
    }

    fraction_to_string(value, value_text, FRACTION_STRING_SIZE);
    gchar *message = g_strdup_printf("Valor exacto: %s", value_text);
    gtk_label_set_text(GTK_LABEL(widgets.number_result), message);
    g_free(message);
}

// Arma el modelo con lo escrito en la pestana, lo valida y muestra el resultado
static void on_model_changed(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    Problem problem;
    char slack_name[NAME_BUFFER_SIZE];
    char validation_message[MESSAGE_BUFFER_SIZE];
    GString *output = g_string_new(NULL);

    // Datos del modelo a partir de los widgets
    problem_init(&problem);
    problem_set_sizes(&problem,
                      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.variables_spin)),
                      gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.constraints_spin)));
    ProblemError name_error = problem_set_name(&problem, gtk_entry_get_text(GTK_ENTRY(widgets.name_entry)));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.negative_rhs_check))) {
        problem.right_hand_sides[0] = fraction_from_int(-1);
    }

    // Nombres escritos por el usuario; los que no se escriben quedan con el nombre por defecto
    const char *names_text = gtk_entry_get_text(GTK_ENTRY(widgets.names_entry));
    ProblemError variable_name_error = PROBLEM_OK;
    if (strlen(names_text) > 0) {
        gchar **parts = g_strsplit(names_text, ",", -1);
        for (int index = 0; parts[index] != NULL && index < MAX_VARIABLES; index++) {
            ProblemError error = problem_set_variable_name(&problem, index, parts[index]);
            if (error != PROBLEM_OK) {
                variable_name_error = error;
            }
        }
        g_strfreev(parts);
    }

    // Resumen del modelo
    g_string_append_printf(output, "Nombre del problema: \"%s\"\n", problem.name);
    g_string_append_printf(output, "Tamaño: %d variables, %d restricciones\n\n", problem.variable_count,
                           problem.constraint_count);
    g_string_append(output, "Variables:");
    for (int variable = 0; variable < problem.variable_count; variable++) {
        g_string_append_printf(output, "  [%s]", problem.variable_names[variable]);
    }
    g_string_append(output, "\nHolguras:");
    for (int slack = 0; slack < problem.constraint_count; slack++) {
        problem_slack_name(slack, slack_name, NAME_BUFFER_SIZE);
        g_string_append_printf(output, "  [%s]", slack_name);
    }

    // Resultado de la validacion, o el error al guardar un nombre demasiado largo
    g_string_append(output, "\n\nValidación: ");
    if (name_error != PROBLEM_OK) {
        g_string_append_printf(output, "%s", problem_error_message(name_error));
    } else if (variable_name_error != PROBLEM_OK) {
        g_string_append_printf(output, "%s", problem_error_message(variable_name_error));
    } else if (problem_validate(&problem, validation_message, sizeof(validation_message)) == PROBLEM_OK) {
        g_string_append(output, "el problema es válido");
    } else {
        g_string_append(output, validation_message);
    }
    g_string_append(output, "\n");

    set_text_view_text(widgets.model_text, output->str);
    g_string_free(output, TRUE);
}

// Resuelve el problema de referencia elegido y muestra todo el historial del motor
static void on_solve_clicked(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    Problem problem;
    GString *output = g_string_new(NULL);

    // Problema de referencia elegido en la lista
    int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.problem_combo));
    if (selected == REFERENCE_WYNDOR) {
        build_wyndor_glass(&problem);
    } else if (selected == REFERENCE_MINIMIZATION) {
        build_minimization_example(&problem);
    } else if (selected == REFERENCE_MULTIPLE) {
        build_class_multiple_solutions(&problem);
    } else if (selected == REFERENCE_DEGENERATE) {
        build_class_degenerate(&problem);
    } else {
        build_dantzig_nightmare(&problem);
    }

    SimplexResult *result = simplex_solve(&problem);
    if (result == NULL) {
        set_text_view_text(widgets.solver_text, "No hay memoria suficiente para resolver el problema.");
        g_string_free(output, TRUE);
        return;
    }

    // Reporte compartido con la ventana principal
    text_report_append(output, &problem, result);
    set_text_view_text(widgets.solver_text, output->str);
    g_string_free(output, TRUE);
    simplex_result_free(result);
}

// =============================================
// Main
// ---------------------------------------------

// Busca cada widget del .glade; devuelve el id del primero que falte, o NULL si estan todos
static const char *bind_widgets(GtkBuilder *builder) {
    WidgetBinding bindings[] = {
        {"number_entry", &widgets.number_entry},
        {"number_result", &widgets.number_result},
        {"name_entry", &widgets.name_entry},
        {"variables_spin", &widgets.variables_spin},
        {"constraints_spin", &widgets.constraints_spin},
        {"names_entry", &widgets.names_entry},
        {"negative_rhs_check", &widgets.negative_rhs_check},
        {"model_text", &widgets.model_text},
        {"problem_combo", &widgets.problem_combo},
        {"solve_button", &widgets.solve_button},
        {"solver_text", &widgets.solver_text},
    };
    int binding_count = (int)(sizeof(bindings) / sizeof(bindings[0]));

    for (int index = 0; index < binding_count; index++) {
        *bindings[index].target = GTK_WIDGET(gtk_builder_get_object(builder, bindings[index].id));
        if (*bindings[index].target == NULL) {
            return bindings[index].id;
        }
    }
    return NULL;
}

// Carga la ventana de prueba, conecta las senales y corre el bucle de GTK
int main(int argc, char *argv[]) {
    GError *error = NULL;
    gtk_init(&argc, &argv);

    // Localizar el .glade junto al ejecutable
    gchar *executable_dir = get_executable_dir(&error);
    if (executable_dir == NULL) {
        gchar *message = g_strdup_printf("No se pudo ubicar la carpeta del programa: %s", error->message);
        show_error_dialog(message);
        g_free(message);
        g_error_free(error);
        return 1;
    }
    gchar *glade_path = g_build_filename(executable_dir, GLADE_RELATIVE_PATH, NULL);
    g_free(executable_dir);

    // Cargar la interfaz
    GtkBuilder *builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(builder, glade_path, &error)) {
        gchar *message = g_strdup_printf("No se pudo cargar '%s': %s", glade_path, error->message);
        show_error_dialog(message);
        g_free(message);
        g_error_free(error);
        g_free(glade_path);
        g_object_unref(builder);
        return 1;
    }
    g_free(glade_path);

    // Ventana y widgets: si falta alguno se avisa cual
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, WINDOW_ID));
    const char *missing_id = bind_widgets(builder);
    if (window == NULL || missing_id != NULL) {
        const char *absent_id = WINDOW_ID;
        if (missing_id != NULL) {
            absent_id = missing_id;
        }
        gchar *message = g_strdup_printf("Al archivo de la interfaz le falta el widget '%s'.", absent_id);
        show_error_dialog(message);
        g_free(message);
        g_object_unref(builder);
        return 1;
    }
    g_object_unref(builder);

    // Senales
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    g_signal_connect(widgets.number_entry, "changed", G_CALLBACK(on_number_changed), NULL);
    g_signal_connect(widgets.name_entry, "changed", G_CALLBACK(on_model_changed), NULL);
    g_signal_connect(widgets.names_entry, "changed", G_CALLBACK(on_model_changed), NULL);
    g_signal_connect(widgets.variables_spin, "value-changed", G_CALLBACK(on_model_changed), NULL);
    g_signal_connect(widgets.constraints_spin, "value-changed", G_CALLBACK(on_model_changed), NULL);
    g_signal_connect(widgets.negative_rhs_check, "toggled", G_CALLBACK(on_model_changed), NULL);
    g_signal_connect(widgets.solve_button, "clicked", G_CALLBACK(on_solve_clicked), NULL);

    // Mostrar un estado inicial en cada pestana
    on_number_changed(NULL, NULL);
    on_model_changed(NULL, NULL);
    on_solve_clicked(NULL, NULL);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
