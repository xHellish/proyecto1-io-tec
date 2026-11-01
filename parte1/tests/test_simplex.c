#include <stdio.h>
#include <string.h>

#include "fraction.h"
#include "problem.h"
#include "simplex.h"

// =============================================
// Test helpers
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;
static int tests_skipped = 0;

// Registra una verificacion y avisa si fallo
static void check(int condition, const char *description) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s\n", description);
    }
}

// Asigna una fraccion de la forma numerador / denominador
static void set_fraction(Fraction *target, long long numerator, long long denominator) {
    FractionStatus status = fraction_make(numerator, denominator, target);
    if (status != FRACTION_OK) {
        printf("Invalid test data: %s\n", fraction_status_message(status));
        checks_failed++;
    }
}

// Compara toda la tabla contra el texto esperado, celda por celda
static void check_table(const Table *table, const char *expected[][8], int row_count, const char *name) {
    char text[FRACTION_STRING_SIZE];
    char description[160];

    check(table->row_count == row_count, name);
    for (int row = 0; row < row_count; row++) {
        for (int column = 0; column < 8; column++) {
            fraction_to_string(table->cells[row][column], text, FRACTION_STRING_SIZE);
            snprintf(description, sizeof(description), "%s, row %d column %d (expected %s, got %s)",
                     name, row, column, expected[row][column], text);
            check(strcmp(text, expected[row][column]) == 0, description);
        }
    }
}

// =============================================
// Reference problems
// ---------------------------------------------

// Pesadilla de Dantzig (tarea de clase): 4 variables, 2 restricciones, lados derechos en 0
static void build_dantzig_nightmare(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Dantzig nightmare");
    problem_set_sizes(problem, 4, 2);

    set_fraction(&problem->objective[0], 2, 1);
    set_fraction(&problem->objective[1], 3, 1);
    set_fraction(&problem->objective[2], -1, 1);
    set_fraction(&problem->objective[3], -12, 1);

    set_fraction(&problem->coefficients[0][0], -2, 1);
    set_fraction(&problem->coefficients[0][1], -9, 1);
    set_fraction(&problem->coefficients[0][2], 1, 1);
    set_fraction(&problem->coefficients[0][3], 9, 1);

    set_fraction(&problem->coefficients[1][0], 1, 3);
    set_fraction(&problem->coefficients[1][1], 1, 1);
    set_fraction(&problem->coefficients[1][2], -1, 3);
    set_fraction(&problem->coefficients[1][3], -2, 1);
}

// =============================================
// Tests that work with the current base
// ---------------------------------------------

// La tabla inicial coincide con la del cuaderno y la columna elegida es x2
static void test_initial_table(void) {
    Problem problem;
    build_dantzig_nightmare(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "solve returns a result");
    if (result == NULL) {
        return;
    }

    const char *expected[3][8] = {
        {"1", "-2", "-3", "1", "12", "0", "0", "0"},
        {"0", "-2", "-9", "1", "9", "1", "0", "0"},
        {"0", "1/3", "1", "-1/3", "-2", "0", "1", "0"},
    };
    check(result->step_count >= 1, "the history has at least one step");
    check_table(&result->steps[0].table, expected, 3, "Dantzig initial table");
    check(result->steps[0].table.column_count == 8, "Dantzig table has 8 columns");
    check(result->steps[0].table.basis[0] == 5 && result->steps[0].table.basis[1] == 6,
          "slack variables start in the basis");
    check(result->steps[0].table.column_types[0] == COLUMN_Z, "column 0 is Z");
    check(result->steps[0].table.column_types[1] == COLUMN_ORIGINAL, "column 1 is original");
    check(result->steps[0].table.column_types[5] == COLUMN_SLACK, "column 5 is a slack");
    check(result->steps[0].table.column_types[7] == COLUMN_RIGHT_HAND_SIDE, "last column is the right-hand side");
    check(result->steps[0].entering_column == 2, "x2 enters first (most negative)");
    simplex_result_free(result);
}

