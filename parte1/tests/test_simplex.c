#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fraction.h"
#include "problem.h"
#include "reference_problems.h"
#include "simplex.h"

// =============================================
// Constants
// ---------------------------------------------
#define RANDOM_PROBLEM_COUNT 200
#define RANDOM_SEED 2026

// =============================================
// Test helpers
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;

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

// Compara una tabla contra el texto esperado, celda por celda (expected va fila por fila)
static void check_table(const Table *table, const char *expected[], int row_count, int column_count, const char *name) {
    char text[FRACTION_STRING_SIZE];
    char description[160];

    check(table->row_count == row_count && table->column_count == column_count, name);
    if (table->row_count != row_count || table->column_count != column_count) {
        return;
    }
    for (int row = 0; row < row_count; row++) {
        for (int column = 0; column < column_count; column++) {
            fraction_to_string(table->cells[row][column], text, FRACTION_STRING_SIZE);
            snprintf(description, sizeof(description), "%s, row %d column %d (expected %s, got %s)",
                     name, row, column, expected[row * column_count + column], text);
            check(strcmp(text, expected[row * column_count + column]) == 0, description);
        }
    }
}

// Verifica la columna que entra, la fila que sale y cuantas filas empataron en un paso
static void check_step(const SimplexResult *result, int step_index, int entering, int leaving, int tied, const char *name) {
    char description[160];
    const Step *step = &result->steps[step_index];

    snprintf(description, sizeof(description), "%s: entering column", name);
    check(step->entering_column == entering, description);
    snprintf(description, sizeof(description), "%s: leaving constraint", name);
    check(step->leaving_constraint == leaving, description);
    snprintf(description, sizeof(description), "%s: tied rows", name);
    check(step->tied_row_count == tied, description);
}

// Verifica la variable basica de cada restriccion
static void check_basis(const Table *table, const int expected[], int constraint_count, const char *name) {
    int matches = 1;
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        if (table->basis[constraint] != expected[constraint]) {
            matches = 0;
        }
    }
    check(matches, name);
}

// Verifica un valor de la solucion como texto
static void check_value(Fraction value, const char *expected, const char *description) {
    char text[FRACTION_STRING_SIZE];
    fraction_to_string(value, text, FRACTION_STRING_SIZE);
    if (strcmp(text, expected) != 0) {
        printf("FAIL: %s (expected %s, got %s)\n", description, expected, text);
        checks_failed++;
    }
    checks_run++;
}

// =============================================
// Reference problems
// ---------------------------------------------

// Chvatal: maximizar 5x1 + 4x2 + 3x3 con 3 restricciones; el optimo es Z = 13 en (2, 0, 1)
static void build_chvatal(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Chvatal");
    problem_set_sizes(problem, 3, 3);

    set_fraction(&problem->objective[0], 5, 1);
    set_fraction(&problem->objective[1], 4, 1);
    set_fraction(&problem->objective[2], 3, 1);

    set_fraction(&problem->coefficients[0][0], 2, 1);
    set_fraction(&problem->coefficients[0][1], 3, 1);
    set_fraction(&problem->coefficients[0][2], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 5, 1);

    set_fraction(&problem->coefficients[1][0], 4, 1);
    set_fraction(&problem->coefficients[1][1], 1, 1);
    set_fraction(&problem->coefficients[1][2], 2, 1);
    set_fraction(&problem->right_hand_sides[1], 11, 1);

    set_fraction(&problem->coefficients[2][0], 3, 1);
    set_fraction(&problem->coefficients[2][1], 4, 1);
    set_fraction(&problem->coefficients[2][2], 2, 1);
    set_fraction(&problem->right_hand_sides[2], 8, 1);
}

