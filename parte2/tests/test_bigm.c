#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fraction.h"
#include "problem.h"
#include "simplex.h"

// =============================================
// Constants
// ---------------------------------------------
#define RANDOM_PROBLEM_COUNT 600
#define RANDOM_SEED 2027
#define RANDOM_TOLERANCE 1e-7
#define RANDOM_MAX_VARIABLES 4
#define RANDOM_MAX_CONSTRAINTS 5

// =============================================
// Test helpers
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;

// Cuenta una comprobacion y avisa si falla
static void check(int condition, const char *description) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s\n", description);
    }
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

// Arma el problema: objetivo y cantidad de restricciones (los coeficientes se agregan con set_constraint)
static void start_problem(Problem *problem, ObjectiveType type, int variable_count, int constraint_count,
                          const int objective[]) {
    problem_init(problem);
    problem_set_sizes(problem, variable_count, constraint_count);
    problem->objective_type = type;
    for (int variable = 0; variable < variable_count; variable++) {
        problem->objective[variable] = fraction_from_int(objective[variable]);
    }
}

// Escribe una restriccion: coeficientes, relacion y lado derecho
static void set_constraint(Problem *problem, int constraint, const int coefficients[], ConstraintType type,
                           int right_hand_side) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        problem->coefficients[constraint][variable] = fraction_from_int(coefficients[variable]);
    }
    problem->constraint_types[constraint] = type;
    problem->right_hand_sides[constraint] = fraction_from_int(right_hand_side);
}

// Cuenta las columnas de un tipo en la tabla
static int count_columns(const Table *table, ColumnType type) {
    int count = 0;
    for (int column = 0; column < table->column_count; column++) {
        if (table->column_types[column] == type) {
            count++;
        }
    }
    return count;
}

// =============================================
// Known problems
// ---------------------------------------------

// Minimizar 2x1 + 3x2 con x1 + x2 >= 4 y x1 + 3x2 >= 6: Z = 9 en (3, 1)
static void test_minimize_with_surplus(void) {
    Problem problem;
    const int objective[] = {2, 3};
    const int first[] = {1, 1};
    const int second[] = {1, 3};
    start_problem(&problem, OBJECTIVE_MINIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_GREATER_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_GREATER_EQUAL, 6);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL, "minimize >= returns a result");
    if (result == NULL) {
        return;
    }
    check(result->status == SIMPLEX_OPTIMAL, "minimize >= is optimal");
    check(result->has_artificials == 1, "minimize >= uses artificial variables");
    check_value(result->objective_value, "9", "minimize >= Z");
    check_value(result->variable_values[0], "3", "minimize >= x1");
    check_value(result->variable_values[1], "1", "minimize >= x2");
    check_value(result->slack_values[0], "0", "minimize >= first surplus");
    check_value(result->slack_values[1], "0", "minimize >= second surplus");

    // Tabla inicial: Z, x1, x2, e1, e2, a1, a2 y lado derecho, con -M en las artificiales (minimizar)
    const Table *initial = &result->initial_table;
    check(initial->column_count == 8, "initial table has 8 columns");
    check(count_columns(initial, COLUMN_SURPLUS) == 2, "two surplus columns");
    check(count_columns(initial, COLUMN_ARTIFICIAL) == 2, "two artificial columns");
    check_value(initial->m_cells[5], "-1", "initial a1 has -M when minimizing");
    check_value(initial->m_cells[6], "-1", "initial a2 has -M when minimizing");
    check_value(initial->cells[1][3], "-1", "surplus coefficient is -1");
    check(initial->basis[0] == 5 && initial->basis[1] == 6, "artificials start in the basis");

    // La canonizacion deja las artificiales sin M y mueve la M al resto de la fila
    const Table *canonized = &result->steps[0].table;
    check_value(canonized->m_cells[5], "0", "canonized a1 has no M");
    check_value(canonized->m_cells[6], "0", "canonized a2 has no M");
    check_value(canonized->m_cells[1], "2", "canonized x1 has 2M");
    check_value(canonized->m_cells[2], "4", "canonized x2 has 4M");
    check_value(canonized->m_cells[3], "-1", "canonized e1 has -M");
    check_value(canonized->m_cells[7], "10", "canonized right-hand side has 10M");

    // Las artificiales salen de la base y sus columnas desaparecen de la tabla final
    const Table *final_table = &result->steps[result->step_count - 1].table;
    check(count_columns(final_table, COLUMN_ARTIFICIAL) == 0, "final table has no artificial columns");
    check(final_table->column_count == 6, "final table has 6 columns");
    simplex_result_free(result);
}

