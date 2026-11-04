#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main_window.h"
#include "problem.h"

// =============================================
// Constants
// ---------------------------------------------
// Misma disposicion de la cuadricula que main_window.c
#define FIRST_VARIABLE_COLUMN 1
#define RIGHT_HAND_SIDE_COLUMN (FIRST_VARIABLE_COLUMN + MAX_VARIABLES + 1)
#define NAMES_ROW 0
#define OBJECTIVE_ROW 1
#define FIRST_CONSTRAINT_ROW 2
#define DIALOG_CLOSER_MS 30

// =============================================
// State
// ---------------------------------------------
static GtkBuilder *builder = NULL;
static GtkWidget *model_grid = NULL;
static GtkWidget *solve_button = NULL;
static GtkWidget *variables_spin = NULL;
static GtkWidget *constraints_spin = NULL;
static GtkWidget *intermediate_check = NULL;
static GtkWidget *problem_name_entry = NULL;
static int dialogs_closed = 0;
static int checks_run = 0;
static int checks_failed = 0;
static gchar *output_dir = NULL;

// =============================================
// Helpers
// ---------------------------------------------

// Registra el resultado de una comprobacion
static void check(int condition, const char *description) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s\n", description);
    }
}

// Cierra cualquier dialogo de mensaje abierto, como si el usuario presionara Cerrar
static gboolean close_open_dialogs(gpointer user_data G_GNUC_UNUSED) {
    GList *toplevels = gtk_window_list_toplevels();
    for (GList *item = toplevels; item != NULL; item = item->next) {
        if (GTK_IS_MESSAGE_DIALOG(item->data) && gtk_widget_get_visible(GTK_WIDGET(item->data))) {
            gtk_dialog_response(GTK_DIALOG(item->data), GTK_RESPONSE_CLOSE);
            dialogs_closed++;
        }
    }
    g_list_free(toplevels);
    return G_SOURCE_CONTINUE;
}

// Campo de la cuadricula por columna y fila
static GtkWidget *grid_entry(int column, int row) {
    return gtk_grid_get_child_at(GTK_GRID(model_grid), column, row);
}

// Escribe texto en un campo de la cuadricula
static void set_entry(int column, int row, const char *text) {
    gtk_entry_set_text(GTK_ENTRY(grid_entry(column, row)), text);
}

// Cambia las cantidades como lo haria el usuario con los selectores
static void set_sizes(int variable_count, int constraint_count) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(variables_spin), variable_count);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(constraints_spin), constraint_count);
}

// Llena todo el tamano indicado con datos validos: coeficientes 1..9 y lado derecho 50
static void fill_valid_data(int variable_count, int constraint_count) {
    char text[32];

    for (int variable = 0; variable < variable_count; variable++) {
        snprintf(text, sizeof(text), "%d", 1 + (variable % 9));
        set_entry(FIRST_VARIABLE_COLUMN + variable, OBJECTIVE_ROW, text);
    }
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        for (int variable = 0; variable < variable_count; variable++) {
            snprintf(text, sizeof(text), "%d", 1 + ((variable + constraint) % 9));
            set_entry(FIRST_VARIABLE_COLUMN + variable, FIRST_CONSTRAINT_ROW + constraint, text);
        }
        set_entry(RIGHT_HAND_SIDE_COLUMN, FIRST_CONSTRAINT_ROW + constraint, "50");
    }
}

// Presiona Resolver y devuelve cuantos dialogos se cerraron durante ese clic
static int click_solve(void) {
    int before = dialogs_closed;
    gtk_button_clicked(GTK_BUTTON(solve_button));
    return dialogs_closed - before;
}

// Devuelve 1 si el archivo existe
static int file_exists(const char *name) {
    gchar *path = g_build_filename(output_dir, name, NULL);
    int exists = g_file_test(path, G_FILE_TEST_EXISTS);
    g_free(path);
    return exists;
}

// =============================================
// Scenarios
// ---------------------------------------------

// Todo en cero por defecto: debe resolverse sin errores
static void test_default_form(void) {
    gtk_entry_set_text(GTK_ENTRY(problem_name_entry), "Default form");
    check(click_solve() == 0, "default 2x2 form solves without dialogs");
    check(file_exists("Default_form.pdf"), "default form produces a pdf");
}

