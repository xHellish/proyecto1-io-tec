#include "problem.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// =============================================
// Helpers
// ---------------------------------------------

// Indica si el texto esta vacio o solo tiene espacios
static int is_blank(const char *text) {
    int index = 0;
    while (text[index] != '\0') {
        if (!isspace((unsigned char)text[index])) {
            return 0;
        }
        index++;
    }
    return 1;
}

// Copia el texto sin los espacios del inicio y del final
static void trim_into(const char *text, char *buffer, size_t buffer_size) {
    // Saltar espacios iniciales y retroceder sobre los finales
    const char *start = text;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    size_t length = strlen(start);
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        length--;
    }

    // Copiar sin pasarse del buffer
    if (length >= buffer_size) {
        length = buffer_size - 1;
    }
    memcpy(buffer, start, length);
    buffer[length] = '\0';
}

// Copia un nombre al destino si cabe en MAX_NAME_LENGTH
static ProblemError copy_name(char *destination, const char *source) {
    if (strlen(source) >= MAX_NAME_LENGTH) {
        return PROBLEM_ERROR_NAME_TOO_LONG;
    }
    snprintf(destination, MAX_NAME_LENGTH, "%s", source);
    return PROBLEM_OK;
}

// Escribe un nombre como "$<letter>_n$"; usa llaves si el numero tiene dos digitos
static void write_indexed_name(char letter, int index, char *buffer, size_t buffer_size) {
    int number = index + 1;
    if (number < 10) {
        snprintf(buffer, buffer_size, "$%c_%d$", letter, number);
        return;
    }
    snprintf(buffer, buffer_size, "$%c_{%d}$", letter, number);
}

// =============================================
// Construction
// ---------------------------------------------

// Deja el problema con los valores por defecto
void problem_init(Problem *problem) {
    // Limpiar todo y fijar valores generales
    memset(problem, 0, sizeof(Problem));
    problem->objective_type = OBJECTIVE_MAXIMIZE;
    problem->variable_count = MIN_VARIABLES;
    problem->constraint_count = MIN_CONSTRAINTS;

    // Nombres por defecto y objetivo en cero
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        problem_default_variable_name(variable, problem->variable_names[variable], MAX_NAME_LENGTH);
        problem->objective[variable] = fraction_from_int(0);
    }

    // Restricciones <= con coeficientes y lado derecho en cero
    for (int constraint = 0; constraint < MAX_CONSTRAINTS; constraint++) {
        problem->constraint_types[constraint] = CONSTRAINT_LESS_EQUAL;
        problem->right_hand_sides[constraint] = fraction_from_int(0);
        for (int variable = 0; variable < MAX_VARIABLES; variable++) {
            problem->coefficients[constraint][variable] = fraction_from_int(0);
        }
    }
}

// Cambia las cantidades conservando los datos de las posiciones que quedan fuera
ProblemError problem_set_sizes(Problem *problem, int variable_count, int constraint_count) {
    if (variable_count < MIN_VARIABLES || variable_count > MAX_VARIABLES) {
        return PROBLEM_ERROR_VARIABLE_COUNT;
    }
    if (constraint_count < MIN_CONSTRAINTS || constraint_count > MAX_CONSTRAINTS) {
        return PROBLEM_ERROR_CONSTRAINT_COUNT;
    }
    problem->variable_count = variable_count;
    problem->constraint_count = constraint_count;
    return PROBLEM_OK;
}

// Cambia el nombre del problema
ProblemError problem_set_name(Problem *problem, const char *name) {
    return copy_name(problem->name, name);
}

// Cambia el nombre de una variable
ProblemError problem_set_variable_name(Problem *problem, int variable_index, const char *name) {
    if (variable_index < 0 || variable_index >= MAX_VARIABLES) {
        return PROBLEM_ERROR_INDEX_OUT_OF_RANGE;
    }
    return copy_name(problem->variable_names[variable_index], name);
}

// Nombre por defecto de una variable: $x_1$
void problem_default_variable_name(int index, char *buffer, size_t buffer_size) {
    write_indexed_name('x', index, buffer, buffer_size);
}

// Nombre de una variable de holgura: $s_1$
void problem_slack_name(int index, char *buffer, size_t buffer_size) {
    write_indexed_name('s', index, buffer, buffer_size);
}

// =============================================
// Validation
// ---------------------------------------------