// Con dos columnas empatadas entra la de menor indice
static void test_entering_column_tie(void) {
    Problem problem;
    problem_init(&problem);
    problem_set_name(&problem, "Tie");
    set_fraction(&problem.objective[0], 3, 1);
    set_fraction(&problem.objective[1], 3, 1);
    set_fraction(&problem.coefficients[0][0], 1, 1);
    set_fraction(&problem.coefficients[0][1], 1, 1);
    set_fraction(&problem.right_hand_sides[0], 4, 1);
    set_fraction(&problem.right_hand_sides[1], 6, 1);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "tie problem returns a result");
    if (result == NULL) {
        return;
    }
    check(result->steps[0].entering_column == 1, "tied columns: the lowest index enters");
    simplex_result_free(result);
}

// Problemas que el motor actual rechaza con un estado claro
static void test_rejected_problems(void) {
    Problem problem;
    SimplexResult *result;

    problem_init(&problem);
    problem.objective_type = OBJECTIVE_MINIMIZE;
    result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_ERROR_UNSUPPORTED, "minimize is unsupported for now");
    simplex_result_free(result);

    problem_init(&problem);
    problem.constraint_types[1] = CONSTRAINT_GREATER_EQUAL;
    result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_ERROR_UNSUPPORTED, ">= is unsupported for now");
    simplex_result_free(result);

    problem_init(&problem);
    set_fraction(&problem.right_hand_sides[0], -1, 1);
    result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_ERROR_INVALID_PROBLEM, "negative right-hand side is invalid");
    simplex_result_free(result);

    problem_init(&problem);
    problem.variable_count = 16;
    result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_ERROR_INVALID_PROBLEM, "too many variables is invalid");
    simplex_result_free(result);
}

// =============================================
// Tests that wait for the pending pieces
// ---------------------------------------------

// Camino completo de la tarea: debe terminar en no acotado siguiendo las tablas del cuaderno
static void test_dantzig_nightmare_history(void) {
    Problem problem;
    build_dantzig_nightmare(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "nightmare returns a result");
    if (result == NULL) {
        return;
    }

    // Mientras falten las piezas pendientes se avisa y se salta, sin contar como fallo
    if (result->status == SIMPLEX_ERROR_NOT_IMPLEMENTED) {
        printf("SKIPPED: Dantzig nightmare history (solver pieces are not implemented yet)\n");
        tests_skipped++;
        simplex_result_free(result);
        return;
    }

    check(result->status == SIMPLEX_UNBOUNDED, "nightmare ends unbounded");
    check(result->step_count == 3, "nightmare history has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    // Tabla 2 del cuaderno: x2 entra y sale la segunda restriccion
    const char *second_table[3][8] = {
        {"1", "-1", "0", "0", "6", "0", "3", "0"},
        {"0", "1", "0", "-2", "-9", "1", "9", "0"},
        {"0", "1/3", "1", "-1/3", "-2", "0", "1", "0"},
    };
    check(result->steps[0].leaving_constraint == 1, "step 0: the second constraint leaves");
    check_table(&result->steps[1].table, second_table, 3, "table after the first pivot");

    // Empate en las dos filas: sale x2 (menor indice que s1), camino 2 del cuaderno
    check(result->steps[1].entering_column == 1, "step 1: x1 enters");
    check(result->steps[1].tied_row_count == 2, "step 1: two rows tie");
    check(result->steps[1].leaving_constraint == 1, "step 1: x2 leaves by the tie rule");

    // Tabla del camino 2: la columna x3 no tiene fracciones positivas
    const char *third_table[3][8] = {
        {"1", "0", "3", "-1", "0", "0", "6", "0"},
        {"0", "0", "-3", "-1", "-3", "1", "6", "0"},
        {"0", "1", "3", "-1", "-6", "0", "3", "0"},
    };
    check_table(&result->steps[2].table, third_table, 3, "table of path 2");
    check(result->steps[2].entering_column == 3, "step 2: x3 enters");
    check(result->steps[2].leaving_constraint == -1, "step 2: no valid ratio");
    simplex_result_free(result);
}

// =============================================
// Main
// ---------------------------------------------

// Corre las pruebas del motor e informa el resultado
int main(void) {
    test_initial_table();
    test_entering_column_tie();
    test_rejected_problems();
    test_dantzig_nightmare_history();

    printf("%d checks run, %d failed, %d skipped\n", checks_run, checks_failed, tests_skipped);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
