#include "simplex.h"

#include <stdlib.h>

// =============================================
// Helpers
// ---------------------------------------------

// Revisa que el motor actual pueda resolver el problema (restricciones <= y lados derechos no negativos)
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

    // Las restricciones >= o = son de la Parte 2
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

    // Maximizar o minimizar solo cambia la regla de la columna que entra, no la tabla
    table->objective_type = problem->objective_type;

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

// Indica si el valor de la fila objetivo mejora Z: negativo al maximizar, positivo al minimizar
static int improves_objective(const Table *table, Fraction value) {
    if (table->objective_type == OBJECTIVE_MINIMIZE) {
        return fraction_sign(value) > 0;
    }
    return fraction_sign(value) < 0;
}

// Indica si candidate mejora Z mas que best: mas negativo al maximizar, mas positivo al minimizar
static int improves_more(const Table *table, Fraction candidate, Fraction best) {
    if (table->objective_type == OBJECTIVE_MINIMIZE) {
        return fraction_compare(candidate, best) > 0;
    }
    return fraction_compare(candidate, best) < 0;
}

// =============================================
// Engine pieces
// ---------------------------------------------

// Elige la columna que entra: la que mas mejora Z segun maximizar o minimizar
int simplex_choose_entering_column(const Table *table) {
    int entering_column = -1;

    // Solo las columnas de variables: se excluyen Z (primera) y el lado derecho (ultima)
    for (int column = 1; column < table->column_count - 1; column++) {
        Fraction value = table->cells[0][column];
        if (improves_objective(table, value)) {
            // Con empate se conserva la de menor indice: solo se reemplaza si mejora estrictamente
            if (entering_column == -1 || improves_more(table, value, table->cells[0][entering_column])) {
                entering_column = column;
            }
        }
    }
    return entering_column;
}

// Elige la fila que sale: la de fraccion minima, con desempate por menor indice de variable basica
SimplexStatus simplex_choose_leaving_constraint(const Table *table, int entering_column, Step *step) {
    int constraint_count = table->row_count - 1;
    int right_hand_side_column = table->column_count - 1;
    int minimum_constraint = -1;

    step->leaving_constraint = -1;
    step->tied_row_count = 0;

    // Fraccion de cada fila: solo cuenta si el coeficiente de la columna que entra es positivo
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        step->ratio_is_valid[constraint] = 0;
        Fraction coefficient = table->cells[constraint + 1][entering_column];
        if (fraction_sign(coefficient) > 0) {
            FractionStatus status = fraction_divide(table->cells[constraint + 1][right_hand_side_column],
                                                    coefficient, &step->ratios[constraint]);
            if (status != FRACTION_OK) {
                return SIMPLEX_ERROR_OVERFLOW;
            }
            step->ratio_is_valid[constraint] = 1;

            // Recordar la fila con la fraccion mas pequena hasta ahora
            if (minimum_constraint == -1 ||
                fraction_compare(step->ratios[constraint], step->ratios[minimum_constraint]) < 0) {
                minimum_constraint = constraint;
            }
        }
    }

    // Sin fracciones validas la columna puede crecer sin limite
    if (minimum_constraint == -1) {
        return SIMPLEX_UNBOUNDED;
    }

    // Entre las filas empatadas en el minimo sale la de variable basica con menor indice de columna
    for (int constraint = 0; constraint < constraint_count; constraint++) {
        if (step->ratio_is_valid[constraint] &&
            fraction_compare(step->ratios[constraint], step->ratios[minimum_constraint]) == 0) {
            step->tied_row_count++;
            if (step->leaving_constraint == -1 ||
                table->basis[constraint] < table->basis[step->leaving_constraint]) {
                step->leaving_constraint = constraint;
            }
        }
    }
    return SIMPLEX_RUNNING;
}

// Pivotea: deja 1 en el pivote y 0 en el resto de su columna
SimplexStatus simplex_pivot(Table *table, int leaving_constraint, int entering_column) {
    int pivot_row = leaving_constraint + 1;
    Fraction pivot = table->cells[pivot_row][entering_column];

    // Dividir toda la fila del pivote entre el pivote
    for (int column = 0; column < table->column_count; column++) {
        FractionStatus status = fraction_divide(table->cells[pivot_row][column], pivot,
                                                &table->cells[pivot_row][column]);
        if (status != FRACTION_OK) {
            return SIMPLEX_ERROR_OVERFLOW;
        }
    }

    // A cada otra fila (incluida la objetivo) restarle su valor en la columna por la fila del pivote
    for (int row = 0; row < table->row_count; row++) {
        Fraction factor = table->cells[row][entering_column];
        if (row == pivot_row || fraction_sign(factor) == 0) {
            continue;
        }
        for (int column = 0; column < table->column_count; column++) {
            Fraction product;
            FractionStatus status = fraction_multiply(factor, table->cells[pivot_row][column], &product);
            if (status != FRACTION_OK) {
                return SIMPLEX_ERROR_OVERFLOW;
            }
            status = fraction_subtract(table->cells[row][column], product, &table->cells[row][column]);
            if (status != FRACTION_OK) {
                return SIMPLEX_ERROR_OVERFLOW;
            }
        }
    }

    // La variable que entra reemplaza a la que sale en la base
    table->basis[leaving_constraint] = entering_column;
    return SIMPLEX_RUNNING;
}

