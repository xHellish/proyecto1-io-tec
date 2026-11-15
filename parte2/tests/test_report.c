#include <glib.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fraction.h"
#include "latex_report.h"
#include "launcher.h"
#include "problem.h"
#include "reference_problems.h"
#include "simplex.h"

// =============================================
// Constants
// ---------------------------------------------
#define MAX_RANDOM_VALUE 9
#define RANDOM_SEED 12345

// =============================================
// State
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;
static const char *output_dir = NULL;

// =============================================
// Helpers
// ---------------------------------------------

// Registra el resultado de una comprobacion
static void check(int condition, const char *description, const char *detail) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s (%s)\n", description, detail);
    }
}

// Asigna un entero a una fraccion
static void set_int(Fraction *target, long long value) {
    fraction_make(value, 1, target);
}

// Llena un problema con datos enteros pequenos reproducibles; los lados derechos son positivos
static void fill_random_problem(Problem *problem, int variable_count, int constraint_count, ObjectiveType objective) {
    problem_init(problem);
    problem_set_name(problem, "Random");
    problem_set_sizes(problem, variable_count, constraint_count);
    problem->objective_type = objective;
    for (int variable = 0; variable < variable_count; variable++) {
        set_int(&problem->objective[variable], 1 + rand() % MAX_RANDOM_VALUE);
    }
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        for (int variable = 0; variable < variable_count; variable++) {
            set_int(&problem->coefficients[constraint][variable], 1 + rand() % MAX_RANDOM_VALUE);
        }
        set_int(&problem->right_hand_sides[constraint], 10 + rand() % 90);
    }
}

// Valida, resuelve, escribe el .tex y lo compila; devuelve 1 si todo el recorrido sale bien.
// Si algo falla deja el motivo en message, igual que la interfaz
static int run_pipeline(const Problem *problem, int show_intermediate, const char *file_name, char *message,
                        size_t message_size) {
    int is_ok = 0;
    gchar *tex_name = g_strdup_printf("%s.tex", file_name);
    gchar *tex_path = g_build_filename(output_dir, tex_name, NULL);

    // Mismas etapas que on_solve_clicked: validar, resolver, escribir y compilar
    if (problem_validate(problem, message, message_size) == PROBLEM_OK) {
        SimplexResult *result = simplex_solve(problem);
        if (result == NULL) {
            snprintf(message, message_size, "out of memory");
        } else {
            if (launcher_prepare_output_dir(output_dir, message, message_size) &&
                latex_report_write(tex_path, problem, result, show_intermediate, message, message_size) &&
                launcher_compile(output_dir, tex_name, message, message_size)) {
                is_ok = 1;
            }
            simplex_result_free(result);
        }
    }

    g_free(tex_name);
    g_free(tex_path);
    return is_ok;
}

// Corre un problema que debe compilar, con y sin tablas intermedias
static void expect_compiles(const Problem *problem, const char *base_name) {
    char message[MESSAGE_BUFFER_SIZE] = "";
    char file_name[128];

    for (int intermediate = 0; intermediate < 2; intermediate++) {
        const char *suffix = "short";
        if (intermediate == 1) {
            suffix = "full";
        }
        snprintf(file_name, sizeof(file_name), "%s_%s", base_name, suffix);
        int is_ok = run_pipeline(problem, intermediate, file_name, message, sizeof(message));
        check(is_ok, file_name, message);
    }
}

// =============================================
// Scenarios
// ---------------------------------------------

