#include "main_window.h"

#include <stdio.h>

#include "fraction.h"
#include "problem.h"
#include "simplex.h"
#include "text_report.h"

// =============================================
// Constants
// ---------------------------------------------
// Columnas de la cuadricula: etiqueta, variables, relacion y lado derecho
#define LABEL_GRID_COLUMN 0
#define FIRST_VARIABLE_GRID_COLUMN 1
#define RELATION_GRID_COLUMN (FIRST_VARIABLE_GRID_COLUMN + MAX_VARIABLES)
#define RIGHT_HAND_SIDE_GRID_COLUMN (RELATION_GRID_COLUMN + 1)

// Filas de la cuadricula: nombres, funcion objetivo y restricciones
#define NAMES_GRID_ROW 0
#define OBJECTIVE_GRID_ROW 1
#define FIRST_CONSTRAINT_GRID_ROW 2

// Ancho en caracteres de cada campo de la cuadricula
#define ENTRY_WIDTH_CHARS 7

// Posicion de "Minimizar" en la lista del objetivo
#define OBJECTIVE_MINIMIZE_INDEX 1

// =============================================
// Widgets
// ---------------------------------------------
typedef struct {
    GtkWidget *window;
    GtkWidget *problem_name_entry;
    GtkWidget *objective_combo;
    GtkWidget *variables_spin;
    GtkWidget *constraints_spin;
    GtkWidget *model_grid;
    GtkWidget *solve_button;
    GtkWidget *status_label;
    GtkWidget *results_text;

    // Cuadricula creada una sola vez en su tamano maximo; solo se muestra la parte activa
    GtkWidget *name_entries[MAX_VARIABLES];
    GtkWidget *objective_entries[MAX_VARIABLES];
    GtkWidget *coefficient_entries[MAX_CONSTRAINTS][MAX_VARIABLES];
    GtkWidget *constraint_labels[MAX_CONSTRAINTS];
    GtkWidget *relation_labels[MAX_CONSTRAINTS];
    GtkWidget *right_hand_side_entries[MAX_CONSTRAINTS];
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

// Muestra un dialogo de error sobre la ventana principal
static void show_error_dialog(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widgets.window), GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Reemplaza el contenido de un GtkTextView
static void set_text_view_text(GtkWidget *text_view, const char *text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, text, -1);
}

// Crea un campo de texto de la cuadricula; su visibilidad la controla update_sizes
static GtkWidget *create_grid_entry(int grid_column, int grid_row, const char *placeholder) {
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), ENTRY_WIDTH_CHARS);
    gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_NAME_LENGTH - 1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    gtk_widget_set_no_show_all(entry, TRUE);
    gtk_grid_attach(GTK_GRID(widgets.model_grid), entry, grid_column, grid_row, 1, 1);
    return entry;
}

// Crea una etiqueta de la cuadricula; su visibilidad la controla update_sizes si no_show_all es verdadero
static GtkWidget *create_grid_label(int grid_column, int grid_row, const char *text, gboolean no_show_all) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_widget_set_no_show_all(label, no_show_all);
    gtk_grid_attach(GTK_GRID(widgets.model_grid), label, grid_column, grid_row, 1, 1);
    return label;
}

