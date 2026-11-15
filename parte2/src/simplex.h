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

// Z + originales + holguras o excesos + artificiales + lado derecho (cada restriccion aporta a lo sumo dos)
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
    COLUMN_SURPLUS,
    COLUMN_ARTIFICIAL,
    COLUMN_RIGHT_HAND_SIDE
} ColumnType;

// Estado del motor. SIMPLEX_RUNNING significa "todo bien, sigue" y es el unico
// valor que no termina la ejecucion
typedef enum {
    SIMPLEX_RUNNING = 0,
    SIMPLEX_OPTIMAL,
    SIMPLEX_UNBOUNDED,
    SIMPLEX_INFEASIBLE,
    SIMPLEX_ITERATION_LIMIT,
    SIMPLEX_ERROR_OVERFLOW,
    SIMPLEX_ERROR_INVALID_PROBLEM
} SimplexStatus;

// Tabla del Simplex en el formato de clase: columnas Z, originales, holguras y excesos, artificiales
// y lado derecho (en ese orden); la fila 0 es la fila objetivo y las demas son restricciones.
// Con la Gran M, cada valor de la fila objetivo es a + b*M: cells guarda a y m_cells guarda b
// (M es mayor que cualquier numero). Las demas filas nunca llevan M
typedef struct {
    int row_count;
    int column_count;
    Fraction cells[MAX_TABLE_ROWS][MAX_TABLE_COLUMNS];
    ColumnType column_types[MAX_TABLE_COLUMNS];
    // Parte de M de la fila objetivo, una por columna
    Fraction m_cells[MAX_TABLE_COLUMNS];
    // Dueno de cada columna: la variable original (indice desde 0) o la restriccion a la que
    // pertenece su holgura, exceso o artificial; -1 en Z y en el lado derecho. Sirve para
    // nombrar las columnas aunque se omitan las artificiales que salieron de la base
    int column_owner[MAX_TABLE_COLUMNS];
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
    // Variables basicas que valen 0 en esta tabla; mayor que 0 significa BFS degenerada
    int zero_basic_count;
} Step;

// Resultado completo: estado, historial y solucion (la solucion solo se llena si es optimo)
typedef struct {
    SimplexStatus status;
    // 1 si el problema necesito variables artificiales (restricciones >= o =): se uso la Gran M
    int has_artificials;
    // Tabla inicial tal como se arma, con M en las artificiales. Con artificiales, steps[0] es esta
    // misma tabla ya canonizada (columnas artificiales en cero en la fila objetivo)
    Table initial_table;
    int step_count;
    Step steps[MAX_STEPS];
    Fraction variable_values[MAX_VARIABLES];
    // Valor de la holgura o del exceso de cada restriccion (0 en las de igualdad)
    Fraction slack_values[MAX_CONSTRAINTS];
    Fraction objective_value;

    // 1 si alguna tabla del historial tiene una variable basica en 0 (problema degenerado)
    int is_degenerate;

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

// Signo de a + b*M en la fila objetivo: manda la parte de M y, si es 0, la constante
int simplex_objective_sign(const Table *table, int column);

// Compara dos columnas de la fila objetivo (-1, 0 o 1) con la misma regla
int simplex_objective_compare(const Table *table, int left_column, int right_column);

// Cuenta las variables basicas que valen 0 (lado derecho 0 en su fila)
int simplex_count_zero_basics(const Table *table);

// Canoniza la tabla: anula en la fila objetivo la M de cada columna artificial basica, sumando M
// veces la fila de esa artificial. Devuelve SIMPLEX_ERROR_OVERFLOW si alguna operacion desborda
SimplexStatus simplex_canonize(Table *table);

// Omite una columna de la tabla (las artificiales que salen de la base) y ajusta la base
void simplex_remove_column(Table *table, int column);

// Pivotea la tabla: divide la fila del pivote entre el pivote, anula la
// columna en las demas filas (incluida la objetivo) y actualiza la base.
// Devuelve SIMPLEX_ERROR_OVERFLOW si alguna operacion desborda
SimplexStatus simplex_pivot(Table *table, int leaving_constraint, int entering_column);

// Con la tabla optima, llena variable_values, slack_values y objective_value
// del resultado (las no basicas valen 0, las basicas valen el lado derecho de su fila).
// Devuelve SIMPLEX_OPTIMAL si sale bien
SimplexStatus simplex_extract_solution(const Problem *problem, const Table *table, SimplexResult *result);

#endif