// Entradas invalidas: cada una debe mostrar exactamente un dialogo y no cerrar el programa
static void test_invalid_inputs(void) {
    set_sizes(2, 2);
    fill_valid_data(2, 2);
    const char *bad_values[] = {"abc", "1.5abc", "--3", "1/0", "99999999999999999999", "1e5", "1..2", ".", "/", "1/"};
    int bad_count = (int)(sizeof(bad_values) / sizeof(bad_values[0]));

    // Cada valor malo en un coeficiente
    for (int index = 0; index < bad_count; index++) {
        set_entry(FIRST_VARIABLE_COLUMN, FIRST_CONSTRAINT_ROW, bad_values[index]);
        check(click_solve() == 1, bad_values[index]);
    }
    fill_valid_data(2, 2);

    // Lado derecho negativo
    set_entry(RIGHT_HAND_SIDE_COLUMN, FIRST_CONSTRAINT_ROW, "-1");
    check(click_solve() == 1, "negative right hand side rejected");
    set_entry(RIGHT_HAND_SIDE_COLUMN, FIRST_CONSTRAINT_ROW, "50");

    // Nombre de variable vacio y repetido
    set_entry(FIRST_VARIABLE_COLUMN, NAMES_ROW, "");
    check(click_solve() == 1, "empty variable name rejected");
    set_entry(FIRST_VARIABLE_COLUMN, NAMES_ROW, "$a$");
    set_entry(FIRST_VARIABLE_COLUMN + 1, NAMES_ROW, "$a$");
    check(click_solve() == 1, "duplicate variable name rejected");
    set_entry(FIRST_VARIABLE_COLUMN, NAMES_ROW, "$x_1$");
    set_entry(FIRST_VARIABLE_COLUMN + 1, NAMES_ROW, "$x_2$");

    // Nombre de variable igual a una holgura
    set_entry(FIRST_VARIABLE_COLUMN, NAMES_ROW, "$s_1$");
    check(click_solve() == 1, "slack name conflict rejected");
    set_entry(FIRST_VARIABLE_COLUMN, NAMES_ROW, "$x_1$");

    // Con todo corregido vuelve a funcionar
    check(click_solve() == 0, "valid again after errors");
}

// Combinaciones de tamano resolviendo varias veces seguidas, sin reiniciar
static void test_size_changes(void) {
    int sizes[][2] = {{15, 15}, {2, 2}, {2, 15}, {15, 2}, {7, 3}, {15, 15}, {2, 2}};
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));
    char name[64];

    for (int index = 0; index < size_count; index++) {
        set_sizes(sizes[index][0], sizes[index][1]);
        fill_valid_data(sizes[index][0], sizes[index][1]);
        snprintf(name, sizeof(name), "Sizes %d", index);
        gtk_entry_set_text(GTK_ENTRY(problem_name_entry), name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(intermediate_check), index % 2);
        snprintf(name, sizeof(name), "size %dx%d solves", sizes[index][0], sizes[index][1]);
        check(click_solve() == 0, name);
    }
}

// Clics repetidos sobre Resolver sin cambiar nada
static void test_repeated_clicks(void) {
    set_sizes(3, 3);
    fill_valid_data(3, 3);
    gtk_entry_set_text(GTK_ENTRY(problem_name_entry), "Repeated clicks");
    for (int index = 0; index < 10; index++) {
        check(click_solve() == 0, "repeated click solves");
    }
}

// Nombres del problema vacios o con simbolos: deben generar un archivo seguro
static void test_problem_names(void) {
    const char *names[] = {"", "A & B", "100% #1 $5", "../../etc", "   ", "Ñandú"};
    int name_count = (int)(sizeof(names) / sizeof(names[0]));

    set_sizes(2, 2);
    fill_valid_data(2, 2);
    for (int index = 0; index < name_count; index++) {
        gtk_entry_set_text(GTK_ENTRY(problem_name_entry), names[index]);
        check(click_solve() <= 1, names[index]);
    }
}

// =============================================
// Main
// ---------------------------------------------
int main(int argc, char **argv) {
    GError *error = NULL;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <main_window.glade> <output directory>\n", argv[0]);
        return 2;
    }
    gtk_init(&argc, &argv);
    output_dir = g_strdup(argv[2]);

    // Cargar la ventana real, igual que main.c
    builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(builder, argv[1], &error)) {
        fprintf(stderr, "Cannot load glade: %s\n", error->message);
        return 2;
    }
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    const char *missing = main_window_setup(builder, window, output_dir);
    if (missing != NULL) {
        fprintf(stderr, "Missing widget: %s\n", missing);
        return 2;
    }
    model_grid = GTK_WIDGET(gtk_builder_get_object(builder, "model_grid"));
    solve_button = GTK_WIDGET(gtk_builder_get_object(builder, "solve_button"));
    variables_spin = GTK_WIDGET(gtk_builder_get_object(builder, "variables_spin"));
    constraints_spin = GTK_WIDGET(gtk_builder_get_object(builder, "constraints_spin"));
    intermediate_check = GTK_WIDGET(gtk_builder_get_object(builder, "intermediate_check"));
    problem_name_entry = GTK_WIDGET(gtk_builder_get_object(builder, "problem_name_entry"));
    gtk_widget_show_all(window);

    // El cerrador de dialogos corre dentro del bucle anidado de gtk_dialog_run
    g_timeout_add(DIALOG_CLOSER_MS, close_open_dialogs, NULL);

    test_default_form();
    test_invalid_inputs();
    test_size_changes();
    test_repeated_clicks();
    test_problem_names();

    printf("%d checks run, %d failed, %d dialogs closed\n", checks_run, checks_failed, dialogs_closed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
