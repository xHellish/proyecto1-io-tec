#include "geometry.h"

#include <stdlib.h>

// =============================================
// Constants
// ---------------------------------------------
// Tamaño de caja que se usa si los cálculos exactos no caben
#define FALLBACK_BOX_LIMIT 10

// =============================================
// Helpers
// ---------------------------------------------

// Evalúa la suma de coeficientes por coordenadas; devuelve 0 si algún cálculo desborda
static int evaluate_half_space(const HalfSpace *space, const Vertex *vertex, int dimension, Fraction *total) {
    *total = fraction_from_int(0);
    for (int axis = 0; axis < dimension; axis++) {
        Fraction product;
        if (fraction_multiply(space->coefficients[axis], vertex->coordinates[axis], &product) != FRACTION_OK ||
            fraction_add(*total, product, total) != FRACTION_OK) {
            return 0;
        }
    }
    return 1;
}

// Indica si un vértice cumple un semiespacio: 1 si cumple, 0 si no y -1 si el cálculo desborda
static int satisfies_half_space(const HalfSpace *space, const Vertex *vertex, int dimension) {
    Fraction total;
    if (!evaluate_half_space(space, vertex, dimension, &total)) {
        return -1;
    }
    int comparison = fraction_compare(total, space->right_hand_side);
    if (space->type == CONSTRAINT_LESS_EQUAL) {
        return comparison <= 0;
    }
    if (space->type == CONSTRAINT_GREATER_EQUAL) {
        return comparison >= 0;
    }
    return comparison == 0;
}

// Resuelve el sistema de los planos elegidos con Gauss y fracciones: 1 si hay un único punto,
// 0 si el sistema es singular y -1 si un cálculo desborda
static int solve_chosen_planes(const HalfSpace spaces[], const int chosen[], int dimension, Vertex *vertex) {
    Fraction matrix[GEOMETRY_MAX_DIMENSION][GEOMETRY_MAX_DIMENSION + 1];

    // Una fila por plano: coeficientes y lado derecho
    for (int row = 0; row < dimension; row++) {
        for (int axis = 0; axis < dimension; axis++) {
            matrix[row][axis] = spaces[chosen[row]].coefficients[axis];
        }
        matrix[row][dimension] = spaces[chosen[row]].right_hand_side;
    }

    for (int column = 0; column < dimension; column++) {
        // Buscar una fila con pivote distinto de cero
        int pivot_row = -1;
        for (int row = column; row < dimension; row++) {
            if (fraction_sign(matrix[row][column]) != 0) {
                pivot_row = row;
                break;
            }
        }
        if (pivot_row == -1) {
            return 0;
        }
        for (int index = 0; index <= dimension; index++) {
            Fraction swap = matrix[column][index];
            matrix[column][index] = matrix[pivot_row][index];
            matrix[pivot_row][index] = swap;
        }

        // Dejar 1 en el pivote y 0 en el resto de la columna
        Fraction pivot = matrix[column][column];
        for (int index = 0; index <= dimension; index++) {
            if (fraction_divide(matrix[column][index], pivot, &matrix[column][index]) != FRACTION_OK) {
                return -1;
            }
        }
        for (int row = 0; row < dimension; row++) {
            Fraction factor = matrix[row][column];
            if (row == column || fraction_sign(factor) == 0) {
                continue;
            }
            for (int index = 0; index <= dimension; index++) {
                Fraction product;
                if (fraction_multiply(factor, matrix[column][index], &product) != FRACTION_OK ||
                    fraction_subtract(matrix[row][index], product, &matrix[row][index]) != FRACTION_OK) {
                    return -1;
                }
            }
        }
    }

    for (int axis = 0; axis < dimension; axis++) {
        vertex->coordinates[axis] = matrix[axis][dimension];
    }
    return 1;
}

// Indica si el vértice ya está en la lista
static int vertex_is_listed(const Vertex vertices[], int vertex_count, const Vertex *candidate, int dimension) {
    for (int index = 0; index < vertex_count; index++) {
        int is_equal = 1;
        for (int axis = 0; axis < dimension; axis++) {
            if (fraction_compare(vertices[index].coordinates[axis], candidate->coordinates[axis]) != 0) {
                is_equal = 0;
            }
        }
        if (is_equal) {
            return 1;
        }
    }
    return 0;
}

// Agrega un semiespacio x_axis (relación) valor
static void add_axis_half_space(HalfSpace *space, int axis, ConstraintType type, Fraction value) {
    for (int index = 0; index < GEOMETRY_MAX_DIMENSION; index++) {
        space->coefficients[index] = fraction_from_int(0);
    }
    space->coefficients[axis] = fraction_from_int(1);
    space->type = type;
    space->right_hand_side = value;
}

// =============================================
// Public functions
// ---------------------------------------------

// Restricciones del problema, x_j >= 0 y, si se pide, la caja del gráfico
int geometry_build_half_spaces(const Problem *problem, const Fraction box_limits[], HalfSpace spaces[]) {
    int count = 0;

    // Restricciones del problema
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        for (int axis = 0; axis < GEOMETRY_MAX_DIMENSION; axis++) {
            spaces[count].coefficients[axis] = fraction_from_int(0);
            if (axis < problem->variable_count) {
                spaces[count].coefficients[axis] = problem->coefficients[constraint][axis];
            }
        }
        spaces[count].type = problem->constraint_types[constraint];
        spaces[count].right_hand_side = problem->right_hand_sides[constraint];
        count++;
    }

    // No negatividad de cada variable
    for (int axis = 0; axis < problem->variable_count; axis++) {
        add_axis_half_space(&spaces[count], axis, CONSTRAINT_GREATER_EQUAL, fraction_from_int(0));
        count++;
    }

    // Caja del gráfico
    if (box_limits != NULL) {
        for (int axis = 0; axis < problem->variable_count; axis++) {
            add_axis_half_space(&spaces[count], axis, CONSTRAINT_LESS_EQUAL, box_limits[axis]);
            count++;
        }
    }
    return count;
}

