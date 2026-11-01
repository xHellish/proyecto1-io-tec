#include "simplex.h"

#include <stdlib.h>

// =============================================
// Helpers
// ---------------------------------------------

// Revisa que el motor actual pueda resolver el problema (maximizar con restricciones <=)
static SimplexStatus validate_problem(const Problem *problem) {
    // Tamanos dentro de los limites
    if (problem->variable_count < MIN_VARIABLES || problem->variable_count > MAX_VARIABLES) {
        return SIMPLEX_ERROR_INVALID_PROBLEM;
    }
    if (problem->constraint_count < MIN_CONSTRAINTS || problem->constraint_count > MAX_CONSTRAINTS) {
        return SIMPLEX_ERROR_INVALID_PROBLEM;
    }

    // Lados derechos no negativos (garantizan que la tabla inicial sea factible)
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (fraction_sign(problem->right_hand_sides[constraint]) < 0) {
            return SIMPLEX_ERROR_INVALID_PROBLEM;
        }
    }

    // Minimizar y las restricciones >= o = son de la Parte 2 y de la Fase 4
    if (problem->objective_type != OBJECTIVE_MAXIMIZE) {
        return SIMPLEX_ERROR_UNSUPPORTED;
    }
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (problem->constraint_types[constraint] != CONSTRAINT_LESS_EQUAL) {
            return SIMPLEX_ERROR_UNSUPPORTED;
        }
    }

    return SIMPLEX_RUNNING;
}

// Arma la tabla inicial en el formato de clase
static void build_initial_table(const Problem *problem, Table *table) {
    int variable_count = problem->variable_count;
    int constraint_count = problem->constraint_count;
    int right_hand_side_column = 1 + variable_count + constraint_count;

    // Dimensiones: fila objetivo mas restricciones, y Z, originales, holguras y lado derecho
    table->row_count = constraint_count + 1;
    table->column_count = right_hand_side_column + 1;

    // Todo en cero para solo escribir los valores distintos de cero
    for (int row = 0; row < table->row_count; row++) {
        for (int column = 0; column < table->column_count; column++) {
            table->cells[row][column] = fraction_from_int(0);
        }
    }

    // Tipo de cada columna
    table->column_types[0] = COLUMN_Z;
    for (int column = 1; column <= variable_count; column++) {
        table->column_types[column] = COLUMN_ORIGINAL;
    }
    for (int column = variable_count + 1; column < right_hand_side_column; column++) {
        table->column_types[column] = COLUMN_SLACK;
    }
    table->column_types[right_hand_side_column] = COLUMN_RIGHT_HAND_SIDE;

    // Fila objetivo: Z - c1 x1 - c2 x2 ... = 0, con el signo de los coeficientes cambiado
    table->cells[0][0] = fraction_from_int(1);
    for (int variable = 0; variable < variable_count; variable++) {
        table->cells[0][variable + 1] = fraction_negate(problem->objective[variable]);
    }

    // Restricciones: coeficientes, su holgura (identidad) y lado derecho
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        int row = constraint + 1;
        for (int variable = 0; variable < variable_count; variable++) {
            table->cells[row][variable + 1] = problem->coefficients[constraint][variable];
        }
        int slack_column = 1 + variable_count + constraint;
        table->cells[row][slack_column] = fraction_from_int(1);
        table->cells[row][right_hand_side_column] = problem->right_hand_sides[constraint];

        // La holgura de cada restriccion empieza en la base
        table->basis[constraint] = slack_column;
    }
}

// Guarda en el historial la tabla con la que empieza un paso, sin decisiones todavia
static void start_step(Step *step, const Table *table) {
    step->table = *table;
    step->entering_column = -1;
    step->leaving_constraint = -1;
    step->pivot = fraction_from_int(0);
    step->tied_row_count = 0;
}

// =============================================
// Engine pieces
// ---------------------------------------------

// Elige la columna que entra: el valor mas negativo de la fila objetivo
int simplex_choose_entering_column(const Table *table) {
    int entering_column = -1;

    // Solo las columnas de variables: se excluyen Z (primera) y el lado derecho (ultima)
    for (int column = 1; column < table->column_count - 1; column++) {
        Fraction value = table->cells[0][column];
        if (fraction_sign(value) < 0) {
            // Con empate se conserva la de menor indice: solo se reemplaza con un valor estrictamente menor
            if (entering_column == -1 || fraction_compare(value, table->cells[0][entering_column]) < 0) {
                entering_column = column;
            }
        }
    }
    return entering_column;
}

