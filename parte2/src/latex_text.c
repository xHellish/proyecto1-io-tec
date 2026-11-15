#include "latex_text.h"

// =============================================
// Public functions
// ---------------------------------------------

// Fraccion en modo matematico
void latex_append_fraction(GString *output, Fraction value) {
    if (value.denominator == 1) {
        g_string_append_printf(output, "%lld", value.numerator);
        return;
    }
    if (value.numerator < 0) {
        g_string_append_printf(output, "-\\frac{%lld}{%lld}", -value.numerator, value.denominator);
        return;
    }
    g_string_append_printf(output, "\\frac{%lld}{%lld}", value.numerator, value.denominator);
}

// Decimal con punto, sin depender del idioma del sistema
void latex_append_decimal(GString *output, double value) {
    char buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, sizeof(buffer), "%.3f", value);
    g_string_append(output, buffer);
}

// Una restriccion como "3 x1 + 2 x2 <= 12"
void latex_append_constraint(GString *output, const Problem *problem, int constraint) {
    int is_first = 1;

    // Terminos con coeficiente distinto de cero
    for (int variable = 0; variable < problem->variable_count; variable++) {
        Fraction coefficient = problem->coefficients[constraint][variable];
        int sign = fraction_sign(coefficient);
        if (sign == 0) {
            continue;
        }
        if (sign < 0) {
            g_string_append(output, "$-$ ");
        } else if (!is_first) {
            g_string_append(output, "$+$ ");
        }
        Fraction magnitude = coefficient;
        if (sign < 0) {
            magnitude = fraction_negate(coefficient);
        }
        if (magnitude.numerator != 1 || magnitude.denominator != 1) {
            g_string_append(output, "$");
            latex_append_fraction(output, magnitude);
            g_string_append(output, "\\,$");
        }
        g_string_append(output, problem->variable_names[variable]);
        g_string_append(output, " ");
        is_first = 0;
    }
    if (is_first) {
        g_string_append(output, "$0$ ");
    }

    // Relacion y lado derecho
    if (problem->constraint_types[constraint] == CONSTRAINT_GREATER_EQUAL) {
        g_string_append(output, "$\\geq$ $");
    } else if (problem->constraint_types[constraint] == CONSTRAINT_EQUAL) {
        g_string_append(output, "$=$ $");
    } else {
        g_string_append(output, "$\\leq$ $");
    }
    latex_append_fraction(output, problem->right_hand_sides[constraint]);
    g_string_append(output, "$");
}