// Optimo con fracciones: maximizar x1 + x2 con x1 + 2x2 <= 4 y 3x1 + x2 <= 6; Z = 14/5 en (8/5, 6/5)
static void build_fractional_optimum(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Fractional optimum");
    problem_set_sizes(problem, 2, 2);

    set_fraction(&problem->objective[0], 1, 1);
    set_fraction(&problem->objective[1], 1, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->coefficients[0][1], 2, 1);
    set_fraction(&problem->right_hand_sides[0], 4, 1);

    set_fraction(&problem->coefficients[1][0], 3, 1);
    set_fraction(&problem->coefficients[1][1], 1, 1);
    set_fraction(&problem->right_hand_sides[1], 6, 1);
}

// Beale: un ejemplo clasico que cicla con la regla de la columna mas negativa
static void build_beale(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Beale");
    problem_set_sizes(problem, 4, 3);

    set_fraction(&problem->objective[0], 3, 4);
    set_fraction(&problem->objective[1], -150, 1);
    set_fraction(&problem->objective[2], 1, 50);
    set_fraction(&problem->objective[3], -6, 1);

    set_fraction(&problem->coefficients[0][0], 1, 4);
    set_fraction(&problem->coefficients[0][1], -60, 1);
    set_fraction(&problem->coefficients[0][2], -1, 25);
    set_fraction(&problem->coefficients[0][3], 9, 1);

    set_fraction(&problem->coefficients[1][0], 1, 2);
    set_fraction(&problem->coefficients[1][1], -90, 1);
    set_fraction(&problem->coefficients[1][2], -1, 50);
    set_fraction(&problem->coefficients[1][3], 3, 1);

    set_fraction(&problem->coefficients[2][2], 1, 1);
    set_fraction(&problem->right_hand_sides[2], 1, 1);
}

// =============================================
// Base tests
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

    const char *expected[] = {
        "1", "-2", "-3", "1", "12", "0", "0", "0",
        "0", "-2", "-9", "1", "9", "1", "0", "0",
        "0", "1/3", "1", "-1/3", "-2", "0", "1", "0",
    };
    check_table(&result->steps[0].table, expected, 3, 8, "Dantzig initial table");
    const int initial_basis[] = {5, 6};
    check_basis(&result->steps[0].table, initial_basis, 2, "slack variables start in the basis");
    check(result->steps[0].table.column_types[0] == COLUMN_Z, "column 0 is Z");
    check(result->steps[0].table.column_types[1] == COLUMN_ORIGINAL, "column 1 is original");
    check(result->steps[0].table.column_types[5] == COLUMN_SLACK, "column 5 is a slack");
    check(result->steps[0].table.column_types[7] == COLUMN_RIGHT_HAND_SIDE, "last column is the right-hand side");
    simplex_result_free(result);
}