// Problemas de referencia de clase
static void test_reference_problems(void) {
    Problem problem;

    build_dantzig_nightmare(&problem);
    expect_compiles(&problem, "ref_dantzig");
    build_wyndor_glass(&problem);
    expect_compiles(&problem, "ref_wyndor");
    build_minimization_example(&problem);
    expect_compiles(&problem, "ref_minimization");
    build_class_multiple_solutions(&problem);
    expect_compiles(&problem, "ref_multiple");
    build_class_degenerate(&problem);
    expect_compiles(&problem, "ref_degenerate");
    build_class_notes_maximization(&problem);
    expect_compiles(&problem, "ref_class_notes");
    build_dakota(&problem);
    expect_compiles(&problem, "ref_dakota");
    build_dakota_multiple(&problem);
    expect_compiles(&problem, "ref_dakota_multiple");
    build_class_four_fourteen(&problem);
    expect_compiles(&problem, "ref_four_fourteen");
    build_class_minimization(&problem);
    expect_compiles(&problem, "ref_class_minimization");
    build_breadco_unbounded(&problem);
    expect_compiles(&problem, "ref_breadco");
}

// Devuelve 1 si el .tex generado con ese nombre contiene el texto
static int tex_contains(const char *file_name, const char *text) {
    gchar *path = g_strdup_printf("%s/%s.tex", output_dir, file_name);
    gchar *contents = NULL;
    int has_text = 0;

    if (g_file_get_contents(path, &contents, NULL, NULL)) {
        has_text = (strstr(contents, text) != NULL);
        g_free(contents);
    }
    g_free(path);
    return has_text;
}

// Llena un problema de 3 variables con una restriccion por fila: coeficientes, relacion y lado derecho
static void build_three_variable_problem(Problem *problem, ObjectiveType objective_type, int constraint_count,
                                         const int coefficients[][3], const ConstraintType types[],
                                         const int right_hand_sides[]) {
    problem_init(problem);
    problem_set_name(problem, "Three variables");
    problem_set_sizes(problem, 3, constraint_count);
    problem->objective_type = objective_type;
    for (int variable = 0; variable < 3; variable++) {
        set_int(&problem->objective[variable], 1 + variable);
    }
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        for (int variable = 0; variable < 3; variable++) {
            set_int(&problem->coefficients[constraint][variable], coefficients[constraint][variable]);
        }
        problem->constraint_types[constraint] = types[constraint];
        set_int(&problem->right_hand_sides[constraint], right_hand_sides[constraint]);
    }
}

// Graficos de los extras: el 2D solo con 2 variables y el cuerpo 3D solo con 3
static void test_plot_pages(void) {
    Problem problem;

    build_wyndor_glass(&problem);
    expect_compiles(&problem, "plot_two_variables");
    check(tex_contains("plot_two_variables_short", "Gráfico de la región factible"), "plot_2d_present",
          "2 variables must have the region plot");
    check(!tex_contains("plot_two_variables_short", "Cuerpo Símplex tridimensional"), "plot_3d_absent_in_2d",
          "2 variables must not have the 3D body");

    build_dakota(&problem);
    expect_compiles(&problem, "plot_three_variables");
    check(tex_contains("plot_three_variables_short", "Cuerpo Símplex tridimensional"), "plot_3d_present",
          "3 variables must have the 3D body");
    check(!tex_contains("plot_three_variables_short", "Gráfico de la región factible"), "plot_2d_absent_in_3d",
          "3 variables must not have the 2D plot");

    build_breadco_unbounded(&problem);
    expect_compiles(&problem, "plot_four_variables");
    check(!tex_contains("plot_four_variables_short", "Cuerpo Símplex tridimensional") &&
              !tex_contains("plot_four_variables_short", "Gráfico de la región factible"),
          "plots_absent_in_4d", "4 variables must have no plots");
}