// Maximizar 3x1 + 2x2 con x1 + x2 = 4 y x1 <= 3: Z = 11 en (3, 1)
static void test_maximize_with_equality(void) {
    Problem problem;
    const int objective[] = {3, 2};
    const int first[] = {1, 1};
    const int second[] = {1, 0};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 3);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "maximize = is optimal");
    if (result == NULL) {
        return;
    }
    check_value(result->objective_value, "11", "maximize = Z");
    check_value(result->variable_values[0], "3", "maximize = x1");
    check_value(result->variable_values[1], "1", "maximize = x2");
    check_value(result->slack_values[0], "0", "equality has no slack");
    check_value(result->slack_values[1], "0", "<= slack is 0");
    check_value(result->initial_table.m_cells[4], "1", "initial a1 has +M when maximizing");
    simplex_result_free(result);
}

// Maximizar 2x1 + 3x2 con x1 + x2 <= 10 y x1 + x2 >= 2: Z = 30 en (0, 10) y exceso 8
static void test_mixed_constraints(void) {
    Problem problem;
    const int objective[] = {2, 3};
    const int row[] = {1, 1};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 10);
    set_constraint(&problem, 1, row, CONSTRAINT_GREATER_EQUAL, 2);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "mixed constraints are optimal");
    if (result == NULL) {
        return;
    }
    check_value(result->objective_value, "30", "mixed Z");
    check_value(result->variable_values[1], "10", "mixed x2");
    check_value(result->slack_values[0], "0", "mixed slack");
    check_value(result->slack_values[1], "8", "mixed surplus");
    simplex_result_free(result);
}

// x1 + x2 <= 2 y x1 + x2 >= 5: no hay region factible
static void test_infeasible(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int row[] = {1, 1};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 2);
    set_constraint(&problem, 1, row, CONSTRAINT_GREATER_EQUAL, 5);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_INFEASIBLE, "contradictory constraints are infeasible");
    if (result == NULL) {
        return;
    }
    check(result->step_count > 0, "infeasible keeps its history");
    check_value(result->objective_value, "0", "infeasible keeps a valid Z");
    simplex_result_free(result);
}

// Un problema infactible que ademas mejora Z sin limite sigue siendo infactible
static void test_infeasible_with_improving_direction(void) {
    Problem problem;
    const int objective[] = {1, 0};
    const int first[] = {0, 1};
    const int second[] = {0, 1};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_LESS_EQUAL, 1);
    set_constraint(&problem, 1, second, CONSTRAINT_GREATER_EQUAL, 3);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_INFEASIBLE, "infeasible with a free direction");
    simplex_result_free(result);
}

// Maximizar x1 + x2 con x1 - x2 >= 1 y x2 >= 0: no acotado
static void test_unbounded_with_artificials(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int first[] = {1, -1};
    const int second[] = {0, 1};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_GREATER_EQUAL, 1);
    set_constraint(&problem, 1, second, CONSTRAINT_GREATER_EQUAL, 0);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_UNBOUNDED, "unbounded with >= constraints");
    simplex_result_free(result);
}

// Maximizar x1 + x2 con x1 + x2 <= 6 y x1 + x2 >= 2: segmento entre (6, 0) y (0, 6), Z = 6
static void test_multiple_solutions_with_surplus(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int row[] = {1, 1};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 6);
    set_constraint(&problem, 1, row, CONSTRAINT_GREATER_EQUAL, 2);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "multiple solutions with >= are optimal");
    if (result == NULL) {
        return;
    }
    check_value(result->objective_value, "6", "multiple solutions Z");
    check(result->multiple_kind != MULTIPLE_NONE, "multiple solutions are detected with artificials");
    simplex_result_free(result);
}

// Minimizar x1 + x2 con x1 - x2 = 0 y x1 + x2 >= 2: la artificial arranca en 0 (degenerado), Z = 2 en (1, 1)
static void test_degenerate_equality(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int first[] = {1, -1};
    const int second[] = {1, 1};
    start_problem(&problem, OBJECTIVE_MINIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_EQUAL, 0);
    set_constraint(&problem, 1, second, CONSTRAINT_GREATER_EQUAL, 2);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "degenerate equality is optimal");
    if (result == NULL) {
        return;
    }
    check(result->is_degenerate == 1, "degenerate equality is reported as degenerate");
    check_value(result->objective_value, "2", "degenerate equality Z");
    check_value(result->variable_values[0], "1", "degenerate equality x1");
    check_value(result->variable_values[1], "1", "degenerate equality x2");
    simplex_result_free(result);
}