// Lee la solucion de una tabla: las no basicas valen 0 y las basicas el lado derecho de su fila
static void read_solution(const Problem *problem, const Table *table, Fraction variable_values[],
                          Fraction slack_values[], Fraction *objective_value) {
    int right_hand_side_column = table->column_count - 1;

    // Todo en cero para las variables que no estan en la base
    for (int variable = 0; variable < problem->variable_count; variable++) {
        variable_values[variable] = fraction_from_int(0);
    }
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        slack_values[slack] = fraction_from_int(0);
    }

    // Cada variable basica toma el valor del lado derecho de su fila
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        int column = table->basis[constraint];
        Fraction value = table->cells[constraint + 1][right_hand_side_column];
        if (table->column_types[column] == COLUMN_ORIGINAL) {
            variable_values[column - 1] = value;
        } else {
            slack_values[column - problem->variable_count - 1] = value;
        }
    }

    // El lado derecho de la fila objetivo es el valor de Z
    *objective_value = table->cells[0][right_hand_side_column];
}

// Extrae la solucion de la tabla optima hacia el resultado
SimplexStatus simplex_extract_solution(const Problem *problem, const Table *table, SimplexResult *result) {
    read_solution(problem, table, result->variable_values, result->slack_values, &result->objective_value);
    return SIMPLEX_OPTIMAL;
}

// Indica si la columna es de una variable basica
static int is_basic_column(const Table *table, int column) {
    for (int constraint = 0; constraint < table->row_count - 1; constraint++) {
        if (table->basis[constraint] == column) {
            return 1;
        }
    }
    return 0;
}

// Indica si dos soluciones tienen los mismos valores en las variables originales
static int same_point(const Fraction first[], const Fraction second[], int variable_count) {
    for (int variable = 0; variable < variable_count; variable++) {
        if (fraction_compare(first[variable], second[variable]) != 0) {
            return 0;
        }
    }
    return 1;
}

// Direccion del rayo: la variable que entra crece 1 y cada basica original baja su coeficiente en esa columna
static void build_ray_direction(const Problem *problem, const Table *table, int entering_column,
                                SimplexResult *result) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        result->ray_direction[variable] = fraction_from_int(0);
    }
    if (table->column_types[entering_column] == COLUMN_ORIGINAL) {
        result->ray_direction[entering_column - 1] = fraction_from_int(1);
    }
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        int column = table->basis[constraint];
        if (table->column_types[column] == COLUMN_ORIGINAL) {
            result->ray_direction[column - 1] = fraction_negate(table->cells[constraint + 1][entering_column]);
        }
    }
}