// Problemas que el motor actual rechaza con un estado claro
static void test_rejected_problems(void) {
    Problem problem;
    SimplexResult *result;

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
// Maximization tests (tables checked one by one)
// ---------------------------------------------

// Wyndor Glass: 2 pivoteos, una fraccion invalida y optimo Z = 36
static void test_wyndor_glass(void) {
    Problem problem;
    build_wyndor_glass(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "Wyndor returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_OPTIMAL, "Wyndor is optimal");
    check(result->step_count == 3, "Wyndor has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    const char *table_0[] = {
        "1", "-3", "-5", "0", "0", "0", "0",
        "0", "1", "0", "1", "0", "0", "4",
        "0", "0", "2", "0", "1", "0", "12",
        "0", "3", "2", "0", "0", "1", "18",
    };
    const char *table_1[] = {
        "1", "-3", "0", "0", "5/2", "0", "30",
        "0", "1", "0", "1", "0", "0", "4",
        "0", "0", "1", "0", "1/2", "0", "6",
        "0", "3", "0", "0", "-1", "1", "6",
    };
    const char *table_2[] = {
        "1", "0", "0", "0", "3/2", "1", "36",
        "0", "0", "0", "1", "1/3", "-1/3", "2",
        "0", "0", "1", "0", "1/2", "0", "6",
        "0", "1", "0", "0", "-1/3", "1/3", "2",
    };
    check_table(&result->steps[0].table, table_0, 4, 7, "Wyndor table 0");
    check_table(&result->steps[1].table, table_1, 4, 7, "Wyndor table 1");
    check_table(&result->steps[2].table, table_2, 4, 7, "Wyndor table 2");

    check_step(result, 0, 2, 1, 1, "Wyndor step 0");
    check_step(result, 1, 1, 2, 1, "Wyndor step 1");
    check_step(result, 2, -1, -1, 0, "Wyndor step 2");
    check(result->steps[0].ratio_is_valid[0] == 0, "Wyndor step 0: constraint 1 has no valid ratio");
    check_value(result->steps[0].ratios[1], "6", "Wyndor step 0: ratio of constraint 2");
    check_value(result->steps[0].ratios[2], "9", "Wyndor step 0: ratio of constraint 3");
    check_value(result->steps[0].pivot, "2", "Wyndor step 0: pivot");

    const int final_basis[] = {3, 2, 1};
    check_basis(&result->steps[2].table, final_basis, 3, "Wyndor final basis");
    check_value(result->objective_value, "36", "Wyndor Z");
    check_value(result->variable_values[0], "2", "Wyndor x1");
    check_value(result->variable_values[1], "6", "Wyndor x2");
    check_value(result->slack_values[0], "2", "Wyndor s1");
    check_value(result->slack_values[1], "0", "Wyndor s2");
    check_value(result->slack_values[2], "0", "Wyndor s3");
    simplex_result_free(result);
}

// Chvatal: 3 variables, una fila con coeficiente 0 en la columna que entra y optimo Z = 13
static void test_chvatal(void) {
    Problem problem;
    build_chvatal(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "Chvatal returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_OPTIMAL, "Chvatal is optimal");
    check(result->step_count == 3, "Chvatal has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    const char *table_0[] = {
        "1", "-5", "-4", "-3", "0", "0", "0", "0",
        "0", "2", "3", "1", "1", "0", "0", "5",
        "0", "4", "1", "2", "0", "1", "0", "11",
        "0", "3", "4", "2", "0", "0", "1", "8",
    };
    const char *table_1[] = {
        "1", "0", "7/2", "-1/2", "5/2", "0", "0", "25/2",
        "0", "1", "3/2", "1/2", "1/2", "0", "0", "5/2",
        "0", "0", "-5", "0", "-2", "1", "0", "1",
        "0", "0", "-1/2", "1/2", "-3/2", "0", "1", "1/2",
    };
    const char *table_2[] = {
        "1", "0", "3", "0", "1", "0", "1", "13",
        "0", "1", "2", "0", "2", "0", "-1", "2",
        "0", "0", "-5", "0", "-2", "1", "0", "1",
        "0", "0", "-1", "1", "-3", "0", "2", "1",
    };
    check_table(&result->steps[0].table, table_0, 4, 8, "Chvatal table 0");
    check_table(&result->steps[1].table, table_1, 4, 8, "Chvatal table 1");
    check_table(&result->steps[2].table, table_2, 4, 8, "Chvatal table 2");

    check_step(result, 0, 1, 0, 1, "Chvatal step 0");
    check_step(result, 1, 3, 2, 1, "Chvatal step 1");
    check(result->steps[1].ratio_is_valid[1] == 0, "Chvatal step 1: a zero coefficient gives no ratio");
    check_value(result->steps[1].ratios[0], "5", "Chvatal step 1: ratio of constraint 1");
    check_value(result->steps[1].ratios[2], "1", "Chvatal step 1: ratio of constraint 3");

    const int final_basis[] = {1, 5, 3};
    check_basis(&result->steps[2].table, final_basis, 3, "Chvatal final basis");
    check_value(result->objective_value, "13", "Chvatal Z");
    check_value(result->variable_values[0], "2", "Chvatal x1");
    check_value(result->variable_values[1], "0", "Chvatal x2");
    check_value(result->variable_values[2], "1", "Chvatal x3");
    check_value(result->slack_values[1], "1", "Chvatal s2");
    simplex_result_free(result);
}

// Optimo fraccionario de 2 x 2, con empate al elegir la columna (entra x1, la de menor indice)
static void test_fractional_optimum(void) {
    Problem problem;
    build_fractional_optimum(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "fractional optimum returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_OPTIMAL, "fractional optimum is optimal");
    check(result->step_count == 3, "fractional optimum has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    const char *table_0[] = {
        "1", "-1", "-1", "0", "0", "0",
        "0", "1", "2", "1", "0", "4",
        "0", "3", "1", "0", "1", "6",
    };
    const char *table_1[] = {
        "1", "0", "-2/3", "0", "1/3", "2",
        "0", "0", "5/3", "1", "-1/3", "2",
        "0", "1", "1/3", "0", "1/3", "2",
    };
    const char *table_2[] = {
        "1", "0", "0", "2/5", "1/5", "14/5",
        "0", "0", "1", "3/5", "-1/5", "6/5",
        "0", "1", "0", "-1/5", "2/5", "8/5",
    };
    check_table(&result->steps[0].table, table_0, 3, 6, "fractional table 0");
    check_table(&result->steps[1].table, table_1, 3, 6, "fractional table 1");
    check_table(&result->steps[2].table, table_2, 3, 6, "fractional table 2");

    check_step(result, 0, 1, 1, 1, "fractional step 0");
    check_step(result, 1, 2, 0, 1, "fractional step 1");
    check_value(result->steps[1].ratios[0], "6/5", "fractional step 1: ratio of constraint 1");
    check_value(result->objective_value, "14/5", "fractional Z");
    check_value(result->variable_values[0], "8/5", "fractional x1");
    check_value(result->variable_values[1], "6/5", "fractional x2");
    simplex_result_free(result);
}

// Funcion objetivo en 0: la tabla inicial ya es optima, sin pivoteos
static void test_zero_objective(void) {
    Problem problem;
    problem_init(&problem);
    problem_set_name(&problem, "Zero objective");
    set_fraction(&problem.coefficients[0][0], 1, 1);
    set_fraction(&problem.right_hand_sides[0], 4, 1);
    set_fraction(&problem.coefficients[1][1], 1, 1);
    set_fraction(&problem.right_hand_sides[1], 5, 1);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "zero objective returns a result");
    if (result == NULL) {
        return;
    }
    check(result->status == SIMPLEX_OPTIMAL, "zero objective is optimal");
    check(result->step_count == 1, "zero objective needs no pivots");
    check_value(result->objective_value, "0", "zero objective Z");
    check_value(result->slack_values[0], "4", "zero objective s1");
    check_value(result->slack_values[1], "5", "zero objective s2");
    simplex_result_free(result);
}

// Con una columna entre 0 y sin fraccion positiva el problema es no acotado
static void test_unbounded(void) {
    Problem problem;
    problem_init(&problem);
    problem_set_name(&problem, "Unbounded");
    set_fraction(&problem.objective[0], 1, 1);
    set_fraction(&problem.coefficients[0][1], 1, 1);
    set_fraction(&problem.right_hand_sides[0], 3, 1);
    set_fraction(&problem.coefficients[1][1], 1, 1);
    set_fraction(&problem.right_hand_sides[1], 4, 1);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "unbounded problem returns a result");
    if (result == NULL) {
        return;
    }
    check(result->status == SIMPLEX_UNBOUNDED, "x1 can grow without limit");
    check(result->step_count == 1, "unbounded is detected in the first table");
    check_step(result, 0, 1, -1, 0, "unbounded step 0");
    simplex_result_free(result);
}

// =============================================
// Minimization tests (second algorithm: most positive value enters)
// ---------------------------------------------

// Minimizacion de ejemplo: entra el valor mas positivo y para cuando todos son <= 0
static void test_minimization_example(void) {
    Problem problem;
    build_minimization_example(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "minimization returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_OPTIMAL, "minimization is optimal");
    check(result->step_count == 3, "minimization history has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    // Tabla inicial: la fila Z es igual que al maximizar, Z - x1 + 3x2 = 0
    const char *initial_table[] = {
        "1", "-1", "3", "0", "0", "0",
        "0", "-1", "1", "1", "0", "2",
        "0", "1", "1", "0", "1", "6",
    };
    check_table(&result->steps[0].table, initial_table, 3, 6, "minimization initial table");
    check(result->steps[0].table.objective_type == OBJECTIVE_MINIMIZE, "table remembers it minimizes");

    // Entra x2 (3 es el mas positivo) y sale s1 (fraccion 2 contra 6)
    check_step(result, 0, 2, 0, 1, "minimization step 0");
    check_value(result->steps[0].ratios[0], "2", "minimization step 0: ratio of constraint 1");
    check_value(result->steps[0].ratios[1], "6", "minimization step 0: ratio of constraint 2");

    // Segunda tabla: entra x1 (2 es el unico positivo); la fila 1 tiene coeficiente negativo y no cuenta
    const char *second_table[] = {
        "1", "2", "0", "-3", "0", "-6",
        "0", "-1", "1", "1", "0", "2",
        "0", "2", "0", "-1", "1", "4",
    };
    check_table(&result->steps[1].table, second_table, 3, 6, "minimization second table");
    check_step(result, 1, 1, 1, 1, "minimization step 1");
    check(!result->steps[1].ratio_is_valid[0], "minimization step 1: negative coefficient has no ratio");
    check_value(result->steps[1].pivot, "2", "minimization step 1: pivot");

    // Tabla final: todos los valores de la fila Z son <= 0
    const char *final_table[] = {
        "1", "0", "0", "-2", "-1", "-10",
        "0", "0", "1", "1/2", "1/2", "4",
        "0", "1", "0", "-1/2", "1/2", "2",
    };
    check_table(&result->steps[2].table, final_table, 3, 6, "minimization final table");
    check_step(result, 2, -1, -1, 0, "minimization step 2");
    const int final_basis[] = {2, 1};
    check_basis(&result->steps[2].table, final_basis, 2, "minimization final basis");

    check_value(result->objective_value, "-10", "minimization Z");
    check_value(result->variable_values[0], "2", "minimization x1");
    check_value(result->variable_values[1], "4", "minimization x2");
    check_value(result->slack_values[0], "0", "minimization s1");
    check_value(result->slack_values[1], "0", "minimization s2");
    simplex_result_free(result);
}

// Costos no negativos: al minimizar la tabla inicial ya es optima con Z = 0
static void test_minimization_already_optimal(void) {
    Problem problem;
    build_wyndor_glass(&problem);
    problem.objective_type = OBJECTIVE_MINIMIZE;

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "minimizing Wyndor returns a result");
    if (result == NULL) {
        return;
    }
    check(result->status == SIMPLEX_OPTIMAL, "minimizing Wyndor is optimal");
    check(result->step_count == 1, "minimizing Wyndor needs no pivots");
    check(simplex_choose_entering_column(&result->steps[0].table) == -1, "negative Z row values do not enter");
    check_value(result->objective_value, "0", "minimizing Wyndor Z");
    check_value(result->variable_values[0], "0", "minimizing Wyndor x1");
    check_value(result->slack_values[2], "18", "minimizing Wyndor s3");
    simplex_result_free(result);
}

// Minimizar -x1 - x2 sin limite: empate en la entrada y luego una columna sin fracciones positivas
static void test_minimization_unbounded(void) {
    Problem problem;
    problem_init(&problem);
    problem_set_name(&problem, "Unbounded minimization");
    problem.objective_type = OBJECTIVE_MINIMIZE;
    set_fraction(&problem.objective[0], -1, 1);
    set_fraction(&problem.objective[1], -1, 1);
    set_fraction(&problem.coefficients[0][0], 1, 1);
    set_fraction(&problem.coefficients[0][1], -1, 1);
    set_fraction(&problem.right_hand_sides[0], 1, 1);
    set_fraction(&problem.coefficients[1][0], -1, 1);
    set_fraction(&problem.coefficients[1][1], 1, 1);
    set_fraction(&problem.right_hand_sides[1], 1, 1);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "unbounded minimization returns a result");
    if (result == NULL) {
        return;
    }
    check(result->status == SIMPLEX_UNBOUNDED, "Z can decrease without limit");
    check(result->step_count == 2, "unbounded minimization has 2 tables");

    // x1 y x2 empatan en 1: entra x1 por menor indice
    check_step(result, 0, 1, 0, 1, "unbounded minimization step 0");

    // Despues entra x2 (valor 2) y su columna no tiene coeficientes positivos
    const char *second_table[] = {
        "1", "0", "2", "-1", "0", "-1",
        "0", "1", "-1", "1", "0", "1",
        "0", "0", "0", "1", "1", "2",
    };
    check_table(&result->steps[1].table, second_table, 3, 6, "unbounded minimization second table");
    check_step(result, 1, 2, -1, 0, "unbounded minimization step 1");
    simplex_result_free(result);
}

// =============================================
// Special case tests
// ---------------------------------------------

// Pesadilla de Dantzig: sigue las tablas del cuaderno, resuelve el empate por menor indice y termina no acotado
static void test_dantzig_nightmare_history(void) {
    Problem problem;
    build_dantzig_nightmare(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "nightmare returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_UNBOUNDED, "nightmare ends unbounded");
    check(result->step_count == 3, "nightmare history has 3 tables");
    if (result->step_count != 3) {
        simplex_result_free(result);
        return;
    }

    // Tabla 2 del cuaderno: x2 entra y sale la segunda restriccion
    const char *second_table[] = {
        "1", "-1", "0", "0", "6", "0", "3", "0",
        "0", "1", "0", "-2", "-9", "1", "9", "0",
        "0", "1/3", "1", "-1/3", "-2", "0", "1", "0",
    };
    check_table(&result->steps[1].table, second_table, 3, 8, "table after the first pivot");
    check_step(result, 0, 2, 1, 1, "nightmare step 0");

    // Empate en las dos filas: sale x2 (menor indice que s1), el camino 2 del cuaderno
    check_step(result, 1, 1, 1, 2, "nightmare step 1");

    // Tabla del camino 2: la columna x3 no tiene fracciones positivas
    const char *third_table[] = {
        "1", "0", "3", "-1", "0", "0", "6", "0",
        "0", "0", "-3", "-1", "-3", "1", "6", "0",
        "0", "1", "3", "-1", "-6", "0", "3", "0",
    };
    check_table(&result->steps[2].table, third_table, 3, 8, "table of path 2");
    check_step(result, 2, 3, -1, 0, "nightmare step 2");
    const int final_basis[] = {5, 1};
    check_basis(&result->steps[2].table, final_basis, 2, "nightmare final basis");
    simplex_result_free(result);
}

// Beale cicla con estas reglas: el motor se detiene en el limite y la solucion queda en ceros validos
static void test_iteration_limit(void) {
    Problem problem;
    build_beale(&problem);
    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "Beale returns a result");
    if (result == NULL) {
        return;
    }

    check(result->status == SIMPLEX_ITERATION_LIMIT, "Beale hits the iteration limit");
    check(result->step_count == MAX_ITERATIONS + 1, "Beale fills the whole history");
    check_value(result->objective_value, "0", "Z stays a valid 0 when there is no optimum");
    check_value(result->variable_values[0], "0", "x1 stays a valid 0 when there is no optimum");
    simplex_result_free(result);
}

// =============================================
// Random problems
// ---------------------------------------------

// Devuelve un numero al azar entre minimum y maximum (incluidos)
static int random_between(int minimum, int maximum) {
    return minimum + rand() % (maximum - minimum + 1);
}

// Suma left + right * factor sin perder el error de desbordamiento; devuelve 0 si desbordo
static int add_product(Fraction *total, Fraction left, Fraction right) {
    Fraction product;
    if (fraction_multiply(left, right, &product) != FRACTION_OK) {
        return 0;
    }
    return fraction_add(*total, product, total) == FRACTION_OK;
}

// Comprueba con aritmetica exacta que una solucion optima sea factible y coherente
static int verify_optimal_solution(const Problem *problem, const SimplexResult *result) {
    const Table *final_table = &result->steps[result->step_count - 1].table;
    Fraction total;
    Fraction objective_total = fraction_from_int(0);

    // Todas las variables y holguras son no negativas
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (fraction_sign(result->variable_values[variable]) < 0) {
            return 0;
        }
    }
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        if (fraction_sign(result->slack_values[slack]) < 0) {
            return 0;
        }
    }

    // Cada restriccion se cumple como igualdad: suma(a * x) + holgura = lado derecho
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        total = result->slack_values[constraint];
        for (int variable = 0; variable < problem->variable_count; variable++) {
            if (!add_product(&total, problem->coefficients[constraint][variable], result->variable_values[variable])) {
                return 0;
            }
        }
        if (fraction_compare(total, problem->right_hand_sides[constraint]) != 0) {
            return 0;
        }
    }

    // Z coincide con suma(c * x)
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (!add_product(&objective_total, problem->objective[variable], result->variable_values[variable])) {
            return 0;
        }
    }
    if (fraction_compare(objective_total, result->objective_value) != 0) {
        return 0;
    }

    // Condicion de optimo: sin negativos en la fila objetivo al maximizar, sin positivos al minimizar
    for (int column = 1; column < final_table->column_count - 1; column++) {
        int sign = fraction_sign(final_table->cells[0][column]);
        if (problem->objective_type == OBJECTIVE_MAXIMIZE && sign < 0) {
            return 0;
        }
        if (problem->objective_type == OBJECTIVE_MINIMIZE && sign > 0) {
            return 0;
        }
    }
    return 1;
}

