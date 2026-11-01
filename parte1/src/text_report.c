#include "text_report.h"

#include <stdio.h>
#include <string.h>

// =============================================
// Constants
// ---------------------------------------------
// Ancho minimo de cada columna de la tabla
#define MINIMUM_CELL_WIDTH 5

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

// Agrega un paso del historial: su tabla y las decisiones que se tomaron sobre ella
static void append_step(GString *output, const SimplexResult *result, const Problem *problem, int step_index) {
    const Step *step = &result->steps[step_index];
    char label[NAME_BUFFER_SIZE];
    char pivot_text[FRACTION_STRING_SIZE];
    char ratio_text[FRACTION_STRING_SIZE];
    int is_last_step = (step_index == result->step_count - 1);

    g_string_append_printf(output, "Tabla %d\n", step_index);
    append_table(output, &step->table, problem->variable_count);

    // Columna que entra, segun la regla de maximizar o minimizar
    if (step->entering_column == -1) {
        g_string_append(output, "Entra: ninguna (la tabla es óptima)\n\n");
        return;
    }
    write_column_label(&step->table, problem->variable_count, step->entering_column, label, NAME_BUFFER_SIZE);
    if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append_printf(output, "Entra: %s (valor más positivo de la fila Z)\n", label);
    } else {
        g_string_append_printf(output, "Entra: %s (valor más negativo de la fila Z)\n", label);
    }

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
    } else if (is_last_step && result->status == SIMPLEX_UNBOUNDED) {
        g_string_append(output, "Sale: ninguna (la columna no tiene coeficientes positivos)\n");
    }
    g_string_append(output, "\n");
}

// Agrega la solucion final de un problema optimo
static void append_solution(GString *output, const SimplexResult *result, const Problem *problem) {
    char value_text[FRACTION_STRING_SIZE];

    fraction_to_string(result->objective_value, value_text, FRACTION_STRING_SIZE);
    g_string_append_printf(output, "Solución: Z = %s\n", value_text);
    for (int variable = 0; variable < problem->variable_count; variable++) {
        fraction_to_string(result->variable_values[variable], value_text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, "  x%d = %s\n", variable + 1, value_text);
    }
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        fraction_to_string(result->slack_values[slack], value_text, FRACTION_STRING_SIZE);
        g_string_append_printf(output, "  s%d = %s\n", slack + 1, value_text);
    }
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
        append_solution(output, result, problem);
    }
}