// Busca soluciones multiples: una variable no basica con 0 en la fila Z permite pivotear una vez mas.
// Prueba las columnas en orden de indice y se queda con la primera que lleva a otro punto; si ninguna
// lo hace pero alguna no tiene fracciones validas, las soluciones forman un rayo
static SimplexStatus find_alternative_solution(const Problem *problem, const Table *optimal_table,
                                               SimplexResult *result) {
    Step candidate;
    Step ray_step;
    int ray_column = -1;
    Fraction objective_value;

    for (int column = 1; column < optimal_table->column_count - 1; column++) {
        // Solo variables no basicas con 0 en la fila objetivo
        if (is_basic_column(optimal_table, column) || fraction_sign(optimal_table->cells[0][column]) != 0) {
            continue;
        }
        start_step(&candidate, optimal_table);
        candidate.entering_column = column;

        // Sin fracciones validas la columna no lleva a otra tabla: se recuerda el primer rayo
        SimplexStatus status = simplex_choose_leaving_constraint(optimal_table, column, &candidate);
        if (status == SIMPLEX_UNBOUNDED) {
            if (ray_column == -1) {
                ray_column = column;
                ray_step = candidate;
            }
            continue;
        }
        if (status != SIMPLEX_RUNNING) {
            return status;
        }
        candidate.pivot = optimal_table->cells[candidate.leaving_constraint + 1][column];

        // Pivotear una copia y leer la segunda solucion
        Table next_table = *optimal_table;
        status = simplex_pivot(&next_table, candidate.leaving_constraint, column);
        if (status != SIMPLEX_RUNNING) {
            return status;
        }
        read_solution(problem, &next_table, result->alternative_variable_values, result->alternative_slack_values,
                      &objective_value);

        // Con fraccion 0 el pivoteo cae en el mismo punto (degenerado): se prueba la siguiente columna
        if (same_point(result->variable_values, result->alternative_variable_values, problem->variable_count)) {
            continue;
        }
        result->multiple_kind = MULTIPLE_SEGMENT;
        result->alternative_step = candidate;
        result->alternative_table = next_table;
        return SIMPLEX_RUNNING;
    }

    // Sin segundo punto: puede quedar un rayo de soluciones o una solucion unica
    for (int variable = 0; variable < problem->variable_count; variable++) {
        result->alternative_variable_values[variable] = fraction_from_int(0);
    }
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        result->alternative_slack_values[slack] = fraction_from_int(0);
    }
    if (ray_column != -1) {
        result->multiple_kind = MULTIPLE_RAY;
        result->alternative_step = ray_step;
        build_ray_direction(problem, optimal_table, ray_column, result);
    }
    return SIMPLEX_RUNNING;
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

    // La solucion empieza en cero para que nunca contenga fracciones invalidas si no hay optimo
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        result->variable_values[variable] = fraction_from_int(0);
    }
    for (int slack = 0; slack < MAX_CONSTRAINTS; slack++) {
        result->slack_values[slack] = fraction_from_int(0);
    }
    result->objective_value = fraction_from_int(0);
    for (int variable = 0; variable < MAX_VARIABLES; variable++) {
        result->alternative_variable_values[variable] = fraction_from_int(0);
        result->ray_direction[variable] = fraction_from_int(0);
    }
    for (int slack = 0; slack < MAX_CONSTRAINTS; slack++) {
        result->alternative_slack_values[slack] = fraction_from_int(0);
    }
    result->multiple_kind = MULTIPLE_NONE;

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

        // Si ningun valor de la fila objetivo mejora Z la tabla es optima
        int entering_column = simplex_choose_entering_column(&table);
        if (entering_column == -1) {
            status = simplex_extract_solution(problem, &table, result);

            // Con el optimo, revisar si hay soluciones multiples (un error de desbordamiento reemplaza al optimo)
            SimplexStatus alternative_status = find_alternative_solution(problem, &table, result);
            if (alternative_status != SIMPLEX_RUNNING) {
                status = alternative_status;
            }
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

// Calcula un punto del segmento o del rayo de soluciones optimas
SimplexStatus simplex_alternative_point(const SimplexResult *result, int variable_count, Fraction parameter,
                                        Fraction values[]) {
    Fraction first_part;
    Fraction second_part;
    Fraction complement;

    if (result->multiple_kind == MULTIPLE_NONE) {
        return SIMPLEX_ERROR_INVALID_PROBLEM;
    }

    for (int variable = 0; variable < variable_count; variable++) {
        if (result->multiple_kind == MULTIPLE_SEGMENT) {
            // parameter * x1 + (1 - parameter) * x2
            if (fraction_subtract(fraction_from_int(1), parameter, &complement) != FRACTION_OK ||
                fraction_multiply(parameter, result->variable_values[variable], &first_part) != FRACTION_OK ||
                fraction_multiply(complement, result->alternative_variable_values[variable], &second_part) !=
                    FRACTION_OK) {
                return SIMPLEX_ERROR_OVERFLOW;
            }
        } else {
            // x1 + parameter * direccion
            first_part = result->variable_values[variable];
            if (fraction_multiply(parameter, result->ray_direction[variable], &second_part) != FRACTION_OK) {
                return SIMPLEX_ERROR_OVERFLOW;
            }
        }
        if (fraction_add(first_part, second_part, &values[variable]) != FRACTION_OK) {
            return SIMPLEX_ERROR_OVERFLOW;
        }
    }
    return SIMPLEX_RUNNING;
}

// Libera un resultado
void simplex_result_free(SimplexResult *result) {
    free(result);
}

// Mensaje para el usuario del estado del resultado
const char *simplex_status_message(SimplexStatus status) {
    switch (status) {
    case SIMPLEX_RUNNING:
        return "El Simplex todavía está en ejecución.";
    case SIMPLEX_OPTIMAL:
        return "Se encontró una solución óptima.";
    case SIMPLEX_UNBOUNDED:
        return "El problema es no acotado: Z puede mejorar sin límite.";
    case SIMPLEX_ITERATION_LIMIT:
        return "Se alcanzó el límite de iteraciones sin encontrar una solución.";
    case SIMPLEX_ERROR_OVERFLOW:
        return "Un número creció demasiado para representarlo de forma exacta.";
    case SIMPLEX_ERROR_INVALID_PROBLEM:
        return "El problema no es válido: revise las cantidades y los lados derechos.";
    case SIMPLEX_ERROR_UNSUPPORTED:
        return "Esta versión solo acepta restricciones <=.";
    }
    return "Estado desconocido del Simplex.";
}