// Con solo restricciones <= no hay artificiales y la tabla inicial es la primera del historial
static void test_without_artificials(void) {
    Problem problem;
    const int objective[] = {3, 5};
    const int first[] = {1, 0};
    const int second[] = {0, 2};
    start_problem(&problem, OBJECTIVE_MAXIMIZE, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_LESS_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 12);

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "only <= is optimal");
    if (result == NULL) {
        return;
    }
    check(result->has_artificials == 0, "only <= needs no artificial variables");
    check(count_columns(&result->initial_table, COLUMN_ARTIFICIAL) == 0, "only <= has no artificial columns");
    check_value(result->objective_value, "42", "only <= Z");
    simplex_result_free(result);
}

// Los limites de tamano con artificiales en todas las restricciones caben en la tabla
static void test_maximum_size_with_artificials(void) {
    Problem problem;
    problem_init(&problem);
    problem_set_sizes(&problem, MAX_VARIABLES, MAX_CONSTRAINTS);
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        problem.objective[variable] = fraction_from_int(1 + variable);
    }
    for (int constraint = 0; constraint < MAX_CONSTRAINTS; constraint++) {
        for (int variable = 0; variable < MAX_VARIABLES; variable++) {
            problem.coefficients[constraint][variable] = fraction_from_int(1 + (constraint + variable) % 3);
        }
        problem.constraint_types[constraint] = CONSTRAINT_GREATER_EQUAL;
        problem.right_hand_sides[constraint] = fraction_from_int(3 + constraint);
    }
    problem.objective_type = OBJECTIVE_MINIMIZE;

    SimplexResult *result = simplex_solve(&problem);
    check(result != NULL && result->status == SIMPLEX_OPTIMAL, "15 x 15 with all >= is optimal");
    if (result != NULL) {
        check(result->initial_table.column_count == 1 + 15 + 15 + 15 + 1, "15 x 15 initial table has 47 columns");
    }
    simplex_result_free(result);
}

// Las comparaciones con M mandan por la parte de M
static void test_objective_row_comparison(void) {
    Table table;
    memset(&table, 0, sizeof(table));
    table.column_count = 4;
    table.row_count = 1;
    for (int column = 0; column < 4; column++) {
        table.cells[0][column] = fraction_from_int(0);
        table.m_cells[column] = fraction_from_int(0);
    }
    // columna 0: -5 ; columna 1: M ; columna 2: -M ; columna 3: 3
    table.cells[0][0] = fraction_from_int(-5);
    table.m_cells[1] = fraction_from_int(1);
    table.m_cells[2] = fraction_from_int(-1);
    table.cells[0][3] = fraction_from_int(3);

    check(simplex_objective_sign(&table, 0) < 0, "-5 is negative");
    check(simplex_objective_sign(&table, 1) > 0, "M is positive");
    check(simplex_objective_sign(&table, 2) < 0, "-M is negative");
    check(simplex_objective_compare(&table, 1, 3) > 0, "M beats any constant");
    check(simplex_objective_compare(&table, 2, 0) < 0, "-M is below any constant");
    check(simplex_objective_compare(&table, 0, 3) < 0, "-5 is below 3");
}

// =============================================
// Random problems (checked against vertex enumeration)
// ---------------------------------------------

// Devuelve un numero al azar entre minimum y maximum (incluidos)
static int random_between(int minimum, int maximum) {
    return minimum + rand() % (maximum - minimum + 1);
}

// Valor absoluto de un double
static double absolute_value(double value) {
    if (value < 0) {
        return -value;
    }
    return value;
}

// Convierte una fraccion a double
static double to_double(Fraction value) {
    return (double)value.numerator / (double)value.denominator;
}

