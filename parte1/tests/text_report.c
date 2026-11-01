#include "text_report.h"

#include <stdio.h>
#include <string.h>

// =============================================
// Constants
// ---------------------------------------------
// Ancho minimo de cada columna de la tabla
#define MINIMUM_CELL_WIDTH 5

// Soluciones adicionales que se reportan cuando hay soluciones multiples
#define EXTRA_POINT_COUNT 3

// =============================================
// Helpers
// ---------------------------------------------

// Escribe el nombre corto de una columna: Z, x1, s1 o LD (lado derecho)
static void write_column_label(const Table *table, int variable_count, int column, char *buffer, size_t buffer_size) {
    ColumnType type = table->column_types[column];

    if (type == COLUMN_Z) {
        snprintf(buffer, buffer_size, "Z");
        return;
    }
    if (type == COLUMN_ORIGINAL) {
        snprintf(buffer, buffer_size, "x%d", column);
        return;
    }
    if (type == COLUMN_SLACK) {
        snprintf(buffer, buffer_size, "s%d", column - variable_count);
        return;
    }
    snprintf(buffer, buffer_size, "LD");
}

// Agrega un termino "+ 3x1" o "- 1/2x2" de una ecuacion; los coeficientes en 0 se omiten
static void append_term(GString *output, Fraction coefficient, int variable, int *is_first) {
    char text[FRACTION_STRING_SIZE];
    int sign = fraction_sign(coefficient);

    if (sign == 0) {
        return;
    }

    // Signo del termino: el primero solo lo lleva si es negativo
    if (*is_first) {
        if (sign < 0) {
            g_string_append(output, "-");
        }
    } else {
        if (sign < 0) {
            g_string_append(output, " - ");
        } else {
            g_string_append(output, " + ");
        }
    }

    // Valor absoluto del coeficiente; el 1 no se escribe
    Fraction magnitude = coefficient;
    if (sign < 0) {
        magnitude = fraction_negate(coefficient);
    }
    if (magnitude.numerator != 1 || magnitude.denominator != 1) {
        fraction_to_string(magnitude, text, FRACTION_STRING_SIZE);
        g_string_append(output, text);
    }
    g_string_append_printf(output, "x%d", variable + 1);
    *is_first = 0;
}

// Agrega el lado izquierdo de una ecuacion lineal, o 0 si todos los coeficientes son 0
static void append_linear_expression(GString *output, const Fraction coefficients[], int variable_count) {
    int is_first = 1;

    for (int variable = 0; variable < variable_count; variable++) {
        append_term(output, coefficients[variable], variable, &is_first);
    }
    if (is_first) {
        g_string_append(output, "0");
    }
}

// Agrega el problema original: objetivo, restricciones y no negatividad
static void append_problem(GString *output, const Problem *problem) {
    char text[FRACTION_STRING_SIZE];

    g_string_append_printf(output, "Problema: %s\n", problem->name);

    // Funcion objetivo
    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "Minimizar Z = ");
    } else {
        g_string_append(output, "Maximizar Z = ");
    }
    append_linear_expression(output, problem->objective, problem->variable_count);
    g_string_append(output, "\nSujeto a:\n");

    // Restricciones (en esta version todas son <=)
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        g_string_append(output, "  ");
        append_linear_expression(output, problem->coefficients[constraint], problem->variable_count);
        fraction_to_string(problem->right_hand_sides[constraint], text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, " <= %s\n", text);
    }
    g_string_append(output, "  Todas las variables >= 0\n\n");
}