// Cuerpos de 3 variables de todos los tipos: cubo, caso con >= y =, plano, no acotado, no factible y soluciones multiples
static void test_three_variable_bodies(void) {
    Problem problem;
    const int cube[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const ConstraintType cube_types[] = {CONSTRAINT_LESS_EQUAL, CONSTRAINT_LESS_EQUAL, CONSTRAINT_LESS_EQUAL};
    const int cube_limits[] = {4, 5, 6};
    const int mixed[][3] = {{1, 1, 1}, {1, 2, 0}, {0, 1, 3}};
    const ConstraintType mixed_types[] = {CONSTRAINT_LESS_EQUAL, CONSTRAINT_GREATER_EQUAL, CONSTRAINT_EQUAL};
    const int mixed_limits[] = {12, 2, 6};
    const int open_rows[][3] = {{1, -1, 0}, {0, 1, -1}, {0, 0, 1}};
    const ConstraintType open_types[] = {CONSTRAINT_GREATER_EQUAL, CONSTRAINT_GREATER_EQUAL, CONSTRAINT_GREATER_EQUAL};
    const int open_limits[] = {1, 1, 2};
    const int empty_rows[][3] = {{1, 1, 1}, {1, 1, 1}};
    const ConstraintType empty_types[] = {CONSTRAINT_LESS_EQUAL, CONSTRAINT_GREATER_EQUAL};
    const int empty_limits[] = {2, 9};
    const int flat_rows[][3] = {{1, 1, 1}, {1, 0, 0}};
    const ConstraintType flat_types[] = {CONSTRAINT_EQUAL, CONSTRAINT_LESS_EQUAL};
    const int flat_limits[] = {6, 4};
    const int tie_rows[][3] = {{1, 2, 3}, {1, 0, 0}, {0, 1, 0}};
    const ConstraintType tie_types[] = {CONSTRAINT_LESS_EQUAL, CONSTRAINT_LESS_EQUAL, CONSTRAINT_LESS_EQUAL};
    const int tie_limits[] = {12, 4, 3};

    build_three_variable_problem(&problem, OBJECTIVE_MAXIMIZE, 3, cube, cube_types, cube_limits);
    expect_compiles(&problem, "body_box");
    build_three_variable_problem(&problem, OBJECTIVE_MINIMIZE, 3, mixed, mixed_types, mixed_limits);
    expect_compiles(&problem, "body_mixed");
    build_three_variable_problem(&problem, OBJECTIVE_MAXIMIZE, 3, open_rows, open_types, open_limits);
    expect_compiles(&problem, "body_unbounded");
    build_three_variable_problem(&problem, OBJECTIVE_MAXIMIZE, 2, empty_rows, empty_types, empty_limits);
    expect_compiles(&problem, "body_infeasible");
    build_three_variable_problem(&problem, OBJECTIVE_MAXIMIZE, 2, flat_rows, flat_types, flat_limits);
    expect_compiles(&problem, "body_plane");
    build_three_variable_problem(&problem, OBJECTIVE_MAXIMIZE, 3, tie_rows, tie_types, tie_limits);
    expect_compiles(&problem, "body_multiple");
}

// Cada ejemplo del menu de la ventana (incluidos los de la Gran M, el no factible y el no acotado) compila
static void test_sample_problems(void) {
    Problem problem;
    char name[64];

    for (int index = 0; index < sample_problem_count(); index++) {
        const SampleProblem *sample = sample_problem_get(index);
        snprintf(name, sizeof(name), "sample_%02d", index);
        check(build_problem_from_sample(sample, &problem), name, "the sample cannot be read");
        expect_compiles(&problem, name);
    }
}

// Problema no acotado: maximizar x1 + x2 con x1 - x2 <= 1 y x1 - 2x2 <= 2 (x1 puede crecer sin limite)
static void test_unbounded(void) {
    Problem problem;

    problem_init(&problem);
    problem_set_name(&problem, "Unbounded");
    set_int(&problem.objective[0], 1);
    set_int(&problem.objective[1], 1);
    set_int(&problem.coefficients[0][0], 1);
    set_int(&problem.coefficients[0][1], -1);
    set_int(&problem.right_hand_sides[0], 1);
    set_int(&problem.coefficients[1][0], 1);
    set_int(&problem.coefficients[1][1], -2);
    set_int(&problem.right_hand_sides[1], 2);
    expect_compiles(&problem, "case_unbounded");

    // Verificar que el motor lo clasifica como no acotado
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_UNBOUNDED, "case_unbounded status", "expected unbounded");
    simplex_result_free(result);
}