// Resuelve el sistema cuadrado matrix * point = values por Gauss; devuelve 0 si es singular
static int solve_square_system(int size, double matrix[RANDOM_MAX_VARIABLES][RANDOM_MAX_VARIABLES + 1],
                               double point[]) {
    for (int column = 0; column < size; column++) {
        // Pivote parcial: la fila con el valor mas grande en esta columna
        int best_row = column;
        for (int row = column + 1; row < size; row++) {
            if (absolute_value(matrix[row][column]) > absolute_value(matrix[best_row][column])) {
                best_row = row;
            }
        }
        if (absolute_value(matrix[best_row][column]) < RANDOM_TOLERANCE) {
            return 0;
        }
        for (int index = 0; index <= size; index++) {
            double swap = matrix[column][index];
            matrix[column][index] = matrix[best_row][index];
            matrix[best_row][index] = swap;
        }

        // Anular la columna en las demas filas
        for (int row = 0; row < size; row++) {
            if (row == column) {
                continue;
            }
            double factor = matrix[row][column] / matrix[column][column];
            for (int index = column; index <= size; index++) {
                matrix[row][index] -= factor * matrix[column][index];
            }
        }
    }
    for (int row = 0; row < size; row++) {
        point[row] = matrix[row][size] / matrix[row][row];
    }
    return 1;
}

// Indica si un punto cumple x >= 0 y todas las restricciones del problema
static int is_feasible_point(const Problem *problem, const double point[]) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (point[variable] < -RANDOM_TOLERANCE) {
            return 0;
        }
    }
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        double total = 0;
        for (int variable = 0; variable < problem->variable_count; variable++) {
            total += to_double(problem->coefficients[constraint][variable]) * point[variable];
        }
        double right_hand_side = to_double(problem->right_hand_sides[constraint]);
        ConstraintType type = problem->constraint_types[constraint];
        if (type == CONSTRAINT_LESS_EQUAL && total > right_hand_side + RANDOM_TOLERANCE) {
            return 0;
        }
        if (type == CONSTRAINT_GREATER_EQUAL && total < right_hand_side - RANDOM_TOLERANCE) {
            return 0;
        }
        if (type == CONSTRAINT_EQUAL && absolute_value(total - right_hand_side) > RANDOM_TOLERANCE) {
            return 0;
        }
    }
    return 1;
}

// Busca el mejor vertice factible probando todas las combinaciones de variable_count hiperplanos.
// Devuelve 1 si hay al menos un vertice factible (la region esta acotada por la ultima restriccion)
static int best_vertex_value(const Problem *problem, double *best_value) {
    int variable_count = problem->variable_count;
    int plane_count = problem->constraint_count + variable_count;
    int found = 0;

    // Cada combinacion es un conjunto de variable_count hiperplanos (restricciones o x_j = 0)
    int chosen[RANDOM_MAX_VARIABLES];
    for (int index = 0; index < variable_count; index++) {
        chosen[index] = index;
    }
    while (1) {
        // Armar y resolver el sistema de los hiperplanos elegidos
        double matrix[RANDOM_MAX_VARIABLES][RANDOM_MAX_VARIABLES + 1];
        double point[RANDOM_MAX_VARIABLES];
        for (int row = 0; row < variable_count; row++) {
            int plane = chosen[row];
            for (int column = 0; column <= variable_count; column++) {
                matrix[row][column] = 0;
            }
            if (plane < problem->constraint_count) {
                for (int variable = 0; variable < variable_count; variable++) {
                    matrix[row][variable] = to_double(problem->coefficients[plane][variable]);
                }
                matrix[row][variable_count] = to_double(problem->right_hand_sides[plane]);
            } else {
                matrix[row][plane - problem->constraint_count] = 1;
            }
        }
        if (solve_square_system(variable_count, matrix, point) && is_feasible_point(problem, point)) {
            double value = 0;
            for (int variable = 0; variable < variable_count; variable++) {
                value += to_double(problem->objective[variable]) * point[variable];
            }
            int is_better = 0;
            if (problem->objective_type == OBJECTIVE_MAXIMIZE && value > *best_value) {
                is_better = 1;
            }
            if (problem->objective_type == OBJECTIVE_MINIMIZE && value < *best_value) {
                is_better = 1;
            }
            if (!found || is_better) {
                *best_value = value;
            }
            found = 1;
        }

        // Siguiente combinacion en orden lexicografico
        int position = variable_count - 1;
        while (position >= 0 && chosen[position] == plane_count - variable_count + position) {
            position--;
        }
        if (position < 0) {
            break;
        }
        chosen[position]++;
        for (int index = position + 1; index < variable_count; index++) {
            chosen[index] = chosen[index - 1] + 1;
        }
    }
    return found;
}