// Agrega la tabla alineada: encabezado con las columnas y, en cada fila, su variable basica
static void append_table(GString *output, const Table *table, int variable_count) {
    char labels[MAX_TABLE_COLUMNS][NAME_BUFFER_SIZE];
    char cells[MAX_TABLE_ROWS][MAX_TABLE_COLUMNS][FRACTION_STRING_SIZE];
    char row_label[NAME_BUFFER_SIZE];
    int width = MINIMUM_CELL_WIDTH;

    // Textos de encabezados y celdas, y el ancho de la columna mas ancha
    for (int column = 0; column < table->column_count; column++) {
        write_column_label(table, variable_count, column, labels[column], NAME_BUFFER_SIZE);
        if ((int)strlen(labels[column]) > width) {
            width = (int)strlen(labels[column]);
        }
    }
    for (int row = 0; row < table->row_count; row++) {
        for (int column = 0; column < table->column_count; column++) {
            fraction_to_string(table->cells[row][column], cells[row][column], FRACTION_STRING_SIZE);
            if ((int)strlen(cells[row][column]) > width) {
                width = (int)strlen(cells[row][column]);
            }
        }
    }

    // Encabezado
    g_string_append_printf(output, "%*s", width, "Base");
    for (int column = 0; column < table->column_count; column++) {
        g_string_append_printf(output, " %*s", width, labels[column]);
    }
    g_string_append(output, "\n");

    // Filas: la objetivo se llama Z y las demas llevan su variable basica
    for (int row = 0; row < table->row_count; row++) {
        if (row == 0) {
            snprintf(row_label, NAME_BUFFER_SIZE, "Z");
        } else {
            write_column_label(table, variable_count, table->basis[row - 1], row_label, NAME_BUFFER_SIZE);
        }
        g_string_append_printf(output, "%*s", width, row_label);
        for (int column = 0; column < table->column_count; column++) {
            g_string_append_printf(output, " %*s", width, cells[row][column]);
        }
        g_string_append(output, "\n");
    }
}

// Agrega las decisiones tomadas sobre una tabla: columna que entra, fracciones, fila que sale y empates.
// reason explica por que entra la columna; show_no_leaving indica si se explica que ninguna fila sale
static void append_decisions(GString *output, const Step *step, const Problem *problem, const char *reason,
                             int show_no_leaving) {
    char label[NAME_BUFFER_SIZE];
    char pivot_text[FRACTION_STRING_SIZE];
    char ratio_text[FRACTION_STRING_SIZE];

    write_column_label(&step->table, problem->variable_count, step->entering_column, label, NAME_BUFFER_SIZE);
    g_string_append_printf(output, "Entra: %s (%s)\n", label, reason);

    // Fila que sale, con sus fracciones y el empate si lo hubo
    if (step->leaving_constraint != -1) {
        g_string_append(output, "Fracciones:");
        for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
            if (step->ratio_is_valid[constraint]) {
                fraction_to_string(step->ratios[constraint], ratio_text, FRACTION_STRING_SIZE);
                g_string_append_printf(output, "  R%d = %s", constraint + 1, ratio_text);
            } else {
                g_string_append_printf(output, "  R%d = -", constraint + 1);
            }
        }
        g_string_append(output, "\n");

        write_column_label(&step->table, problem->variable_count,
                           step->table.basis[step->leaving_constraint], label, NAME_BUFFER_SIZE);
        fraction_to_string(step->pivot, pivot_text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, "Sale: %s (restricción %d), pivote %s\n",
                               label, step->leaving_constraint + 1, pivot_text);
        if (step->tied_row_count > 1) {
            g_string_append_printf(output,
                                   "Empate: %d filas tienen la fracción mínima; sale la variable básica de menor índice\n",
                                   step->tied_row_count);
        }

        // Fraccion minima 0: la variable entra valiendo 0 y Z no cambia
        if (fraction_sign(step->ratios[step->leaving_constraint]) == 0) {
            g_string_append(output, "Pivoteo degenerado: la fracción mínima es 0, así que Z no mejora con este pivoteo\n");
        }
    } else if (show_no_leaving) {
        g_string_append(output, "Sale: ninguna (la columna no tiene coeficientes positivos)\n");
    }
}

// Agrega la lista de variables basicas que valen 0 en una tabla (BFS degenerada)
static void append_zero_basics(GString *output, const Table *table, int variable_count) {
    char label[NAME_BUFFER_SIZE];
    int right_hand_side_column = table->column_count - 1;
    int is_first = 1;

    g_string_append(output, "BFS degenerada: variable básica en 0:");
    for (int row = 1; row < table->row_count; row++) {
        if (fraction_sign(table->cells[row][right_hand_side_column]) != 0) {
            continue;
        }
        write_column_label(table, variable_count, table->basis[row - 1], label, NAME_BUFFER_SIZE);
        if (is_first) {
            g_string_append_printf(output, " %s", label);
        } else {
            g_string_append_printf(output, ", %s", label);
        }
        is_first = 0;
    }
    g_string_append(output, "\n");
}

