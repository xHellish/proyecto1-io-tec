#ifndef PROBLEM_H
#define PROBLEM_H

#include <stddef.h>

#include "fraction.h"

// =============================================
// Limits
// ---------------------------------------------
#define MIN_VARIABLES 2
#define MAX_VARIABLES 15
#define MIN_CONSTRAINTS 2
#define MAX_CONSTRAINTS 15
#define MAX_NAME_LENGTH 64

// Tamano sugerido para los buffers de nombres generados y mensajes de validacion
#define NAME_BUFFER_SIZE 32
#define MESSAGE_BUFFER_SIZE 256

// =============================================
// Types
// ---------------------------------------------
typedef enum {
    OBJECTIVE_MAXIMIZE,
    OBJECTIVE_MINIMIZE
} ObjectiveType;

// El tipo de cada restriccion queda en el modelo desde ya para la Parte 2
typedef enum {
    CONSTRAINT_LESS_EQUAL,
    CONSTRAINT_GREATER_EQUAL,
    CONSTRAINT_EQUAL
} ConstraintType;

// Datos del problema tal como los escribe el usuario. Solo se usan las primeras
// variable_count variables y constraint_count restricciones; el resto conserva
// sus datos por si el usuario vuelve a aumentar las cantidades
typedef struct {
    char name[MAX_NAME_LENGTH];
    ObjectiveType objective_type;
    int variable_count;
    int constraint_count;
    char variable_names[MAX_VARIABLES][MAX_NAME_LENGTH];
    Fraction objective[MAX_VARIABLES];
    Fraction coefficients[MAX_CONSTRAINTS][MAX_VARIABLES];
    ConstraintType constraint_types[MAX_CONSTRAINTS];
    Fraction right_hand_sides[MAX_CONSTRAINTS];
} Problem;

typedef enum {
    PROBLEM_OK = 0,
    PROBLEM_ERROR_VARIABLE_COUNT,
    PROBLEM_ERROR_CONSTRAINT_COUNT,
    PROBLEM_ERROR_NAME_TOO_LONG,
    PROBLEM_ERROR_INDEX_OUT_OF_RANGE,
    PROBLEM_ERROR_EMPTY_PROBLEM_NAME,
    PROBLEM_ERROR_EMPTY_VARIABLE_NAME,
    PROBLEM_ERROR_DUPLICATE_VARIABLE_NAME,
    PROBLEM_ERROR_SLACK_NAME_CONFLICT,
    PROBLEM_ERROR_NEGATIVE_RIGHT_HAND_SIDE
} ProblemError;

// =============================================
// Functions
// ---------------------------------------------

// Deja el problema con los valores por defecto: 2 x 2, maximizar, todo en 0,
// restricciones <= y variables llamadas $x_1$, $x_2$, ...
void problem_init(Problem *problem);

// Cambia la cantidad de variables y restricciones (2 a 15) conservando los datos escritos
ProblemError problem_set_sizes(Problem *problem, int variable_count, int constraint_count);

// Cambian el nombre del problema o de una variable (indice desde 0)
ProblemError problem_set_name(Problem *problem, const char *name);
ProblemError problem_set_variable_name(Problem *problem, int variable_index, const char *name);

// Escribe el nombre por defecto de una variable ("$x_1$") o de una holgura ("$s_1$"), indice desde 0
void problem_default_variable_name(int index, char *buffer, size_t buffer_size);
void problem_slack_name(int index, char *buffer, size_t buffer_size);

// Revisa que el problema este completo y sea correcto. Escribe en message (en ingles)
// que campo esta mal; si todo esta bien deja message vacio y devuelve PROBLEM_OK
ProblemError problem_validate(const Problem *problem, char *message, size_t message_size);

// Mensaje general en ingles para un codigo de error
const char *problem_error_message(ProblemError error);

#endif
