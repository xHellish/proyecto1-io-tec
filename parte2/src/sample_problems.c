#include "sample_problems.h"

#include <stddef.h>

// =============================================
// Sample problems
// ---------------------------------------------
// Problemas resueltos a mano en el cuaderno de clase, mas uno de 15 x 15 para probar el tamano maximo
// (ese no viene del cuaderno); los resultados se verifican en tests/test_simplex.c
static const SampleProblem SAMPLE_PROBLEMS[] = {
    {
        .title = "Maximización 2 × 2 (cuaderno 04/09)",
        .name = "Ejemplo del cuaderno 04-09",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 2,
        .constraint_count = 2,
        .objective = {"3", "4"},
        .coefficients = {{"1", "1"}, {"1", "2"}},
        .right_hand_sides = {"40", "60"},
        .expected = "Z = 140 con x1 = 20 y x2 = 20 (3 tablas: entra x2 y luego x1).",
        .expected_objective = "140",
    },
    {
        .title = "Puertas y ventanas (maximización 2 × 3)",
        .name = "Puertas y ventanas",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 2,
        .constraint_count = 3,
        .objective = {"3", "5"},
        .coefficients = {{"1", NULL}, {NULL, "2"}, {"3", "2"}},
        .right_hand_sides = {"4", "12", "18"},
        .expected = "Z = 36 con x1 = 2, x2 = 6 y s1 = 2 (la tabla final del cuaderno tiene errores de cuentas).",
        .expected_objective = "36",
    },
    {
        .title = "Dakota (maximización 3 × 4, decimales)",
        .name = "Dakota",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 3,
        .constraint_count = 4,
        .objective = {"60", "30", "20"},
        .coefficients = {{"8", "6", "1"}, {"4", "2", "1.5"}, {"2", "1.5", "0.5"}, {NULL, "1", NULL}},
        .right_hand_sides = {"48", "20", "8", "5"},
        .expected = "Z = 280 con x1 = 2, x2 = 0, x3 = 8, s1 = 24 y s4 = 5.",
        .expected_objective = "280",
    },
    {
        // Generado para probar 15 variables y 15 restricciones: 12 pivoteos, sin empates ni
        // soluciones multiples, y fracciones pequenas (denominador maximo 36) para que las tablas se lean
        .title = "Tamaño máximo 15 × 15 (de prueba, no es del cuaderno)",
        .name = "Tamaño máximo 15 x 15",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 15,
        .constraint_count = 15,
        .objective = {"7", "4", "1", "3", "1", "9", "3", "8", "6", "9", "5", "9", "8", "9", "8"},
        .coefficients =
            {
                {NULL, "2", "3", NULL, NULL, NULL, NULL, "2", NULL, "2", NULL, "3", NULL, "1", "4"},
                {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "4", NULL, NULL, NULL, NULL},
                {"1", NULL, "2", NULL, NULL, NULL, NULL, "1", NULL, NULL, NULL, NULL, NULL, NULL, "3"},
                {NULL, "3", NULL, NULL, NULL, NULL, NULL, "3", NULL, NULL, "1", NULL, NULL, "1", NULL},
                {"2", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "1", "1", NULL, NULL},
                {NULL, NULL, NULL, NULL, NULL, NULL, NULL, "3", NULL, "3", NULL, "2", "1", NULL, "3"},
                {"4", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
                {NULL, NULL, "2", NULL, NULL, "2", "3", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
                {"1", "2", NULL, NULL, NULL, NULL, "2", "2", NULL, NULL, NULL, "2", NULL, NULL, NULL},
                {NULL, NULL, "1", NULL, NULL, NULL, NULL, NULL, "3", NULL, "4", NULL, NULL, NULL, "2"},
                {NULL, "2", NULL, "1", NULL, NULL, NULL, NULL, NULL, NULL, "1", "1", NULL, NULL, NULL},
                {NULL, NULL, "1", NULL, NULL, NULL, NULL, NULL, "3", NULL, NULL, NULL, NULL, NULL, NULL},
                {NULL, NULL, NULL, "4", "1", "1", NULL, NULL, NULL, "2", NULL, NULL, NULL, NULL, NULL},
                {NULL, NULL, NULL, "2", NULL, "1", NULL, "1", NULL, NULL, "3", NULL, NULL, NULL, NULL},
                {NULL, "4", NULL, NULL, NULL, NULL, "4", "2", "1", "4", "1", NULL, NULL, "4", "1"},
            },
        .right_hand_sides = {"30", "80", "40", "30", "50", "20", "20", "50", "50", "80", "100", "30", "40", "70", "100"},
        .expected = "Z = 725 con x1 = 5, x5 = 15, x6 = 25, x9 = 10, x11 = 10, x13 = 20 y x14 = 20 (12 pivoteos).",
        .expected_objective = "725",
    },
    {
        .title = "Minimización (cuaderno 09/09)",
        .name = "Minimización del cuaderno",
        .objective_type = OBJECTIVE_MINIMIZE,
        .variable_count = 2,
        .constraint_count = 2,
        .objective = {"2", "-3"},
        .coefficients = {{"1", "1"}, {"1", "-1"}},
        .right_hand_sides = {"4", "6"},
        .expected = "Z = -12 con x1 = 0, x2 = 4 y s2 = 10 (entra x2, el más positivo).",
        .expected_objective = "-12",
    },
    {
        .title = "Soluciones múltiples: 2x1 + 4x2",
        .name = "Soluciones múltiples de clase",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 2,
        .constraint_count = 2,
        .objective = {"2", "4"},
        .coefficients = {{"1", "2"}, {"1", "1"}},
        .right_hand_sides = {"5", "4"},
        .expected = "Soluciones múltiples: Z = 10 en (0, 5/2) y en (3, 1).",
        .expected_objective = "10",
    },
    {
        .title = "Soluciones múltiples: 4x1 + 14x2",
        .name = "Cuatro catorce",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 2,
        .constraint_count = 2,
        .objective = {"4", "14"},
        .coefficients = {{"2", "7"}, {"7", "2"}},
        .right_hand_sides = {"21", "21"},
        .expected = "Soluciones múltiples: Z = 42 en (0, 3) y en (7/3, 7/3).",
        .expected_objective = "42",
    },
    {
        .title = "Soluciones múltiples: Dakota con x2 en 35",
        .name = "Dakota soluciones múltiples",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 3,
        .constraint_count = 4,
        .objective = {"60", "35", "20"},
        .coefficients = {{"8", "6", "1"}, {"4", "2", "1.5"}, {"2", "1.5", "0.5"}, {NULL, "1", NULL}},
        .right_hand_sides = {"48", "20", "8", "5"},
        .expected = "Soluciones múltiples: Z = 280 en (2, 0, 8) y en (0, 1.6, 11.2).",
        .expected_objective = "280",
    },
    {
        .title = "No acotado: panadería BreadCo",
        .name = "Panadería BreadCo",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 4,
        .constraint_count = 2,
        .objective = {"36", "30", "-3", "-4"},
        .coefficients = {{"1", "1", "-1", NULL}, {"6", "5", NULL, "-1"}},
        .right_hand_sides = {"5", "10"},
        .expected = "No acotado: en la tercera tabla entra x3 y ninguna fracción es válida.",
        .expected_objective = NULL,
    },
    {
        .title = "Degenerado (cuaderno 16/09)",
        .name = "Degenerado del cuaderno",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 2,
        .constraint_count = 3,
        .objective = {"2", "1"},
        .coefficients = {{"3", "1"}, {"1", "-1"}, {NULL, "1"}},
        .right_hand_sides = {"6", "2", "3"},
        .expected = "Z = 5 con x1 = 1 y x2 = 3. Empate en la primera tabla: sale s1 por menor índice (en clase salió s2).",
        .expected_objective = "5",
    },
    {
        .title = "Pesadilla de Dantzig",
        .name = "Pesadilla de Dantzig",
        .objective_type = OBJECTIVE_MAXIMIZE,
        .variable_count = 4,
        .constraint_count = 2,
        .objective = {"2", "3", "-1", "-12"},
        .coefficients = {{"-2", "-9", "1", "9"}, {"1/3", "1", "-1/3", "-2"}},
        .right_hand_sides = {"0", "0"},
        .expected = "No acotado por el camino 2 del cuaderno: empate en 0 y en la tercera tabla entra x3.",
        .expected_objective = NULL,
    },
};

// =============================================
// Public functions
// ---------------------------------------------

// Cantidad de problemas del cuaderno disponibles
int sample_problem_count(void) {
    return (int)(sizeof(SAMPLE_PROBLEMS) / sizeof(SAMPLE_PROBLEMS[0]));
}

// Problema del cuaderno por indice (desde 0); NULL si el indice no existe
const SampleProblem *sample_problem_get(int index) {
    if (index < 0 || index >= sample_problem_count()) {
        return NULL;
    }
    return &SAMPLE_PROBLEMS[index];
}