// Minimizar c x debe seguir los mismos pivoteos que maximizar -c x, con la fila Z con el signo cambiado
static int is_mirror_of(const SimplexResult *maximized, const SimplexResult *minimized) {
    Fraction negated;

    // Mismo final y misma cantidad de tablas
    if (maximized->status != minimized->status || maximized->step_count != minimized->step_count) {
        return 0;
    }

    for (int step_index = 0; step_index < maximized->step_count; step_index++) {
        const Step *max_step = &maximized->steps[step_index];
        const Step *min_step = &minimized->steps[step_index];
        const Table *max_table = &max_step->table;
        const Table *min_table = &min_step->table;

        // Mismas decisiones en cada paso
        if (max_step->entering_column != min_step->entering_column ||
            max_step->leaving_constraint != min_step->leaving_constraint ||
            max_step->tied_row_count != min_step->tied_row_count) {
            return 0;
        }

        // La fila Z cambia de signo (menos la columna Z) y las restricciones son identicas
        for (int row = 0; row < max_table->row_count; row++) {
            for (int column = 0; column < max_table->column_count; column++) {
                Fraction expected = max_table->cells[row][column];
                if (row == 0 && column > 0) {
                    negated = fraction_negate(expected);
                    expected = negated;
                }
                if (fraction_compare(expected, min_table->cells[row][column]) != 0) {
                    return 0;
                }
            }
        }
    }

    // El valor optimo cambia de signo
    negated = fraction_negate(maximized->objective_value);
    return fraction_compare(negated, minimized->objective_value) == 0;
}