// Crea todos los campos de la cuadricula en su tamano maximo (15 x 15)
static void build_model_grid(void) {
    char text[NAME_BUFFER_SIZE];

    // Encabezados fijos de la primera columna y del lado derecho
    create_grid_label(LABEL_GRID_COLUMN, NAMES_GRID_ROW, "Variables", FALSE);
    create_grid_label(LABEL_GRID_COLUMN, OBJECTIVE_GRID_ROW, "Z =", FALSE);
    create_grid_label(RIGHT_HAND_SIDE_GRID_COLUMN, NAMES_GRID_ROW, "Lado derecho", FALSE);

    // Nombres de las variables (editables, con el nombre por defecto) y coeficientes del objetivo
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        int grid_column = FIRST_VARIABLE_GRID_COLUMN + variable;
        problem_default_variable_name(variable, text, NAME_BUFFER_SIZE);
        widgets.name_entries[variable] = create_grid_entry(grid_column, NAMES_GRID_ROW, "");
        gtk_entry_set_text(GTK_ENTRY(widgets.name_entries[variable]), text);
        widgets.objective_entries[variable] = create_grid_entry(grid_column, OBJECTIVE_GRID_ROW, "0");
    }

    // Una fila por restriccion: etiqueta, coeficientes, relacion y lado derecho
    for (int constraint = 0; constraint < MAX_CONSTRAINTS; constraint++) {
        int grid_row = FIRST_CONSTRAINT_GRID_ROW + constraint;
        snprintf(text, NAME_BUFFER_SIZE, "R%d", constraint + 1);
        widgets.constraint_labels[constraint] = create_grid_label(LABEL_GRID_COLUMN, grid_row, text, TRUE);
        for (int variable = 0; variable < MAX_VARIABLES; variable++) {
            widgets.coefficient_entries[constraint][variable] =
                create_grid_entry(FIRST_VARIABLE_GRID_COLUMN + variable, grid_row, "0");
        }
        widgets.relation_labels[constraint] = create_grid_label(RELATION_GRID_COLUMN, grid_row, "≤", TRUE);
        widgets.right_hand_side_entries[constraint] =
            create_grid_entry(RIGHT_HAND_SIDE_GRID_COLUMN, grid_row, "0");
    }
}

// Lee un numero de un campo; vacio vale 0. Si es invalido escribe en message que campo fallo
static int read_number(GtkWidget *entry, const char *field_description, Fraction *value,
                       char *message, size_t message_size) {
    FractionStatus status = fraction_parse(gtk_entry_get_text(GTK_ENTRY(entry)), value);

    // Un campo vacio vale 0, como pide el enunciado
    if (status == FRACTION_ERROR_EMPTY_TEXT) {
        *value = fraction_from_int(0);
        return 1;
    }
    if (status != FRACTION_OK) {
        snprintf(message, message_size, "%s: %s", field_description, fraction_status_message(status));
        gtk_widget_grab_focus(entry);
        return 0;
    }
    return 1;
}

// Arma el problema con lo escrito en la ventana; si algo falla escribe el motivo en message
static int build_problem_from_form(Problem *problem, char *message, size_t message_size) {
    char field_description[MESSAGE_BUFFER_SIZE];

    // Cantidades, nombre y tipo de objetivo
    problem_init(problem);
    int variable_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.variables_spin));
    int constraint_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.constraints_spin));
    ProblemError error = problem_set_sizes(problem, variable_count, constraint_count);
    if (error == PROBLEM_OK) {
        error = problem_set_name(problem, gtk_entry_get_text(GTK_ENTRY(widgets.problem_name_entry)));
    }
    if (error != PROBLEM_OK) {
        snprintf(message, message_size, "%s", problem_error_message(error));
        return 0;
    }
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.objective_combo)) == OBJECTIVE_MINIMIZE_INDEX) {
        problem->objective_type = OBJECTIVE_MINIMIZE;
    }

    // Nombres y coeficientes de la funcion objetivo
    for (int variable = 0; variable < variable_count; variable++) {
        error = problem_set_variable_name(problem, variable,
                                          gtk_entry_get_text(GTK_ENTRY(widgets.name_entries[variable])));
        if (error != PROBLEM_OK) {
            snprintf(message, message_size, "Variable %d: %s", variable + 1, problem_error_message(error));
            return 0;
        }
        snprintf(field_description, sizeof(field_description), "Función objetivo, coeficiente de la variable %d",
                 variable + 1);
        if (!read_number(widgets.objective_entries[variable], field_description, &problem->objective[variable],
                         message, message_size)) {
            return 0;
        }
    }

    // Coeficientes y lado derecho de cada restriccion
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        for (int variable = 0; variable < variable_count; variable++) {
            snprintf(field_description, sizeof(field_description), "Restricción %d, coeficiente de la variable %d",
                     constraint + 1, variable + 1);
            if (!read_number(widgets.coefficient_entries[constraint][variable], field_description,
                             &problem->coefficients[constraint][variable], message, message_size)) {
                return 0;
            }
        }
        snprintf(field_description, sizeof(field_description), "Restricción %d, lado derecho", constraint + 1);
        if (!read_number(widgets.right_hand_side_entries[constraint], field_description,
                         &problem->right_hand_sides[constraint], message, message_size)) {
            return 0;
        }
    }

    // Revision completa del modelo (nombres repetidos, lados derechos negativos, etc.)
    return problem_validate(problem, message, message_size) == PROBLEM_OK;
}

