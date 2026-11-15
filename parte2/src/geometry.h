#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "fraction.h"
#include "problem.h"

// =============================================
// Constants
// ---------------------------------------------
// Los gráficos solo existen para 2 y 3 variables
#define GEOMETRY_MAX_DIMENSION 3

// Restricciones del problema, una por variable para x >= 0 y una por variable para la caja del gráfico
#define GEOMETRY_MAX_HALF_SPACES (MAX_CONSTRAINTS + 2 * GEOMETRY_MAX_DIMENSION)

// Vértices que se guardan como máximo (las combinaciones posibles son menos de 1500)
#define GEOMETRY_MAX_VERTICES 1600

// =============================================
// Types
// ---------------------------------------------

// Semiespacio (o plano si es una igualdad): coeficientes por las variables, relación y lado derecho
typedef struct {
    Fraction coefficients[GEOMETRY_MAX_DIMENSION];
    ConstraintType type;
    Fraction right_hand_side;
} HalfSpace;

// Punto exacto con una coordenada por variable
typedef struct {
    Fraction coordinates[GEOMETRY_MAX_DIMENSION];
} Vertex;

// =============================================
// Functions
// ---------------------------------------------

// Arma los semiespacios de un problema de 2 o 3 variables: sus restricciones y x_j >= 0. Si box_limits
// no es NULL agrega x_j <= box_limits[j], una caja que encierra la región para poder dibujarla aunque
// no esté acotada. Devuelve la cantidad de semiespacios
int geometry_build_half_spaces(const Problem *problem, const Fraction box_limits[], HalfSpace spaces[]);

// Busca todos los vértices de la región: los puntos donde se cortan dimension planos y que cumplen todo.
// Devuelve la cantidad de vértices sin repetir; en had_overflow deja 1 si algún cálculo exacto no cupo
int geometry_find_vertices(const HalfSpace spaces[], int space_count, int dimension, Vertex vertices[],
                           int *had_overflow);

// Calcula el tamaño de la caja de cada eje: un poco más que la coordenada más grande entre los vértices
// de la región. Si en un eje los vértices no dan tamaño (no hay región o solo toca el origen) usa los
// cortes de las restricciones con ese eje. Siempre al menos 1
void geometry_box_limits(const Problem *problem, Fraction box_limits[]);

// Indica si el vértice está sobre el plano (cumple la igualdad de ese semiespacio)
int geometry_vertex_is_on_plane(const HalfSpace *space, const Vertex *vertex, int dimension);

// Pasa una fracción a double para dibujar
double geometry_to_double(Fraction value);

#endif