// Tamanos minimo y maximo en todas las combinaciones, maximizando y minimizando
static void test_sizes(void) {
    int sizes[][2] = {{2, 2}, {15, 15}, {2, 15}, {15, 2}};
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));
    Problem problem;
    char name[64];

    srand(RANDOM_SEED);
    for (int index = 0; index < size_count; index++) {
        for (int objective = 0; objective < 2; objective++) {
            ObjectiveType objective_type = OBJECTIVE_MAXIMIZE;
            const char *objective_label = "max";
            if (objective == 1) {
                objective_type = OBJECTIVE_MINIMIZE;
                objective_label = "min";
            }
            fill_random_problem(&problem, sizes[index][0], sizes[index][1], objective_type);
            snprintf(name, sizeof(name), "size_%dx%d_%s", sizes[index][0], sizes[index][1], objective_label);
            expect_compiles(&problem, name);
        }
    }
}

// Tamanos extremos con restricciones >= o = (todas, y mezcladas), maximizando y minimizando
static void test_big_m_sizes(void) {
    int sizes[][2] = {{2, 2}, {15, 15}, {2, 15}, {15, 2}};
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));
    Problem problem;
    char name[64];

    srand(RANDOM_SEED);
    for (int index = 0; index < size_count; index++) {
        for (int mixed = 0; mixed < 2; mixed++) {
            for (int objective = 0; objective < 2; objective++) {
                fill_random_problem(&problem, sizes[index][0], sizes[index][1], (ObjectiveType)objective);

                // Todas >= o una mezcla de <=, >= y =
                for (int constraint = 0; constraint < sizes[index][1]; constraint++) {
                    if (mixed == 0) {
                        problem.constraint_types[constraint] = CONSTRAINT_GREATER_EQUAL;
                    } else {
                        problem.constraint_types[constraint] = (ConstraintType)(constraint % 3);
                    }
                }
                const char *relation_label = "ge";
                if (mixed == 1) {
                    relation_label = "mix";
                }
                snprintf(name, sizeof(name), "bigm_%dx%d_%s_%d", sizes[index][0], sizes[index][1], relation_label,
                         objective);
                expect_compiles(&problem, name);
            }
        }
    }
}

// Nombres del problema con caracteres especiales de LaTeX: deben compilar
static void test_problem_names(void) {
    const char *names[] = {"A & B", "100% real", "Cost $5", "Item #1", "snake_case", "{braces}", "tilde ~ hat ^",
                           "back\\slash", "Ñandú árbol"};
    int name_count = (int)(sizeof(names) / sizeof(names[0]));
    Problem problem;
    char file_name[64];

    for (int index = 0; index < name_count; index++) {
        build_wyndor_glass(&problem);
        problem_set_name(&problem, names[index]);
        snprintf(file_name, sizeof(file_name), "name_%d", index);
        expect_compiles(&problem, file_name);
    }
}

// Nombres de variables rotos: pueden rechazarse o compilar, pero nunca colgar ni fallar el programa
static void test_variable_names(void) {
    const char *names[] = {"$x_", "}{", "\\", "%", "&", "_", "$$", "\\frac", "x^", "$\\alpha$", "a b"};
    int name_count = (int)(sizeof(names) / sizeof(names[0]));
    Problem problem;
    char message[MESSAGE_BUFFER_SIZE];
    char file_name[64];

    for (int index = 0; index < name_count; index++) {
        build_wyndor_glass(&problem);
        problem_set_variable_name(&problem, 0, names[index]);
        snprintf(file_name, sizeof(file_name), "varname_%d", index);
        int is_ok = run_pipeline(&problem, 1, file_name, message, sizeof(message));
        if (is_ok) {
            printf("  variable name '%s': compiled\n", names[index]);
        } else {
            printf("  variable name '%s': rejected: %s\n", names[index], message);
        }
        check(1, file_name, "no crash");
    }
}

