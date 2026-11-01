#include "reference_problems.h"

#include <stdio.h>
#include <stdlib.h>

// =============================================
// Helpers
// ---------------------------------------------

// Asigna numerador / denominador; los datos de referencia son fijos, asi que un error termina la prueba
static void set_fraction(Fraction *target, long long numerator, long long denominator) {
    FractionStatus status = fraction_make(numerator, denominator, target);
    if (status != FRACTION_OK) {
        fprintf(stderr, "Invalid reference data: %s\n", fraction_status_message(status));
        exit(1);
    }
}

// =============================================
// Reference problems
// ---------------------------------------------

// Pesadilla de Dantzig: las tablas de referencia estan en el cuaderno de clase
void build_dantzig_nightmare(Problem *problem) {
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

// Wyndor Glass: x1 <= 4, 2x2 <= 12, 3x1 + 2x2 <= 18
void build_wyndor_glass(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Wyndor Glass");
    problem_set_sizes(problem, 2, 3);

    set_fraction(&problem->objective[0], 3, 1);
    set_fraction(&problem->objective[1], 5, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 4, 1);

    set_fraction(&problem->coefficients[1][1], 2, 1);
    set_fraction(&problem->right_hand_sides[1], 12, 1);

    set_fraction(&problem->coefficients[2][0], 3, 1);
    set_fraction(&problem->coefficients[2][1], 2, 1);
    set_fraction(&problem->right_hand_sides[2], 18, 1);
}

// Minimizacion de ejemplo: entra x2 y luego x1 (los valores mas positivos de la fila Z)
void build_minimization_example(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Minimization example");
    problem->objective_type = OBJECTIVE_MINIMIZE;

    set_fraction(&problem->objective[0], 1, 1);
    set_fraction(&problem->objective[1], -3, 1);

    set_fraction(&problem->coefficients[0][0], -1, 1);
    set_fraction(&problem->coefficients[0][1], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 2, 1);

    set_fraction(&problem->coefficients[1][0], 1, 1);
    set_fraction(&problem->coefficients[1][1], 1, 1);
    set_fraction(&problem->right_hand_sides[1], 6, 1);
}