// Prueba todas las combinaciones de dimension planos y se queda con los puntos que cumplen todo
int geometry_find_vertices(const HalfSpace spaces[], int space_count, int dimension, Vertex vertices[],
                           int *had_overflow) {
    int vertex_count = 0;
    int chosen[GEOMETRY_MAX_DIMENSION];

    *had_overflow = 0;
    if (space_count < dimension) {
        return 0;
    }

    // Primera combinación: los primeros planos
    for (int index = 0; index < dimension; index++) {
        chosen[index] = index;
    }

    while (1) {
        Vertex candidate;
        int solve_status = solve_chosen_planes(spaces, chosen, dimension, &candidate);
        if (solve_status < 0) {
            *had_overflow = 1;
        }

        if (solve_status > 0) {
            // El punto debe cumplir todas las restricciones, no solo los planos elegidos
            int is_feasible = 1;
            for (int space = 0; space < space_count && is_feasible; space++) {
                int result = satisfies_half_space(&spaces[space], &candidate, dimension);
                if (result < 0) {
                    *had_overflow = 1;
                }
                if (result != 1) {
                    is_feasible = 0;
                }
            }
            if (is_feasible && !vertex_is_listed(vertices, vertex_count, &candidate, dimension) &&
                vertex_count < GEOMETRY_MAX_VERTICES) {
                vertices[vertex_count] = candidate;
                vertex_count++;
            }
        }

        // Siguiente combinación en orden lexicográfico
        int position = dimension - 1;
        while (position >= 0 && chosen[position] == space_count - dimension + position) {
            position--;
        }
        if (position < 0) {
            break;
        }
        chosen[position]++;
        for (int index = position + 1; index < dimension; index++) {
            chosen[index] = chosen[index - 1] + 1;
        }
    }
    return vertex_count;
}

// Tamaño de la caja de cada eje: un cuarto más que la coordenada más grande de los vértices
void geometry_box_limits(const Problem *problem, Fraction box_limits[]) {
    int dimension = problem->variable_count;
    HalfSpace spaces[GEOMETRY_MAX_HALF_SPACES];
    Fraction largest[GEOMETRY_MAX_DIMENSION];
    int had_overflow = 0;

    // Sin caja los vértices son solo los de la región (puede ser que no haya ninguno)
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);
    int space_count = geometry_build_half_spaces(problem, NULL, spaces);
    int vertex_count = 0;
    if (vertices != NULL) {
        vertex_count = geometry_find_vertices(spaces, space_count, dimension, vertices, &had_overflow);
    }

    // Coordenada más grande de los vértices
    for (int axis = 0; axis < dimension; axis++) {
        largest[axis] = fraction_from_int(0);
        for (int index = 0; index < vertex_count; index++) {
            if (fraction_compare(vertices[index].coordinates[axis], largest[axis]) > 0) {
                largest[axis] = vertices[index].coordinates[axis];
            }
        }
    }
    free(vertices);

    // Cortes de cada restricción con los ejes (lado derecho / coeficiente, si es positivo): solo cuentan en
    // un eje donde los vértices no dan tamaño (no hay región o solo toca el origen), porque un corte lejano
    // achicaría la región en el dibujo
    for (int axis = 0; axis < dimension; axis++) {
        if (fraction_sign(largest[axis]) > 0) {
            continue;
        }
        for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
            Fraction intercept;
            if (fraction_sign(problem->coefficients[constraint][axis]) == 0) {
                continue;
            }
            if (fraction_divide(problem->right_hand_sides[constraint], problem->coefficients[constraint][axis],
                                &intercept) != FRACTION_OK) {
                continue;
            }
            if (fraction_compare(intercept, largest[axis]) > 0) {
                largest[axis] = intercept;
            }
        }
    }

    // Un cuarto más que lo más grande, redondeado hacia arriba a un entero, y al menos 1
    for (int axis = 0; axis < dimension; axis++) {
        Fraction margin;
        box_limits[axis] = fraction_from_int(FALLBACK_BOX_LIMIT);
        if (fraction_multiply(largest[axis], (Fraction){5, 4}, &margin) != FRACTION_OK) {
            continue;
        }
        long long limit = (margin.numerator + margin.denominator - 1) / margin.denominator;
        if (limit < 1) {
            limit = 1;
        }
        box_limits[axis] = fraction_from_int(limit);
    }
}

// Indica si el vértice está sobre el plano del semiespacio
int geometry_vertex_is_on_plane(const HalfSpace *space, const Vertex *vertex, int dimension) {
    Fraction total;
    if (!evaluate_half_space(space, vertex, dimension, &total)) {
        return 0;
    }
    return fraction_compare(total, space->right_hand_side) == 0;
}

// Fracción a double
double geometry_to_double(Fraction value) {
    return (double)value.numerator / (double)value.denominator;
}