// Numeros enormes y datos que desbordan: la lectura o el motor deben informar el error
static void test_huge_numbers(void) {
    Fraction value;
    Problem problem;
    char message[MESSAGE_BUFFER_SIZE];

    // Lectura de texto
    check(fraction_parse("99999999999999999999", &value) != FRACTION_OK, "huge integer rejected", "parse");
    check(fraction_parse("1e999", &value) != FRACTION_OK, "exponent rejected", "parse");
    check(fraction_parse("0.0000000000000000000001", &value) != FRACTION_OK, "tiny decimal rejected", "parse");
    check(fraction_parse("abc", &value) != FRACTION_OK, "letters rejected", "parse");
    check(fraction_parse("1,5", &value) == FRACTION_OK, "comma decimal accepted", "parse");
    check(fraction_parse("1.5", &value) == FRACTION_OK, "dot decimal accepted", "parse");
    check(fraction_parse("1.5abc", &value) != FRACTION_OK, "trailing garbage rejected", "parse");

    // Valores validos pero enormes: el motor debe terminar con un estado, no con un fallo
    build_wyndor_glass(&problem);
    fraction_make(4000000000000000000LL, 1, &problem.coefficients[0][0]);
    fraction_make(3000000000000000000LL, 7, &problem.coefficients[1][1]);
    int is_ok = run_pipeline(&problem, 1, "huge_values", message, sizeof(message));
    if (is_ok) {
        printf("  huge values: compiled\n");
    } else {
        printf("  huge values: reported: %s\n", message);
    }
    check(1, "huge_values", "no crash");
}

// Fallos del entorno: sin pdflatex, sin visor y sin permiso de escritura deben dar un error claro
static void test_environment_failures(void) {
    char message[MESSAGE_BUFFER_SIZE];
    Problem problem;
    char *original_path = g_strdup(g_getenv("PATH"));

    // Carpeta de salida imposible de crear
    message[0] = '\0';
    check(launcher_prepare_output_dir("/proc/no_permission/output", message, sizeof(message)) == 0,
          "unwritable output dir rejected", "prepare_output_dir");
    check(strlen(message) > 0, "unwritable output dir has message", "prepare_output_dir");

    // Sin pdflatex ni visor en el PATH
    g_setenv("PATH", "/nonexistent", TRUE);
    build_wyndor_glass(&problem);
    message[0] = '\0';
    check(launcher_compile(output_dir, "ref_wyndor_short.tex", message, sizeof(message)) == 0, "missing pdflatex rejected",
          "compile");
    check(strlen(message) > 0, "missing pdflatex has message", "compile");
    message[0] = '\0';
    check(launcher_open_pdf("/nonexistent.pdf", message, sizeof(message)) == 0, "missing viewer rejected", "open_pdf");
    check(strlen(message) > 0, "missing viewer has message", "open_pdf");
    g_setenv("PATH", original_path, TRUE);
    g_free(original_path);

    // Archivo .tex que no se puede escribir
    SimplexResult *result = simplex_solve(&problem);
    message[0] = '\0';
    check(latex_report_write("/proc/no_permission/file.tex", &problem, result, 0, message, sizeof(message)) == 0,
          "unwritable tex rejected", "report_write");
    check(strlen(message) > 0, "unwritable tex has message", "report_write");
    simplex_result_free(result);
}

// =============================================
// Main
// ---------------------------------------------
int main(int argc, char **argv) {
    // Igual que la ventana, usa el idioma del sistema: un decimal con coma rompería el LaTeX
    setlocale(LC_ALL, "");
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    output_dir = argv[1];

    test_reference_problems();
    test_unbounded();
    test_sample_problems();
    test_plot_pages();
    test_three_variable_bodies();
    test_sizes();
    test_big_m_sizes();
    test_problem_names();
    test_variable_names();
    test_huge_numbers();
    test_environment_failures();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
