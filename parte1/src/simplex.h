#ifndef SIMPLEX_H
#define SIMPLEX_H

#include "fraction.h"
#include "problem.h"

// =============================================
// Constants
// ---------------------------------------------
// Pivoteos maximos antes de rendirse (evita que un ciclo cuelgue el programa)
#define MAX_ITERATIONS 200

// Un paso por pivoteo mas el estado final
#define MAX_STEPS (MAX_ITERATIONS + 1)

// Z + originales + holguras + (exceso y artificiales de la Parte 2) + lado derecho
#define MAX_TABLE_COLUMNS (1 + MAX_VARIABLES + 2 * MAX_CONSTRAINTS + 1)

// Fila objetivo + restricciones
#define MAX_TABLE_ROWS (MAX_CONSTRAINTS + 1)

// =============================================
// Types
// ---------------------------------------------
typedef enum {
    COLUMN_Z,
    COLUMN_ORIGINAL,
    COLUMN_SLACK,
    COLUMN_RIGHT_HAND_SIDE
} ColumnType;

// Estado del motor. SIMPLEX_RUNNING significa "todo bien, sigue" y es el unico
// valor que no termina la ejecucion
typedef enum {
    SIMPLEX_RUNNING = 0,
    SIMPLEX_OPTIMAL,
    SIMPLEX_UNBOUNDED,
    SIMPLEX_ITERATION_LIMIT,
    SIMPLEX_ERROR_OVERFLOW,
    SIMPLEX_ERROR_INVALID_PROBLEM,
    SIMPLEX_ERROR_UNSUPPORTED
} SimplexStatus;

// Tabla del Simplex en el formato de clase: columnas Z, originales, holguras y
// lado derecho (en ese orden); la fila 0 es la fila objetivo y las demas son restricciones
typedef struct {
    int row_count;
    int column_count;
    Fraction cells[MAX_TABLE_ROWS][MAX_TABLE_COLUMNS];
    ColumnType column_types[MAX_TABLE_COLUMNS];
    // Decide que valores de la fila objetivo mejoran Z: negativos al maximizar, positivos al minimizar
    ObjectiveType objective_type;
    // Columna de la variable basica de cada restriccion (la fila de la tabla es el indice + 1)
    int basis[MAX_CONSTRAINTS];
} Table;

// Soluciones multiples: ninguna, un segmento entre dos tablas finales o un rayo sin segundo vertice
typedef enum {
    MULTIPLE_NONE = 0,
    MULTIPLE_SEGMENT,
    MULTIPLE_RAY
} MultipleKind;

// Un paso del historial: la tabla con la que empieza y las decisiones tomadas sobre ella
typedef struct {
    Table table;
    // Columna que entra; -1 si la tabla ya es optima
    int entering_column;
    // Restriccion que sale (indice desde 0); -1 si ninguna fraccion es valida
    int leaving_constraint;
    Fraction pivot;
    // Fraccion (lado derecho / coeficiente) de cada restriccion; solo es valida si el coeficiente es positivo
    Fraction ratios[MAX_CONSTRAINTS];
    int ratio_is_valid[MAX_CONSTRAINTS];
    // Filas empatadas en la fraccion minima; mayor que 1 significa que se uso la regla de desempate
    int tied_row_count;
} Step;

// Resultado completo: estado, historial y solucion (la solucion solo se llena si es optimo)
typedef struct {
    SimplexStatus status;
    int step_count;
    Step steps[MAX_STEPS];
    Fraction variable_values[MAX_VARIABLES];
    Fraction slack_values[MAX_CONSTRAINTS];
    Fraction objective_value;

    // Soluciones multiples. alternative_step es la tabla optima con la columna de 0 que entra,
    // sus fracciones y el pivote. Con un segmento, alternative_table es la segunda tabla final y
    // alternative_*_values su solucion. Con un rayo no hay segunda tabla y ray_direction indica
    // cuanto cambia cada variable original por cada unidad que crece la variable que entra
    MultipleKind multiple_kind;
    Step alternative_step;
    Table alternative_table;
    Fraction alternative_variable_values[MAX_VARIABLES];
    Fraction alternative_slack_values[MAX_CONSTRAINTS];
    Fraction ray_direction[MAX_VARIABLES];
} SimplexResult;

// =============================================
// Public functions
// ---------------------------------------------

// Resuelve el problema; devuelve NULL si no hay memoria (liberar con simplex_result_free)
SimplexResult *simplex_solve(const Problem *problem);

// Libera un resultado
void simplex_result_free(SimplexResult *result);

// Mensaje para el usuario del estado del resultado
const char *simplex_status_message(SimplexStatus status);

// Calcula un punto optimo de las soluciones multiples (solo variables originales).
// Segmento: parameter * solucion 1 + (1 - parameter) * solucion 2, con 0 <= parameter <= 1.
// Rayo: solucion 1 + parameter * ray_direction, con parameter >= 0.
// Devuelve SIMPLEX_RUNNING si sale bien, SIMPLEX_ERROR_OVERFLOW si desborda y
// SIMPLEX_ERROR_INVALID_PROBLEM si el resultado no tiene soluciones multiples
SimplexStatus simplex_alternative_point(const SimplexResult *result, int variable_count, Fraction parameter,
                                        Fraction values[]);

// =============================================
// Engine pieces
// ---------------------------------------------
// Piezas internas del motor. Estan en el header para poder probarlas por separado.
// Las que devuelven SimplexStatus usan SIMPLEX_RUNNING como "salio bien"

// Devuelve la columna que entra. Al maximizar es la de valor mas negativo en la fila
// objetivo; al minimizar (segundo algoritmo de clase) es la de valor mas positivo.
// Con empate, la de menor indice. Devuelve -1 si ninguna mejora Z (tabla optima)
int simplex_choose_entering_column(const Table *table);

// Elige la restriccion que sale de la base. Debe llenar en el paso
// ratios, ratio_is_valid, tied_row_count y leaving_constraint. La fraccion de cada
// fila es lado derecho / coeficiente de la columna que entra, y solo cuenta si ese
// coeficiente es positivo. Gana la fraccion minima; con empate sale la fila cuya
// variable basica tenga el menor indice de columna. Devuelve SIMPLEX_UNBOUNDED si
// ninguna fraccion es valida y SIMPLEX_ERROR_OVERFLOW si una division desborda
SimplexStatus simplex_choose_leaving_constraint(const Table *table, int entering_column, Step *step);

// Pivotea la tabla: divide la fila del pivote entre el pivote, anula la
// columna en las demas filas (incluida la objetivo) y actualiza la base.
// Devuelve SIMPLEX_ERROR_OVERFLOW si alguna operacion desborda
SimplexStatus simplex_pivot(Table *table, int leaving_constraint, int entering_column);

// Con la tabla optima, llena variable_values, slack_values y objective_value
// del resultado (las no basicas valen 0, las basicas valen el lado derecho de su fila).
// Devuelve SIMPLEX_OPTIMAL si sale bien
SimplexStatus simplex_extract_solution(const Problem *problem, const Table *table, SimplexResult *result);

#endif
