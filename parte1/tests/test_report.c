#include <glib.h>
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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    output_dir = argv[1];

    test_reference_problems();
    test_unbounded();
    test_sizes();
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
