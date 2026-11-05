#ifndef SAMPLE_PROBLEMS_H
#define SAMPLE_PROBLEMS_H

#include "problem.h"

// =============================================
// Types
// ---------------------------------------------
// Problema del cuaderno tal como se escribe en el formulario. Los numeros van como texto
// (igual que en los campos) y una celda NULL queda vacia, es decir, vale 0
typedef struct {
    // Texto del menu desplegable
    const char *title;
    // Nombre del problema que se escribe en el formulario
    const char *name;
    ObjectiveType objective_type;
    int variable_count;
    int constraint_count;
    const char *objective[MAX_VARIABLES];
    const char *coefficients[MAX_CONSTRAINTS][MAX_VARIABLES];
    const char *right_hand_sides[MAX_CONSTRAINTS];
    // Resultado del cuaderno para comparar con el PDF
    const char *expected;
    // Z optimo como texto de fraccion; NULL si el problema es no acotado
    const char *expected_objective;
} SampleProblem;

// =============================================
// Functions
// ---------------------------------------------

// Cantidad de problemas del cuaderno disponibles
int sample_problem_count(void);

// Problema del cuaderno por indice (desde 0); NULL si el indice no existe
const SampleProblem *sample_problem_get(int index);

#endif