// Resuelve problemas aleatorios de todos los tamanos y verifica cada solucion optima
static void test_random_problems(void) {
    int optimal_count = 0;
    int unbounded_count = 0;
    int other_count = 0;
    int sizes[4][2] = {{2, 2}, {15, 15}, {2, 15}, {15, 2}};
    char description[128];

    srand(RANDOM_SEED);
    for (int index = 0; index < RANDOM_PROBLEM_COUNT; index++) {
        Problem problem;
        int variable_count = random_between(MIN_VARIABLES, MAX_VARIABLES);
        int constraint_count = random_between(MIN_CONSTRAINTS, MAX_CONSTRAINTS);

        // Los primeros problemas cubren los tamanos extremos
        if (index < 4) {
            variable_count = sizes[index][0];
            constraint_count = sizes[index][1];
        }

        // Coeficientes enteros pequenos y lados derechos positivos
        problem_init(&problem);
        problem_set_name(&problem, "Random");
        problem_set_sizes(&problem, variable_count, constraint_count);
        for (int variable = 0; variable < variable_count; variable++) {
            problem.objective[variable] = fraction_from_int(random_between(0, 9));
        }
        for (int constraint = 0; constraint < constraint_count; constraint++) {
            problem.right_hand_sides[constraint] = fraction_from_int(random_between(1, 30));
            for (int variable = 0; variable < variable_count; variable++) {
                problem.coefficients[constraint][variable] = fraction_from_int(random_between(0, 9));
            }
        }

        SimplexResult *result = simplex_solve(&problem);
        snprintf(description, sizeof(description), "random problem %d (%d x %d) returns a result",
                 index, variable_count, constraint_count);
        check(result != NULL, description);
        if (result == NULL) {
            continue;
        }

        // Un optimo debe ser factible y coherente; los demas estados validos solo se cuentan
        snprintf(description, sizeof(description), "random problem %d (%d x %d) has a valid status",
                 index, variable_count, constraint_count);
        if (result->status == SIMPLEX_OPTIMAL) {
            optimal_count++;
            snprintf(description, sizeof(description), "random problem %d (%d x %d) has a correct solution",
                     index, variable_count, constraint_count);
            check(verify_optimal_solution(&problem, result), description);
        } else if (result->status == SIMPLEX_UNBOUNDED) {
            unbounded_count++;
            check(1, description);
        } else if (result->status == SIMPLEX_ITERATION_LIMIT || result->status == SIMPLEX_ERROR_OVERFLOW) {
            other_count++;
            check(1, description);
        } else {
            check(0, description);
        }

        // El mismo problema minimizando -c x: mismos pasos y, si hay optimo, una solucion correcta
        Problem minimization = problem;
        minimization.objective_type = OBJECTIVE_MINIMIZE;
        for (int variable = 0; variable < variable_count; variable++) {
            minimization.objective[variable] = fraction_negate(problem.objective[variable]);
        }
        SimplexResult *minimized = simplex_solve(&minimization);
        snprintf(description, sizeof(description), "random problem %d (%d x %d) minimizes as a mirror",
                 index, variable_count, constraint_count);
        check(minimized != NULL && is_mirror_of(result, minimized), description);
        if (minimized != NULL && minimized->status == SIMPLEX_OPTIMAL) {
            snprintf(description, sizeof(description), "random problem %d (%d x %d) has a correct minimum",
                     index, variable_count, constraint_count);
            check(verify_optimal_solution(&minimization, minimized), description);
        }
        simplex_result_free(minimized);
        simplex_result_free(result);
    }
    printf("Random problems: %d optimal, %d unbounded, %d other\n", optimal_count, unbounded_count, other_count);
}

// =============================================
// Main
// ---------------------------------------------

// Corre las pruebas del motor e informa el resultado
int main(void) {
    test_initial_table();
    test_rejected_problems();
    test_wyndor_glass();
    test_chvatal();
    test_fractional_optimum();
    test_zero_objective();
    test_unbounded();
    test_minimization_example();
    test_minimization_already_optimal();
    test_minimization_unbounded();
    test_dantzig_nightmare_history();
    test_iteration_limit();
    test_random_problems();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