// Comprueba con doubles que la solucion del motor sea factible
static int engine_solution_is_feasible(const Problem *problem, const SimplexResult *result) {
    double point[RANDOM_MAX_VARIABLES];
    for (int variable = 0; variable < problem->variable_count; variable++) {
        point[variable] = to_double(result->variable_values[variable]);
    }
    return is_feasible_point(problem, point);
}

// Genera problemas al azar (la ultima restriccion acota la region) y compara el motor con la enumeracion
static void test_random_problems(void) {
    int optimal_count = 0;
    int infeasible_count = 0;
    int artificial_count = 0;
    int multiple_count = 0;
    char description[160];

    srand(RANDOM_SEED);
    for (int index = 0; index < RANDOM_PROBLEM_COUNT; index++) {
        int variable_count = random_between(MIN_VARIABLES, RANDOM_MAX_VARIABLES);
        int constraint_count = random_between(MIN_CONSTRAINTS, RANDOM_MAX_CONSTRAINTS);
        Problem problem;
        problem_init(&problem);
        problem_set_sizes(&problem, variable_count, constraint_count);
        if (random_between(0, 1) == 0) {
            problem.objective_type = OBJECTIVE_MAXIMIZE;
        } else {
            problem.objective_type = OBJECTIVE_MINIMIZE;
        }
        for (int variable = 0; variable < variable_count; variable++) {
            problem.objective[variable] = fraction_from_int(random_between(-5, 9));
        }
        for (int constraint = 0; constraint < constraint_count; constraint++) {
            for (int variable = 0; variable < variable_count; variable++) {
                problem.coefficients[constraint][variable] = fraction_from_int(random_between(-3, 9));
            }
            problem.constraint_types[constraint] = (ConstraintType)random_between(0, 2);
            problem.right_hand_sides[constraint] = fraction_from_int(random_between(0, 20));
        }

        // La ultima restriccion acota la region: x1 + ... + xn <= 30
        for (int variable = 0; variable < variable_count; variable++) {
            problem.coefficients[constraint_count - 1][variable] = fraction_from_int(1);
        }
        problem.constraint_types[constraint_count - 1] = CONSTRAINT_LESS_EQUAL;
        problem.right_hand_sides[constraint_count - 1] = fraction_from_int(30);

        SimplexResult *result = simplex_solve(&problem);
        snprintf(description, sizeof(description), "random problem %d (%d x %d) returns a result", index,
                 variable_count, constraint_count);
        check(result != NULL, description);
        if (result == NULL) {
            continue;
        }
        if (result->has_artificials) {
            artificial_count++;
        }

        double best_value = 0;
        int has_feasible_point = best_vertex_value(&problem, &best_value);
        if (has_feasible_point) {
            snprintf(description, sizeof(description), "random problem %d is optimal when feasible", index);
            check(result->status == SIMPLEX_OPTIMAL, description);
            if (result->status == SIMPLEX_OPTIMAL) {
                optimal_count++;
                if (result->multiple_kind != MULTIPLE_NONE) {
                    multiple_count++;
                }
                snprintf(description, sizeof(description), "random problem %d has the best objective value", index);
                check(absolute_value(to_double(result->objective_value) - best_value) < 1e-6, description);
                snprintf(description, sizeof(description), "random problem %d solution is feasible", index);
                check(engine_solution_is_feasible(&problem, result), description);
            } else {
                printf("  problem %d: status %d, expected optimal\n", index, (int)result->status);
            }
        } else {
            snprintf(description, sizeof(description), "random problem %d is infeasible when no vertex exists", index);
            check(result->status == SIMPLEX_INFEASIBLE, description);
            infeasible_count++;
        }
        simplex_result_free(result);
    }
    printf("Random problems: %d optimal (%d with multiple solutions), %d infeasible, %d with artificials\n",
           optimal_count, multiple_count, infeasible_count, artificial_count);
}

// =============================================
// Main
// ---------------------------------------------
int main(void) {
    test_objective_row_comparison();
    test_minimize_with_surplus();
    test_maximize_with_equality();
    test_mixed_constraints();
    test_infeasible();
    test_infeasible_with_improving_direction();
    test_unbounded_with_artificials();
    test_multiple_solutions_with_surplus();
    test_degenerate_equality();
    test_without_artificials();
    test_maximum_size_with_artificials();
    test_random_problems();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