// Revisa el problema en orden y describe el primer campo incorrecto
ProblemError problem_validate(const Problem *problem, char *message, size_t message_size) {
    message[0] = '\0';

    // Cantidades de variables y restricciones
    if (problem->variable_count < MIN_VARIABLES || problem->variable_count > MAX_VARIABLES) {
        snprintf(message, message_size, "The number of variables must be between %d and %d.",
                 MIN_VARIABLES, MAX_VARIABLES);
        return PROBLEM_ERROR_VARIABLE_COUNT;
    }
    if (problem->constraint_count < MIN_CONSTRAINTS || problem->constraint_count > MAX_CONSTRAINTS) {
        snprintf(message, message_size, "The number of constraints must be between %d and %d.",
                 MIN_CONSTRAINTS, MAX_CONSTRAINTS);
        return PROBLEM_ERROR_CONSTRAINT_COUNT;
    }

    // Nombre del problema
    if (is_blank(problem->name)) {
        snprintf(message, message_size, "The problem name is empty.");
        return PROBLEM_ERROR_EMPTY_PROBLEM_NAME;
    }

    // Nombres de variables: ninguno vacio
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (is_blank(problem->variable_names[variable])) {
            snprintf(message, message_size, "The name of variable %d is empty.", variable + 1);
            return PROBLEM_ERROR_EMPTY_VARIABLE_NAME;
        }
    }

    // Nombres de variables: ninguno repetido (sin contar espacios en los extremos)
    char first_name[MAX_NAME_LENGTH];
    char second_name[MAX_NAME_LENGTH];
    for (int first = 0; first < problem->variable_count; first++) {
        trim_into(problem->variable_names[first], first_name, MAX_NAME_LENGTH);
        for (int second = first + 1; second < problem->variable_count; second++) {
            trim_into(problem->variable_names[second], second_name, MAX_NAME_LENGTH);
            if (strcmp(first_name, second_name) == 0) {
                snprintf(message, message_size, "Variables %d and %d have the same name: %s.",
                         first + 1, second + 1, first_name);
                return PROBLEM_ERROR_DUPLICATE_VARIABLE_NAME;
            }
        }
    }

    // Nombres de variables: ninguno igual al de una holgura
    char slack_name[NAME_BUFFER_SIZE];
    for (int variable = 0; variable < problem->variable_count; variable++) {
        trim_into(problem->variable_names[variable], first_name, MAX_NAME_LENGTH);
        for (int slack = 0; slack < problem->constraint_count; slack++) {
            problem_slack_name(slack, slack_name, NAME_BUFFER_SIZE);
            if (strcmp(first_name, slack_name) == 0) {
                snprintf(message, message_size,
                         "The name of variable %d (%s) is reserved for a slack variable.",
                         variable + 1, first_name);
                return PROBLEM_ERROR_SLACK_NAME_CONFLICT;
            }
        }
    }

    // Lados derechos: ninguno negativo
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (fraction_sign(problem->right_hand_sides[constraint]) < 0) {
            snprintf(message, message_size, "The right-hand side of constraint %d is negative.",
                     constraint + 1);
            return PROBLEM_ERROR_NEGATIVE_RIGHT_HAND_SIDE;
        }
    }

    return PROBLEM_OK;
}

// Mensaje general para un codigo de error
const char *problem_error_message(ProblemError error) {
    switch (error) {
    case PROBLEM_OK:
        return "No error.";
    case PROBLEM_ERROR_VARIABLE_COUNT:
        return "The number of variables is out of range.";
    case PROBLEM_ERROR_CONSTRAINT_COUNT:
        return "The number of constraints is out of range.";
    case PROBLEM_ERROR_NAME_TOO_LONG:
        return "The name is too long.";
    case PROBLEM_ERROR_INDEX_OUT_OF_RANGE:
        return "The variable position is out of range.";
    case PROBLEM_ERROR_EMPTY_PROBLEM_NAME:
        return "The problem name is empty.";
    case PROBLEM_ERROR_EMPTY_VARIABLE_NAME:
        return "A variable name is empty.";
    case PROBLEM_ERROR_DUPLICATE_VARIABLE_NAME:
        return "Two variables have the same name.";
    case PROBLEM_ERROR_SLACK_NAME_CONFLICT:
        return "A variable name is reserved for a slack variable.";
    case PROBLEM_ERROR_NEGATIVE_RIGHT_HAND_SIDE:
        return "A right-hand side is negative.";
    }
    return "Unknown problem error.";
}
