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

// Cuaderno de clase (04/09): x1 + x2 <= 40 y x1 + 2x2 <= 60, con las tablas hechas a mano
void build_class_notes_maximization(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Class notes maximization");
    problem_set_sizes(problem, 2, 2);

    set_fraction(&problem->objective[0], 3, 1);
    set_fraction(&problem->objective[1], 4, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->coefficients[0][1], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 40, 1);

    set_fraction(&problem->coefficients[1][0], 1, 1);
    set_fraction(&problem->coefficients[1][1], 2, 1);
    set_fraction(&problem->right_hand_sides[1], 60, 1);
}

// Escribe las restricciones de Dakota (comunes a las dos versiones del problema)
static void set_dakota_constraints(Problem *problem) {
    set_fraction(&problem->coefficients[0][0], 8, 1);
    set_fraction(&problem->coefficients[0][1], 6, 1);
    set_fraction(&problem->coefficients[0][2], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 48, 1);

    set_fraction(&problem->coefficients[1][0], 4, 1);
    set_fraction(&problem->coefficients[1][1], 2, 1);
    set_fraction(&problem->coefficients[1][2], 3, 2);
    set_fraction(&problem->right_hand_sides[1], 20, 1);

    set_fraction(&problem->coefficients[2][0], 2, 1);
    set_fraction(&problem->coefficients[2][1], 3, 2);
    set_fraction(&problem->coefficients[2][2], 1, 2);
    set_fraction(&problem->right_hand_sides[2], 8, 1);

    set_fraction(&problem->coefficients[3][1], 1, 1);
    set_fraction(&problem->right_hand_sides[3], 5, 1);
}

// Dakota: 8x1 + 6x2 + x3 <= 48, 4x1 + 2x2 + 1.5x3 <= 20, 2x1 + 1.5x2 + 0.5x3 <= 8 y x2 <= 5
void build_dakota(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Dakota");
    problem_set_sizes(problem, 3, 4);

    set_fraction(&problem->objective[0], 60, 1);
    set_fraction(&problem->objective[1], 30, 1);
    set_fraction(&problem->objective[2], 20, 1);
    set_dakota_constraints(problem);
}

// Dakota con el coeficiente de x2 en 35: mismas restricciones, aparecen soluciones multiples
void build_dakota_multiple(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Dakota multiple");
    problem_set_sizes(problem, 3, 4);

    set_fraction(&problem->objective[0], 60, 1);
    set_fraction(&problem->objective[1], 35, 1);
    set_fraction(&problem->objective[2], 20, 1);
    set_dakota_constraints(problem);
}

// Cuaderno: 2x1 + 7x2 <= 21 y 7x1 + 2x2 <= 21
void build_class_four_fourteen(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Four fourteen");
    problem_set_sizes(problem, 2, 2);

    set_fraction(&problem->objective[0], 4, 1);
    set_fraction(&problem->objective[1], 14, 1);

    set_fraction(&problem->coefficients[0][0], 2, 1);
    set_fraction(&problem->coefficients[0][1], 7, 1);
    set_fraction(&problem->right_hand_sides[0], 21, 1);

    set_fraction(&problem->coefficients[1][0], 7, 1);
    set_fraction(&problem->coefficients[1][1], 2, 1);
    set_fraction(&problem->right_hand_sides[1], 21, 1);
}

// Minimizacion del cuaderno: x1 + x2 <= 4 y x1 - x2 <= 6
void build_class_minimization(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Class minimization");
    problem_set_sizes(problem, 2, 2);
    problem->objective_type = OBJECTIVE_MINIMIZE;

    set_fraction(&problem->objective[0], 2, 1);
    set_fraction(&problem->objective[1], -3, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->coefficients[0][1], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 4, 1);

    set_fraction(&problem->coefficients[1][0], 1, 1);
    set_fraction(&problem->coefficients[1][1], -1, 1);
    set_fraction(&problem->right_hand_sides[1], 6, 1);
}

// BreadCo: x1 + x2 - x3 <= 5 y 6x1 + 5x2 - x4 <= 10 (x3 levadura y x4 harina compradas)
void build_breadco_unbounded(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "BreadCo");
    problem_set_sizes(problem, 4, 2);

    set_fraction(&problem->objective[0], 36, 1);
    set_fraction(&problem->objective[1], 30, 1);
    set_fraction(&problem->objective[2], -3, 1);
    set_fraction(&problem->objective[3], -4, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->coefficients[0][1], 1, 1);
    set_fraction(&problem->coefficients[0][2], -1, 1);
    set_fraction(&problem->right_hand_sides[0], 5, 1);

    set_fraction(&problem->coefficients[1][0], 6, 1);
    set_fraction(&problem->coefficients[1][1], 5, 1);
    set_fraction(&problem->coefficients[1][3], -1, 1);
    set_fraction(&problem->right_hand_sides[1], 10, 1);
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

// Soluciones multiples de clase: en la tabla optima x1 no es basica y tiene 0 en la fila Z
void build_class_multiple_solutions(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Multiple solutions (class)");

    set_fraction(&problem->objective[0], 2, 1);
    set_fraction(&problem->objective[1], 4, 1);

    set_fraction(&problem->coefficients[0][0], 1, 1);
    set_fraction(&problem->coefficients[0][1], 2, 1);
    set_fraction(&problem->right_hand_sides[0], 5, 1);

    set_fraction(&problem->coefficients[1][0], 1, 1);
    set_fraction(&problem->coefficients[1][1], 1, 1);
    set_fraction(&problem->right_hand_sides[1], 4, 1);
}

// Degenerado de clase: x1 empata en las fracciones de las dos primeras restricciones
void build_class_degenerate(Problem *problem) {
    problem_init(problem);
    problem_set_name(problem, "Degenerate (class)");
    problem_set_sizes(problem, 2, 3);

    set_fraction(&problem->objective[0], 2, 1);
    set_fraction(&problem->objective[1], 1, 1);

    set_fraction(&problem->coefficients[0][0], 3, 1);
    set_fraction(&problem->coefficients[0][1], 1, 1);
    set_fraction(&problem->right_hand_sides[0], 6, 1);

    set_fraction(&problem->coefficients[1][0], 1, 1);
    set_fraction(&problem->coefficients[1][1], -1, 1);
    set_fraction(&problem->right_hand_sides[1], 2, 1);

    set_fraction(&problem->coefficients[2][1], 1, 1);
    set_fraction(&problem->right_hand_sides[2], 3, 1);
}