// Agrega la explicacion del problema degenerado y, si se detuvo en el limite, del ciclo
static void append_degenerate_summary(GString *output, const SimplexResult *result) {
    g_string_append(output, "\nProblema degenerado: al menos una tabla tiene una variable básica que vale 0. "
                            "En esos casos se puede pivotear sin que Z mejore, y existe el riesgo de ciclar.\n");
    if (result->status == SIMPLEX_ITERATION_LIMIT) {
        g_string_append_printf(output, "El Simplex repitió pivoteos sin mejorar Z y se detuvo después de %d "
                                       "pivoteos: el problema cicló con estas reglas.\n",
                               MAX_ITERATIONS);
    } else {
        g_string_append(output, "Aun así el Simplex logró salir y terminar.\n");
    }
}

// Agrega un paso del historial: su tabla y las decisiones que se tomaron sobre ella
static void append_step(GString *output, const SimplexResult *result, const Problem *problem, int step_index) {
    const Step *step = &result->steps[step_index];
    int is_last_step = (step_index == result->step_count - 1);

    g_string_append_printf(output, "Tabla %d\n", step_index);
    append_table(output, &step->table, problem->variable_count);
    if (step->zero_basic_count > 0) {
        append_zero_basics(output, &step->table, problem->variable_count);
    }

    // Columna que entra, segun la regla de maximizar o minimizar
    if (step->entering_column == -1) {
        g_string_append(output, "Entra: ninguna (la tabla es óptima)\n\n");
        return;
    }
    if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
        append_decisions(output, step, problem, "valor más positivo de la fila Z",
                         is_last_step && result->status == SIMPLEX_UNBOUNDED);
    } else {
        append_decisions(output, step, problem, "valor más negativo de la fila Z",
                         is_last_step && result->status == SIMPLEX_UNBOUNDED);
    }
    g_string_append(output, "\n");
}

// Agrega una solucion: Z, variables originales y holguras
static void append_solution(GString *output, const char *title, const Problem *problem,
                            const Fraction variable_values[], const Fraction slack_values[],
                            Fraction objective_value) {
    char value_text[FRACTION_STRING_SIZE];

    fraction_to_string(objective_value, value_text, FRACTION_STRING_SIZE);
    g_string_append_printf(output, "%s: Z = %s\n", title, value_text);
    for (int variable = 0; variable < problem->variable_count; variable++) {
        fraction_to_string(variable_values[variable], value_text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, "  x%d = %s\n", variable + 1, value_text);
    }
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        fraction_to_string(slack_values[slack], value_text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, "  s%d = %s\n", slack + 1, value_text);
    }
}

// Agrega un vector de variables originales como (a, b, c)
static void append_vector(GString *output, const Fraction values[], int variable_count) {
    char value_text[FRACTION_STRING_SIZE];

    g_string_append(output, "(");
    for (int variable = 0; variable < variable_count; variable++) {
        if (variable > 0) {
            g_string_append(output, ", ");
        }
        fraction_to_string(values[variable], value_text, FRACTION_STRING_SIZE);
        g_string_append(output, value_text);
    }
    g_string_append(output, ")");
}