// =============================================
// Signal handlers
// ---------------------------------------------

// Muestra solo las filas y columnas activas; lo escrito en las ocultas se conserva
static void on_sizes_changed(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    int variable_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.variables_spin));
    int constraint_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.constraints_spin));

    // Columnas de variables
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        gboolean is_active = variable < variable_count;
        gtk_widget_set_visible(widgets.name_entries[variable], is_active);
        gtk_widget_set_visible(widgets.objective_entries[variable], is_active);
    }

    // Filas de restricciones (cada celda depende tambien de su columna)
    for (int constraint = 0; constraint < MAX_CONSTRAINTS; constraint++) {
        gboolean row_is_active = constraint < constraint_count;
        gtk_widget_set_visible(widgets.constraint_labels[constraint], row_is_active);
        gtk_widget_set_visible(widgets.relation_labels[constraint], row_is_active);
        gtk_widget_set_visible(widgets.right_hand_side_entries[constraint], row_is_active);
        for (int variable = 0; variable < MAX_VARIABLES; variable++) {
            gboolean cell_is_active = row_is_active && variable < variable_count;
            gtk_widget_set_visible(widgets.coefficient_entries[constraint][variable], cell_is_active);
        }
    }
}

// Lee el problema, lo resuelve y muestra el historial completo
static void on_solve_clicked(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    Problem problem;
    char message[MESSAGE_BUFFER_SIZE];

    // Datos de la ventana; cualquier error se explica en un dialogo
    if (!build_problem_from_form(&problem, message, sizeof(message))) {
        gtk_label_set_text(GTK_LABEL(widgets.status_label), "Revise los datos del problema.");
        show_error_dialog(message);
        return;
    }

    SimplexResult *result = simplex_solve(&problem);
    if (result == NULL) {
        show_error_dialog("No hay memoria suficiente para resolver el problema.");
        return;
    }

    // Reporte del historial en el panel de resultados
    GString *output = g_string_new(NULL);
    text_report_append(output, &problem, result);
    set_text_view_text(widgets.results_text, output->str);
    gtk_label_set_text(GTK_LABEL(widgets.status_label), simplex_status_message(result->status));
    g_string_free(output, TRUE);
    simplex_result_free(result);
}

// =============================================
// Public functions
// ---------------------------------------------

// Toma los widgets del .glade, arma la cuadricula y conecta las senales
const char *main_window_setup(GtkBuilder *builder, GtkWidget *window) {
    WidgetBinding bindings[] = {
        {"problem_name_entry", &widgets.problem_name_entry},
        {"objective_combo", &widgets.objective_combo},
        {"variables_spin", &widgets.variables_spin},
        {"constraints_spin", &widgets.constraints_spin},
        {"model_grid", &widgets.model_grid},
        {"solve_button", &widgets.solve_button},
        {"status_label", &widgets.status_label},
        {"results_text", &widgets.results_text},
    };
    int binding_count = (int)(sizeof(bindings) / sizeof(bindings[0]));

    // Cada widget del .glade debe existir
    widgets.window = window;
    for (int index = 0; index < binding_count; index++) {
        GObject *object = gtk_builder_get_object(builder, bindings[index].id);
        if (object == NULL) {
            return bindings[index].id;
        }
        *bindings[index].target = GTK_WIDGET(object);
    }

    // Cuadricula de coeficientes y estado inicial de 2 x 2
    build_model_grid();
    on_sizes_changed(NULL, NULL);

    // Senales
    g_signal_connect(widgets.variables_spin, "value-changed", G_CALLBACK(on_sizes_changed), NULL);
    g_signal_connect(widgets.constraints_spin, "value-changed", G_CALLBACK(on_sizes_changed), NULL);
    g_signal_connect(widgets.solve_button, "clicked", G_CALLBACK(on_solve_clicked), NULL);
    return NULL;
}