// PENDIENTE: ver la descripcion en simplex.h
SimplexStatus simplex_choose_leaving_constraint(const Table *table, int entering_column, Step *step) {
    (void)table;
    (void)entering_column;
    (void)step;
    return SIMPLEX_ERROR_NOT_IMPLEMENTED;
}

// PENDIENTE: ver la descripcion en simplex.h
SimplexStatus simplex_pivot(Table *table, int leaving_constraint, int entering_column) {
    (void)table;
    (void)leaving_constraint;
    (void)entering_column;
    return SIMPLEX_ERROR_NOT_IMPLEMENTED;
}

// PENDIENTE: ver la descripcion en simplex.h
SimplexStatus simplex_extract_solution(const Problem *problem, const Table *table, SimplexResult *result) {
    (void)problem;
    (void)table;
    (void)result;
    return SIMPLEX_ERROR_NOT_IMPLEMENTED;
}

// =============================================
// Public functions
// ---------------------------------------------

// Resuelve el problema pivoteando hasta el optimo, guardando cada paso en el historial
SimplexResult *simplex_solve(const Problem *problem) {
    SimplexResult *result = calloc(1, sizeof(SimplexResult));
    if (result == NULL) {
        return NULL;
    }

    // Rechazar de entrada los problemas que el motor no puede resolver
    result->status = validate_problem(problem);
    if (result->status != SIMPLEX_RUNNING) {
        return result;
    }

    Table table;
    build_initial_table(problem, &table);

    int pivot_count = 0;
    SimplexStatus status = SIMPLEX_RUNNING;
    while (status == SIMPLEX_RUNNING) {
        // Guardar la tabla con la que empieza este paso
        Step *step = &result->steps[pivot_count];
        start_step(step, &table);
        result->step_count = pivot_count + 1;

        // Sin valores negativos en la fila objetivo la tabla es optima
        int entering_column = simplex_choose_entering_column(&table);
        if (entering_column == -1) {
            status = simplex_extract_solution(problem, &table, result);
            continue;
        }
        step->entering_column = entering_column;

        // Fila que sale; si ninguna fraccion es valida el problema es no acotado
        status = simplex_choose_leaving_constraint(&table, entering_column, step);
        if (status != SIMPLEX_RUNNING) {
            continue;
        }
        step->pivot = table.cells[step->leaving_constraint + 1][entering_column];

        // Rendirse si se alcanzo el limite de pivoteos
        if (pivot_count >= MAX_ITERATIONS) {
            status = SIMPLEX_ITERATION_LIMIT;
            continue;
        }

        // Pivotear y pasar al siguiente paso
        status = simplex_pivot(&table, step->leaving_constraint, entering_column);
        pivot_count++;
    }

    result->status = status;
    return result;
}

// Libera un resultado
void simplex_result_free(SimplexResult *result) {
    free(result);
}

// Mensaje en ingles para el estado del resultado
const char *simplex_status_message(SimplexStatus status) {
    switch (status) {
    case SIMPLEX_RUNNING:
        return "The solver is still running.";
    case SIMPLEX_OPTIMAL:
        return "An optimal solution was found.";
    case SIMPLEX_UNBOUNDED:
        return "The problem is unbounded: the objective can grow without limit.";
    case SIMPLEX_ITERATION_LIMIT:
        return "The iteration limit was reached before finding a solution.";
    case SIMPLEX_ERROR_OVERFLOW:
        return "A number became too large to be represented exactly.";
    case SIMPLEX_ERROR_INVALID_PROBLEM:
        return "The problem is invalid: check the sizes and the right-hand sides.";
    case SIMPLEX_ERROR_UNSUPPORTED:
        return "Only maximization with <= constraints is supported for now.";
    case SIMPLEX_ERROR_NOT_IMPLEMENTED:
        return "This part of the solver is not implemented yet.";
    }
    return "Unknown solver status.";
}