// Agrega tres puntos adicionales de las soluciones multiples con su valor del parametro
static void append_extra_points(GString *output, const SimplexResult *result, const Problem *problem,
                                const char *parameter_name, const long long parameters[][2]) {
    Fraction parameter;
    Fraction values[MAX_VARIABLES];
    char parameter_text[FRACTION_STRING_SIZE];

    g_string_append(output, "Otras soluciones óptimas:\n");
    for (int index = 0; index < EXTRA_POINT_COUNT; index++) {
        // Valor del parametro y punto que genera; cualquier fallo se reporta en lugar del punto
        FractionStatus fraction_status = fraction_make(parameters[index][0], parameters[index][1], &parameter);
        if (fraction_status != FRACTION_OK) {
            g_string_append_printf(output, "  %s: %s\n", parameter_name, fraction_status_message(fraction_status));
            continue;
        }
        fraction_to_string(parameter, parameter_text, FRACTION_STRING_SIZE);
        if (simplex_alternative_point(result, problem->variable_count, parameter, values) != SIMPLEX_RUNNING) {
            g_string_append_printf(output, "  %s = %s: el punto no se puede representar de forma exacta\n",
                                   parameter_name, parameter_text);
            continue;
        }
        g_string_append_printf(output, "  %s = %s: ", parameter_name, parameter_text);
        append_vector(output, values, problem->variable_count);
        g_string_append(output, "\n");
    }
}

// Agrega las soluciones multiples: el pivoteo extra, la segunda tabla y la ecuacion que genera las demas
static void append_multiple_solutions(GString *output, const SimplexResult *result, const Problem *problem) {
    const Step *step = &result->alternative_step;
    char label[NAME_BUFFER_SIZE];
    const long long segment_parameters[EXTRA_POINT_COUNT][2] = {{1, 4}, {1, 2}, {3, 4}};
    const long long ray_parameters[EXTRA_POINT_COUNT][2] = {{1, 1}, {2, 1}, {3, 1}};

    write_column_label(&step->table, problem->variable_count, step->entering_column, label, NAME_BUFFER_SIZE);
    g_string_append_printf(output, "\nSoluciones múltiples: %s no es básica y tiene 0 en la fila Z, "
                                   "así que se pivotea una vez más\n",
                           label);
    append_decisions(output, step, problem, "0 en la fila Z", 1);

    // Segmento: segunda tabla final, segunda solucion y combinacion de ambas
    if (result->multiple_kind == MULTIPLE_SEGMENT) {
        g_string_append(output, "\nSegunda tabla final\n");
        append_table(output, &result->alternative_table, problem->variable_count);
        g_string_append(output, "\n");
        append_solution(output, "Segunda solución", problem, result->alternative_variable_values,
                        result->alternative_slack_values, result->objective_value);
        g_string_append(output, "\nTodas las soluciones óptimas: α");
        append_vector(output, result->variable_values, problem->variable_count);
        g_string_append(output, " + (1 - α)");
        append_vector(output, result->alternative_variable_values, problem->variable_count);
        g_string_append(output, ", con 0 ≤ α ≤ 1\n");
        append_extra_points(output, result, problem, "α", segment_parameters);
        return;
    }

    // Rayo: no hay segunda tabla, la variable puede crecer sin cambiar Z
    g_string_append_printf(output, "%s puede crecer sin límite y Z no cambia\n", label);
    g_string_append(output, "Todas las soluciones óptimas: ");
    append_vector(output, result->variable_values, problem->variable_count);
    g_string_append(output, " + t");
    append_vector(output, result->ray_direction, problem->variable_count);
    g_string_append(output, ", con t ≥ 0\n");
    append_extra_points(output, result, problem, "t", ray_parameters);
}

// =============================================
// Public functions
// ---------------------------------------------

// Agrega el reporte completo de una ejecucion del Simplex
void text_report_append(GString *output, const Problem *problem, const SimplexResult *result) {
    append_problem(output, problem);

    // Estado final y todas las tablas del historial
    g_string_append_printf(output, "Estado: %s\n", simplex_status_message(result->status));
    g_string_append_printf(output, "Tablas en el historial: %d\n\n", result->step_count);
    for (int step_index = 0; step_index < result->step_count; step_index++) {
        append_step(output, result, problem, step_index);
    }

    // La solucion solo existe si hubo optimo
    if (result->status == SIMPLEX_OPTIMAL) {
        append_solution(output, "Solución", problem, result->variable_values, result->slack_values,
                        result->objective_value);
        if (result->multiple_kind != MULTIPLE_NONE) {
            append_multiple_solutions(output, result, problem);
        }
    }

    // Explicacion del caso degenerado, con cualquier estado final
    if (result->is_degenerate) {
        append_degenerate_summary(output, result);
    }
}
