#include "plots.h"

#include <math.h>
#include <stdlib.h>

#include "geometry.h"
#include "latex_text.h"

// =============================================
// Constants
// ---------------------------------------------
// Cantidad de colores definidos en el preámbulo (plotc1 ... plotc8); las restricciones los reutilizan
#define PLOT_COLOR_COUNT 8

// Tamaño máximo del dibujo 2D en centímetros
#define PLOT_WIDTH_CM 8.0
#define PLOT_HEIGHT_CM 3.5

// Cuánto sobresalen los ejes de la caja, la separación de los números y el largo de la flecha del objetivo
#define AXIS_EXTRA_CM 0.5
#define OBJECTIVE_ARROW_CM 0.9

// Tolerancia para comparar coordenadas en doubles
#define EPSILON 1e-9

// Máximo de vértices con su coordenada escrita (más ensucian el gráfico)
#define MAX_LABELED_VERTICES 8

// Ticks que se buscan en cada eje
#define TARGET_TICK_COUNT 5

// Proyección isométrica: coseno y seno de 30 grados
#define ISOMETRIC_COS 0.8660254037844386
#define ISOMETRIC_SIN 0.5

// Cuánto más largo que la caja se dibuja cada eje
#define AXIS_REACH 1.12

// Tamaño máximo del dibujo 3D en centímetros y vértices que caben en una cara
#define PLOT_3D_WIDTH_CM 8.0
#define PLOT_3D_HEIGHT_CM 4.4
#define MAX_FACE_VERTICES 200

// Vértices reales con su coordenada escrita en el 3D
#define MAX_LABELED_VERTICES_3D 10

// =============================================
// Types
// ---------------------------------------------
typedef struct {
    double x;
    double y;
} Point;

// =============================================
// Helpers
// ---------------------------------------------

// Escribe un punto de TikZ ya escalado a centímetros
static void append_point(GString *output, Point point, double scale) {
    g_string_append(output, "(");
    latex_append_decimal(output, point.x * scale);
    g_string_append(output, ",");
    latex_append_decimal(output, point.y * scale);
    g_string_append(output, ")");
}

// Paso "bonito" (1, 2 o 5 por una potencia de 10) para que el eje tenga unas 5 marcas
static double nice_tick_step(double limit) {
    double raw_step = limit / TARGET_TICK_COUNT;
    double magnitude = pow(10.0, floor(log10(raw_step)));
    double normalized = raw_step / magnitude;

    if (normalized <= 1.0) {
        return magnitude;
    }
    if (normalized <= 2.0) {
        return 2.0 * magnitude;
    }
    if (normalized <= 5.0) {
        return 5.0 * magnitude;
    }
    return 10.0 * magnitude;
}

// Recorta la recta a*x + b*y = c a la caja [0, limit_x] x [0, limit_y]; devuelve 0 si no pasa por la caja
static int clip_line_to_box(double a, double b, double c, double limit_x, double limit_y, Point *start,
                            Point *end) {
    Point candidates[4];
    int count = 0;

    // Cortes con los cuatro lados de la caja
    if (fabs(b) > EPSILON) {
        double y_at_left = c / b;
        double y_at_right = (c - a * limit_x) / b;
        if (y_at_left >= -EPSILON && y_at_left <= limit_y + EPSILON) {
            candidates[count++] = (Point){0.0, y_at_left};
        }
        if (y_at_right >= -EPSILON && y_at_right <= limit_y + EPSILON) {
            candidates[count++] = (Point){limit_x, y_at_right};
        }
    }
    if (fabs(a) > EPSILON) {
        double x_at_bottom = c / a;
        double x_at_top = (c - b * limit_y) / a;
        if (x_at_bottom >= -EPSILON && x_at_bottom <= limit_x + EPSILON) {
            candidates[count++] = (Point){x_at_bottom, 0.0};
        }
        if (x_at_top >= -EPSILON && x_at_top <= limit_x + EPSILON) {
            candidates[count++] = (Point){x_at_top, limit_y};
        }
    }

    // Los dos cortes más separados forman el segmento visible
    double best_distance = 0.0;
    int found = 0;
    for (int first = 0; first < count; first++) {
        for (int second = first + 1; second < count; second++) {
            double distance = hypot(candidates[first].x - candidates[second].x,
                                    candidates[first].y - candidates[second].y);
            if (distance > best_distance + EPSILON) {
                best_distance = distance;
                *start = candidates[first];
                *end = candidates[second];
                found = 1;
            }
        }
    }
    return found;
}

// Ordena los puntos alrededor de su centro (de menor a mayor ángulo) para recorrer el polígono
static void sort_around_center(Point points[], int count) {
    double center_x = 0.0;
    double center_y = 0.0;
    double angles[GEOMETRY_MAX_VERTICES];

    for (int index = 0; index < count; index++) {
        center_x += points[index].x;
        center_y += points[index].y;
    }
    center_x /= count;
    center_y /= count;
    for (int index = 0; index < count; index++) {
        angles[index] = atan2(points[index].y - center_y, points[index].x - center_x);
    }

    // Ordenamiento por inserción (hay pocos puntos)
    for (int index = 1; index < count; index++) {
        Point point = points[index];
        double angle = angles[index];
        int position = index - 1;
        while (position >= 0 && angles[position] > angle) {
            points[position + 1] = points[position];
            angles[position + 1] = angles[position];
            position--;
        }
        points[position + 1] = point;
        angles[position + 1] = angle;
    }
}

// Escribe un punto exacto como (a, b) en modo matemático
static void append_exact_point(GString *output, const Fraction values[]) {
    g_string_append(output, "$(");
    latex_append_fraction(output, values[0]);
    g_string_append(output, ",\\,");
    latex_append_fraction(output, values[1]);
    g_string_append(output, ")$");
}

// Busca los vértices de la región dentro de la caja del gráfico. Si algún vértice cae en el borde de la caja
// la región no está acotada: se agranda la caja una vez para que se vea más del cuerpo. Devuelve la cantidad
// de vértices y deja en space_count los semiespacios usados
static int find_vertices_in_box(const Problem *problem, Fraction limits[], HalfSpace spaces[], int *space_count,
                                Vertex vertices[], int *had_overflow) {
    int dimension = problem->variable_count;

    geometry_box_limits(problem, limits);
    *space_count = geometry_build_half_spaces(problem, limits, spaces);
    int vertex_count = geometry_find_vertices(spaces, *space_count, dimension, vertices, had_overflow);

    // Algún vértice sobre la caja significa que la región sigue más allá del dibujo
    int touches_box = 0;
    for (int index = 0; index < vertex_count; index++) {
        for (int axis = 0; axis < dimension; axis++) {
            if (fraction_compare(vertices[index].coordinates[axis], limits[axis]) == 0) {
                touches_box = 1;
            }
        }
    }
    if (touches_box) {
        for (int axis = 0; axis < dimension; axis++) {
            Fraction doubled;
            if (fraction_multiply(limits[axis], fraction_from_int(2), &doubled) == FRACTION_OK) {
                limits[axis] = doubled;
            }
        }
        *space_count = geometry_build_half_spaces(problem, limits, spaces);
        vertex_count = geometry_find_vertices(spaces, *space_count, dimension, vertices, had_overflow);
    }
    return vertex_count;
}

// Dibuja los ejes con sus marcas y el nombre de cada variable
static void append_axes(GString *output, const Problem *problem, double limit_x, double limit_y, double scale) {
    double step_x = nice_tick_step(limit_x);
    double step_y = nice_tick_step(limit_y);

    g_string_append(output, "\\draw[->,thick,textgray] (0,0) -- (");
    latex_append_decimal(output, limit_x * scale + AXIS_EXTRA_CM);
    g_string_append_printf(output, ",0) node[right,black]{\\small %s};\n", problem->variable_names[0]);
    g_string_append(output, "\\draw[->,thick,textgray] (0,0) -- (0,");
    latex_append_decimal(output, limit_y * scale + AXIS_EXTRA_CM);
    g_string_append_printf(output, ") node[above,black]{\\small %s};\n", problem->variable_names[1]);

    // Marcas con su número
    for (double tick = step_x; tick <= limit_x + EPSILON; tick += step_x) {
        g_string_append(output, "\\draw[textgray] (");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ",-0.05) -- (");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ",0.05) node[below=1.5pt,textgray]{\\tiny ");
        latex_append_formatted(output, "%g", tick);
        g_string_append(output, "};\n");
    }
    for (double tick = step_y; tick <= limit_y + EPSILON; tick += step_y) {
        g_string_append(output, "\\draw[textgray] (-0.05,");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ") -- (0.05,");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ") node[left=1.5pt,textgray]{\\tiny ");
        latex_append_formatted(output, "%g", tick);
        g_string_append(output, "};\n");
    }
}

// Indica si el vértice está sobre un lado de la caja (entonces no es un vértice real de la región)
static int is_on_box(const Vertex *vertex, const Fraction limits[]) {
    return fraction_compare(vertex->coordinates[0], limits[0]) == 0 ||
           fraction_compare(vertex->coordinates[1], limits[1]) == 0;
}

// Leyenda: un cuadrito de color y la restricción de cada recta, más lo que significan las marcas
static void append_legend(GString *output, const Problem *problem, const SimplexResult *result,
                          int has_region, int show_objective_line) {
    g_string_append(output, "\\begin{minipage}[c]{4.6cm}\\tiny\\raggedright\n");
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        g_string_append_printf(output, "\\tikz\\fill[plotc%d] (0,0) rectangle (2.4mm,1.6mm);\\ R%d: ",
                               constraint % PLOT_COLOR_COUNT + 1, constraint + 1);
        latex_append_constraint(output, problem, constraint);
        g_string_append(output, "\\\\[1.5pt]\n");
    }
    if (has_region) {
        g_string_append(output, "\\tikz\\fill[tecblue,fill opacity=0.25] (0,0) rectangle (2.4mm,1.6mm);\\ "
                                "Región factible\\\\[1.5pt]\n");
    }
    if (result->status == SIMPLEX_OPTIMAL) {
        g_string_append(output, "\\tikz\\node[star,star point ratio=2.2,fill=tecred,inner sep=1.2pt]{};\\ Óptimo\\\\[1.5pt]\n");
    }
    if (result->status == SIMPLEX_OPTIMAL && show_objective_line) {
        g_string_append(output, "\\tikz\\draw[tecred,dashed,thick] (0,0) -- (2.4mm,0);\\ Recta de $Z$\n");
    }
    g_string_append(output, "\\end{minipage}\n");
}

// =============================================
// 3D helpers
// ---------------------------------------------

// Punto del espacio con una coordenada por variable
typedef struct {
    double x[3];
} Point3;

// Cara del poliedro: el semiespacio que la define y los vértices que están en su plano, ya en orden
typedef struct {
    int space;
    int vertex_count;
    int vertices[MAX_FACE_VERTICES];
    double depth;
} Face;

// Proyección isométrica: x1 baja a la izquierda, x2 baja a la derecha y x3 sube
static Point project_point(const double coordinates[]) {
    Point point;
    point.x = (coordinates[1] - coordinates[0]) * ISOMETRIC_COS;
    point.y = coordinates[2] - (coordinates[0] + coordinates[1]) * ISOMETRIC_SIN;
    return point;
}

// Convierte un vértice exacto a coordenadas decimales
static Point3 vertex_to_point(const Vertex *vertex) {
    Point3 point;
    for (int axis = 0; axis < 3; axis++) {
        point.x[axis] = geometry_to_double(vertex->coordinates[axis]);
    }
    return point;
}

// Ordena los vértices de una cara alrededor de su centro, mirando el plano de frente
static void sort_face_vertices(Face *face, const HalfSpace *space, const Point3 points[]) {
    double normal[3];
    double helper[3] = {0.0, 0.0, 0.0};
    double first_axis[3];
    double second_axis[3];
    double center[3] = {0.0, 0.0, 0.0};
    double angles[MAX_FACE_VERTICES];
    int smallest = 0;

    // Base del plano: u = normal x (eje más distinto de la normal), v = normal x u
    for (int axis = 0; axis < 3; axis++) {
        normal[axis] = geometry_to_double(space->coefficients[axis]);
    }
    for (int axis = 1; axis < 3; axis++) {
        if (fabs(normal[axis]) < fabs(normal[smallest])) {
            smallest = axis;
        }
    }
    helper[smallest] = 1.0;
    first_axis[0] = normal[1] * helper[2] - normal[2] * helper[1];
    first_axis[1] = normal[2] * helper[0] - normal[0] * helper[2];
    first_axis[2] = normal[0] * helper[1] - normal[1] * helper[0];
    second_axis[0] = normal[1] * first_axis[2] - normal[2] * first_axis[1];
    second_axis[1] = normal[2] * first_axis[0] - normal[0] * first_axis[2];
    second_axis[2] = normal[0] * first_axis[1] - normal[1] * first_axis[0];

    for (int index = 0; index < face->vertex_count; index++) {
        for (int axis = 0; axis < 3; axis++) {
            center[axis] += points[face->vertices[index]].x[axis] / face->vertex_count;
        }
    }
    for (int index = 0; index < face->vertex_count; index++) {
        double along_first = 0.0;
        double along_second = 0.0;
        for (int axis = 0; axis < 3; axis++) {
            double offset = points[face->vertices[index]].x[axis] - center[axis];
            along_first += offset * first_axis[axis];
            along_second += offset * second_axis[axis];
        }
        angles[index] = atan2(along_second, along_first);
    }

    // Ordenamiento por inserción
    for (int index = 1; index < face->vertex_count; index++) {
        int vertex = face->vertices[index];
        double angle = angles[index];
        int position = index - 1;
        while (position >= 0 && angles[position] > angle) {
            face->vertices[position + 1] = face->vertices[position];
            angles[position + 1] = angles[position];
            position--;
        }
        face->vertices[position + 1] = vertex;
        angles[position + 1] = angle;
    }

    // Profundidad de la cara: el promedio de x1 + x2 + x3 (más grande = más cerca de quien mira)
    face->depth = center[0] + center[1] + center[2];
}

// Indica si ya hay una cara con los mismos vértices (dos restricciones pueden definir el mismo plano)
static int face_is_listed(const Face faces[], int face_count, const Face *candidate) {
    for (int index = 0; index < face_count; index++) {
        if (faces[index].vertex_count != candidate->vertex_count) {
            continue;
        }
        int shared = 0;
        for (int first = 0; first < candidate->vertex_count; first++) {
            for (int second = 0; second < candidate->vertex_count; second++) {
                if (faces[index].vertices[second] == candidate->vertices[first]) {
                    shared++;
                }
            }
        }
        if (shared == candidate->vertex_count) {
            return 1;
        }
    }
    return 0;
}

// Indica si el vértice está sobre un lado de la caja (entonces no es un vértice real de la región)
static int is_on_box_3d(const Vertex *vertex, const Fraction limits[]) {
    for (int axis = 0; axis < 3; axis++) {
        if (fraction_compare(vertex->coordinates[axis], limits[axis]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Escribe el punto de TikZ de unas coordenadas 3D proyectadas y escaladas
static void append_projected(GString *output, const double coordinates[], double scale) {
    Point point = project_point(coordinates);
    append_point(output, point, scale);
}

// Valores de las variables originales de una tabla (las no básicas valen 0)
static void table_point(const Table *table, int variable_count, double coordinates[]) {
    int right_hand_side_column = table->column_count - 1;

    for (int axis = 0; axis < 3; axis++) {
        coordinates[axis] = 0.0;
    }
    for (int constraint = 0; constraint < table->row_count - 1; constraint++) {
        int column = table->basis[constraint];
        if (table->column_types[column] == COLUMN_ORIGINAL && table->column_owner[column] < variable_count) {
            coordinates[table->column_owner[column]] =
                geometry_to_double(table->cells[constraint + 1][right_hand_side_column]);
        }
    }
}

// =============================================
// Public functions
// ---------------------------------------------

// Gráfico de la región factible de un problema de 2 variables
void plots_append_region_2d(GString *output, const Problem *problem, const SimplexResult *result) {
    HalfSpace spaces[GEOMETRY_MAX_HALF_SPACES];
    Fraction limits[GEOMETRY_MAX_DIMENSION];
    Point polygon[GEOMETRY_MAX_VERTICES];
    int had_overflow = 0;
    int polygon_count = 0;
    int real_vertex_count = 0;

    if (problem->variable_count != 2) {
        return;
    }
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);
    if (vertices == NULL) {
        return;
    }

    // Vértices de la región dentro de una caja que la encierra (aunque no esté acotada)
    int space_count = 0;
    int vertex_count = find_vertices_in_box(problem, limits, spaces, &space_count, vertices, &had_overflow);
    double limit_x = geometry_to_double(limits[0]);
    double limit_y = geometry_to_double(limits[1]);
    double scale = PLOT_WIDTH_CM / limit_x;
    if (PLOT_HEIGHT_CM / limit_y < scale) {
        scale = PLOT_HEIGHT_CM / limit_y;
    }

    // Polígono de la región y cantidad de vértices reales (los de la caja no cuentan)
    for (int index = 0; index < vertex_count; index++) {
        polygon[polygon_count].x = geometry_to_double(vertices[index].coordinates[0]);
        polygon[polygon_count].y = geometry_to_double(vertices[index].coordinates[1]);
        polygon_count++;
        if (!is_on_box(&vertices[index], limits)) {
            real_vertex_count++;
        }
    }
    int touches_box = (real_vertex_count < vertex_count);
    if (polygon_count >= 3) {
        sort_around_center(polygon, polygon_count);
    }

    g_string_append(output, "\\slide{Gráfico de la región factible}\n"
                            "\\begin{lrbox}{\\fitbox}\\begin{tabular}{@{}c@{\\hspace{4mm}}c@{}}\n"
                            "\\begin{tikzpicture}\n");

    // Región factible rellena (con una igualdad queda solo un segmento, que se dibuja aparte)
    if (polygon_count >= 3) {
        g_string_append(output, "\\fill[tecblue,fill opacity=0.25] ");
        for (int index = 0; index < polygon_count; index++) {
            append_point(output, polygon[index], scale);
            g_string_append(output, " -- ");
        }
        g_string_append(output, "cycle;\n");
    } else if (polygon_count == 2) {
        g_string_append(output, "\\draw[tecblue,line width=2.4pt,opacity=0.45] ");
        append_point(output, polygon[0], scale);
        g_string_append(output, " -- ");
        append_point(output, polygon[1], scale);
        g_string_append(output, ";\n");
    }

    append_axes(output, problem, limit_x, limit_y, scale);

    // Una recta por restricción, recortada a la caja y con su etiqueta en un extremo
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        Point start;
        Point end;
        double a = geometry_to_double(problem->coefficients[constraint][0]);
        double b = geometry_to_double(problem->coefficients[constraint][1]);
        double c = geometry_to_double(problem->right_hand_sides[constraint]);
        if (!clip_line_to_box(a, b, c, limit_x, limit_y, &start, &end)) {
            continue;
        }
        double width = 0.9;
        if (problem->constraint_types[constraint] == CONSTRAINT_EQUAL) {
            width = 1.6;
        }
        g_string_append_printf(output, "\\draw[plotc%d,line width=", constraint % PLOT_COLOR_COUNT + 1);
        latex_append_formatted(output, "%.1f", width);
        g_string_append(output, "pt] ");
        append_point(output, start, scale);
        g_string_append(output, " -- ");
        append_point(output, end, scale);
        g_string_append_printf(output, " node[plotc%d,font=\\tiny\\bfseries,fill=white,fill opacity=0.8,"
                                       "text opacity=1,inner sep=1pt]{R%d};\n",
                               constraint % PLOT_COLOR_COUNT + 1, constraint + 1);
    }

    // Vértices reales con su coordenada exacta (si no son demasiados)
    for (int index = 0; index < vertex_count; index++) {
        if (is_on_box(&vertices[index], limits)) {
            continue;
        }
        Point point = {geometry_to_double(vertices[index].coordinates[0]),
                       geometry_to_double(vertices[index].coordinates[1])};
        g_string_append(output, "\\fill[black] ");
        append_point(output, point, scale);
        g_string_append(output, " circle (1.1pt);\n");
        if (real_vertex_count <= MAX_LABELED_VERTICES) {
            g_string_append(output, "\\node[font=\\tiny,above right,inner sep=1pt] at ");
            append_point(output, point, scale);
            g_string_append(output, "{");
            append_exact_point(output, vertices[index].coordinates);
            g_string_append(output, "};\n");
        }
    }

    // Óptimo: recta de la función objetivo, flecha hacia donde mejora Z y estrella en cada punto óptimo
    if (result->status == SIMPLEX_OPTIMAL) {
        Point optimum = {geometry_to_double(result->variable_values[0]),
                         geometry_to_double(result->variable_values[1])};
        double cost_x = geometry_to_double(problem->objective[0]);
        double cost_y = geometry_to_double(problem->objective[1]);
        double objective_value = geometry_to_double(result->objective_value);
        Point start;
        Point end;

        if (clip_line_to_box(cost_x, cost_y, objective_value, limit_x, limit_y, &start, &end)) {
            g_string_append(output, "\\draw[tecred,dashed,thick] ");
            append_point(output, start, scale);
            g_string_append(output, " -- ");
            append_point(output, end, scale);
            g_string_append(output, ";\n");
        }

        // La flecha apunta en la dirección que mejora Z: el gradiente al maximizar y su opuesto al minimizar
        double direction_x = cost_x;
        double direction_y = cost_y;
        if (problem->objective_type == OBJECTIVE_MINIMIZE) {
            direction_x = -direction_x;
            direction_y = -direction_y;
        }
        double length = hypot(direction_x, direction_y);
        if (length > EPSILON) {
            g_string_append(output, "\\draw[-{Stealth[length=1.8mm]},tecred,very thick] ");
            append_point(output, optimum, scale);
            g_string_append(output, " -- ++(");
            latex_append_decimal(output, direction_x / length * OBJECTIVE_ARROW_CM);
            g_string_append(output, ",");
            latex_append_decimal(output, direction_y / length * OBJECTIVE_ARROW_CM);
            g_string_append(output, ");\n");
        }

        // Soluciones múltiples: el segmento óptimo o el rayo de soluciones
        if (result->multiple_kind == MULTIPLE_SEGMENT) {
            Point second = {geometry_to_double(result->alternative_variable_values[0]),
                            geometry_to_double(result->alternative_variable_values[1])};
            g_string_append(output, "\\draw[tecred,line width=2.2pt] ");
            append_point(output, optimum, scale);
            g_string_append(output, " -- ");
            append_point(output, second, scale);
            g_string_append(output, ";\n\\node[star,star point ratio=2.2,fill=tecred,draw=white,line width=0.3pt,inner sep=1.6pt] at ");
            append_point(output, second, scale);
            g_string_append(output, "{};\n");
        } else if (result->multiple_kind == MULTIPLE_RAY) {
            double ray_x = geometry_to_double(result->ray_direction[0]);
            double ray_y = geometry_to_double(result->ray_direction[1]);
            double reach = limit_x + limit_y;
            if (ray_x > EPSILON && (limit_x - optimum.x) / ray_x < reach) {
                reach = (limit_x - optimum.x) / ray_x;
            }
            if (ray_y > EPSILON && (limit_y - optimum.y) / ray_y < reach) {
                reach = (limit_y - optimum.y) / ray_y;
            }
            Point ray_end = {optimum.x + ray_x * reach, optimum.y + ray_y * reach};
            g_string_append(output, "\\draw[-{Stealth[length=1.8mm]},tecred,line width=2.2pt] ");
            append_point(output, optimum, scale);
            g_string_append(output, " -- ");
            append_point(output, ray_end, scale);
            g_string_append(output, ";\n");
        }
        g_string_append(output, "\\node[star,star point ratio=2.2,fill=tecred,draw=white,line width=0.3pt,inner sep=1.8pt] at ");
        append_point(output, optimum, scale);
        g_string_append(output, "{};\n");
    }

    // No acotado: una flecha en la dirección en que Z mejora sin límite, desde el centro de la región
    if (result->status == SIMPLEX_UNBOUNDED && polygon_count > 0) {
        Point center = {0.0, 0.0};
        double direction_x = geometry_to_double(problem->objective[0]);
        double direction_y = geometry_to_double(problem->objective[1]);
        if (problem->objective_type == OBJECTIVE_MINIMIZE) {
            direction_x = -direction_x;
            direction_y = -direction_y;
        }
        for (int index = 0; index < polygon_count; index++) {
            center.x += polygon[index].x / polygon_count;
            center.y += polygon[index].y / polygon_count;
        }
        double length = hypot(direction_x, direction_y);
        if (length > EPSILON) {
            g_string_append(output, "\\draw[-{Stealth[length=1.8mm]},tecred,very thick] ");
            append_point(output, center, scale);
            g_string_append(output, " -- ++(");
            latex_append_decimal(output, direction_x / length * OBJECTIVE_ARROW_CM);
            g_string_append(output, ",");
            latex_append_decimal(output, direction_y / length * OBJECTIVE_ARROW_CM);
            g_string_append(output, ");\n");
        }
    }
    g_string_append(output, "\\end{tikzpicture}\n&\n");
    append_legend(output, problem, result, polygon_count > 0, 1);
    g_string_append(output, "\\end{tabular}\\end{lrbox}\n\\showfitted{0.68\\textheight}\n");

    // Nota que explica lo que muestra el gráfico
    g_string_append(output, "{\\footnotesize\\color{textgray}");
    if (vertex_count == 0) {
        g_string_append(output, "{\\color{tecred}\\bfseries La región factible está vacía:} no hay ningún punto que "
                                "cumpla todas las restricciones a la vez, y por eso el problema no es factible.");
    } else if (result->status == SIMPLEX_OPTIMAL) {
        g_string_append(output, "Óptimo en ");
        append_exact_point(output, result->variable_values);
        g_string_append(output, " con $Z^{*} = ");
        latex_append_fraction(output, result->objective_value);
        g_string_append(output, "$: es el último vértice que toca la recta de $Z$ al moverla en la dirección de "
                                "la flecha.");
        if (result->multiple_kind == MULTIPLE_SEGMENT) {
            g_string_append(output, " La recta de $Z$ coincide con un lado de la región: todo el segmento "
                                    "marcado es óptimo.");
        } else if (result->multiple_kind == MULTIPLE_RAY) {
            g_string_append(output, " Todos los puntos del rayo marcado son óptimos.");
        }
    } else if (result->status == SIMPLEX_UNBOUNDED) {
        g_string_append(output, "{\\color{tecred}\\bfseries La región continúa sin límite} (el dibujo se corta "
                                "en el borde del gráfico) y $Z$ mejora sin tope en esa dirección.");
    }
    if (touches_box && result->status == SIMPLEX_OPTIMAL) {
        g_string_append(output, " La región no está acotada: el dibujo se corta en el borde del gráfico.");
    }
    if (had_overflow) {
        g_string_append(output, " Algunos vértices no se pudieron calcular de forma exacta.");
    }
    g_string_append(output, "\\par}\n");
    free(vertices);
}

// Gráfico del cuerpo Símplex de un problema de 3 variables
void plots_append_body_3d(GString *output, const Problem *problem, const SimplexResult *result) {
    HalfSpace spaces[GEOMETRY_MAX_HALF_SPACES];
    Fraction limits[GEOMETRY_MAX_DIMENSION];
    Face faces[GEOMETRY_MAX_HALF_SPACES];
    int had_overflow = 0;
    int face_count = 0;
    int real_vertex_count = 0;

    if (problem->variable_count != 3) {
        return;
    }
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);
    Point3 *points = malloc(sizeof(Point3) * GEOMETRY_MAX_VERTICES);
    if (vertices == NULL || points == NULL) {
        free(vertices);
        free(points);
        return;
    }

    // Vértices de la región dentro de una caja que la encierra (aunque no esté acotada)
    int space_count = 0;
    int vertex_count = find_vertices_in_box(problem, limits, spaces, &space_count, vertices, &had_overflow);
    double limit[3];
    for (int axis = 0; axis < 3; axis++) {
        limit[axis] = geometry_to_double(limits[axis]);
    }
    for (int index = 0; index < vertex_count; index++) {
        points[index] = vertex_to_point(&vertices[index]);
        if (!is_on_box_3d(&vertices[index], limits)) {
            real_vertex_count++;
        }
    }
    int touches_box = (real_vertex_count < vertex_count);

    // Una cara por plano que tenga al menos 3 vértices
    for (int space = 0; space < space_count; space++) {
        Face face;
        face.space = space;
        face.vertex_count = 0;
        for (int index = 0; index < vertex_count && face.vertex_count < MAX_FACE_VERTICES; index++) {
            if (geometry_vertex_is_on_plane(&spaces[space], &vertices[index], 3)) {
                face.vertices[face.vertex_count] = index;
                face.vertex_count++;
            }
        }
        if (face.vertex_count < 3) {
            continue;
        }
        sort_face_vertices(&face, &spaces[space], points);
        if (!face_is_listed(faces, face_count, &face)) {
            faces[face_count] = face;
            face_count++;
        }
    }

    // Se pintan primero las caras más lejanas (menor profundidad)
    for (int index = 1; index < face_count; index++) {
        Face face = faces[index];
        int position = index - 1;
        while (position >= 0 && faces[position].depth > face.depth) {
            faces[position + 1] = faces[position];
            position--;
        }
        faces[position + 1] = face;
    }

    // Escala: el dibujo completo de la caja debe caber en el espacio disponible
    double drawing_width = (limit[0] + limit[1]) * ISOMETRIC_COS;
    double drawing_height = limit[2] + (limit[0] + limit[1]) * ISOMETRIC_SIN;
    double scale = PLOT_3D_WIDTH_CM / drawing_width;
    if (PLOT_3D_HEIGHT_CM / drawing_height < scale) {
        scale = PLOT_3D_HEIGHT_CM / drawing_height;
    }

    g_string_append(output, "\\slide{Cuerpo Símplex tridimensional}\n"
                            "\\begin{lrbox}{\\fitbox}\\begin{tabular}{@{}c@{\\hspace{4mm}}c@{}}\n"
                            "\\begin{tikzpicture}\n");

    // Ejes desde el origen, con una marca y su número cada cierto paso
    for (int axis = 0; axis < 3; axis++) {
        double end[3] = {0.0, 0.0, 0.0};
        double step = nice_tick_step(limit[axis]);
        const char *anchor = "left";
        const char *direction = "right";
        if (axis == 0) {
            anchor = "below left";
            direction = "below left";
        } else if (axis == 1) {
            anchor = "below right";
            direction = "below right";
        } else {
            direction = "above";
        }
        end[axis] = limit[axis] * AXIS_REACH;
        g_string_append(output, "\\draw[->,thick,textgray] ");
        append_projected(output, (double[]){0.0, 0.0, 0.0}, scale);
        g_string_append(output, " -- ");
        append_projected(output, end, scale);
        g_string_append_printf(output, " node[%s,black]{\\small %s};\n", direction, problem->variable_names[axis]);
        for (double tick = step; tick <= limit[axis] + EPSILON; tick += step) {
            double mark[3] = {0.0, 0.0, 0.0};
            mark[axis] = tick;
            g_string_append_printf(output, "\\node[font=\\tiny,textgray,%s,inner sep=1pt] at ", anchor);
            append_projected(output, mark, scale);
            g_string_append(output, "{");
            latex_append_formatted(output, "%g", tick);
            g_string_append(output, "};\n");
        }
    }

    // Caras del cuerpo: color de la restricción que las define y los planos de la caja casi transparentes
    for (int index = 0; index < face_count; index++) {
        const Face *face = &faces[index];
        int constraint = face->space;
        if (constraint < problem->constraint_count) {
            g_string_append_printf(output, "\\filldraw[plotc%d,fill opacity=0.45,draw=black!55,line width=0.35pt] ",
                                   constraint % PLOT_COLOR_COUNT + 1);
        } else if (touches_box && face->space >= space_count - 3) {
            g_string_append(output, "\\filldraw[textgray,fill opacity=0.08,draw=black!30,dashed,line width=0.3pt] ");
        } else {
            g_string_append(output, "\\filldraw[textgray,fill opacity=0.18,draw=black!55,line width=0.35pt] ");
        }
        for (int corner = 0; corner < face->vertex_count; corner++) {
            append_projected(output, points[face->vertices[corner]].x, scale);
            g_string_append(output, " -- ");
        }
        g_string_append(output, "cycle;\n");
    }

    // Una región con menos de 3 vértices en un plano es un segmento o un punto: se dibuja con sus vértices
    if (face_count == 0 && vertex_count >= 2) {
        g_string_append(output, "\\draw[tecblue,line width=2.2pt] ");
        append_projected(output, points[0].x, scale);
        g_string_append(output, " -- ");
        append_projected(output, points[1].x, scale);
        g_string_append(output, ";\n");
    }

    // Camino del Símplex: pasa de vértice en vértice (solo sin artificiales, donde todos son vértices reales)
    if (!result->has_artificials && result->step_count > 1) {
        double previous[3];
        table_point(&result->steps[0].table, 3, previous);
        for (int step = 1; step < result->step_count; step++) {
            double current[3];
            table_point(&result->steps[step].table, 3, current);
            if (fabs(current[0] - previous[0]) + fabs(current[1] - previous[1]) + fabs(current[2] - previous[2]) >
                EPSILON) {
                g_string_append(output, "\\draw[-{Stealth[length=1.6mm]},tecred,dashed,line width=1.1pt] ");
                append_projected(output, previous, scale);
                g_string_append(output, " -- ");
                append_projected(output, current, scale);
                g_string_append(output, ";\n");
            }
            for (int axis = 0; axis < 3; axis++) {
                previous[axis] = current[axis];
            }
        }
    }

    // Vértices reales con su coordenada exacta (si no son demasiados)
    for (int index = 0; index < vertex_count; index++) {
        if (is_on_box_3d(&vertices[index], limits)) {
            continue;
        }
        g_string_append(output, "\\fill[black] ");
        append_projected(output, points[index].x, scale);
        g_string_append(output, " circle (1pt);\n");
        if (real_vertex_count <= MAX_LABELED_VERTICES_3D) {
            g_string_append(output, "\\node[font=\\tiny,above right,inner sep=0.5pt] at ");
            append_projected(output, points[index].x, scale);
            g_string_append(output, "{$(");
            for (int axis = 0; axis < 3; axis++) {
                if (axis > 0) {
                    g_string_append(output, ",");
                }
                latex_append_fraction(output, vertices[index].coordinates[axis]);
            }
            g_string_append(output, ")$};\n");
        }
    }

    // Óptimo: estrella, y el segmento o rayo cuando hay soluciones múltiples
    if (result->status == SIMPLEX_OPTIMAL) {
        double optimum[3];
        for (int axis = 0; axis < 3; axis++) {
            optimum[axis] = geometry_to_double(result->variable_values[axis]);
        }
        if (result->multiple_kind == MULTIPLE_SEGMENT) {
            double second[3];
            for (int axis = 0; axis < 3; axis++) {
                second[axis] = geometry_to_double(result->alternative_variable_values[axis]);
            }
            g_string_append(output, "\\draw[tecred,line width=2.2pt] ");
            append_projected(output, optimum, scale);
            g_string_append(output, " -- ");
            append_projected(output, second, scale);
            g_string_append(output, ";\n\\node[star,star point ratio=2.2,fill=tecred,draw=white,line width=0.3pt,"
                                    "inner sep=1.6pt] at ");
            append_projected(output, second, scale);
            g_string_append(output, "{};\n");
        } else if (result->multiple_kind == MULTIPLE_RAY) {
            double reach = limit[0] + limit[1] + limit[2];
            double ray_end[3];
            for (int axis = 0; axis < 3; axis++) {
                double direction = geometry_to_double(result->ray_direction[axis]);
                if (direction > EPSILON && (limit[axis] - optimum[axis]) / direction < reach) {
                    reach = (limit[axis] - optimum[axis]) / direction;
                }
            }
            for (int axis = 0; axis < 3; axis++) {
                ray_end[axis] = optimum[axis] + geometry_to_double(result->ray_direction[axis]) * reach;
            }
            g_string_append(output, "\\draw[-{Stealth[length=1.8mm]},tecred,line width=2.2pt] ");
            append_projected(output, optimum, scale);
            g_string_append(output, " -- ");
            append_projected(output, ray_end, scale);
            g_string_append(output, ";\n");
        }
        g_string_append(output, "\\node[star,star point ratio=2.2,fill=tecred,draw=white,line width=0.3pt,"
                                "inner sep=1.8pt] at ");
        append_projected(output, optimum, scale);
        g_string_append(output, "{};\n");
    }
    g_string_append(output, "\\end{tikzpicture}\n&\n");
    append_legend(output, problem, result, face_count > 0, 0);
    g_string_append(output, "\\end{tabular}\\end{lrbox}\n\\showfitted{0.68\\textheight}\n");

    // Nota que explica lo que muestra el gráfico
    g_string_append(output, "{\\footnotesize\\color{textgray}");
    if (vertex_count == 0) {
        g_string_append(output, "{\\color{tecred}\\bfseries La región factible está vacía:} no hay ningún punto que "
                                "cumpla todas las restricciones a la vez, y por eso el problema no es factible.");
    } else if (result->status == SIMPLEX_OPTIMAL) {
        g_string_append(output, "Óptimo en $(");
        for (int axis = 0; axis < 3; axis++) {
            if (axis > 0) {
                g_string_append(output, ",\\,");
            }
            latex_append_fraction(output, result->variable_values[axis]);
        }
        g_string_append(output, ")$ con $Z^{*} = ");
        latex_append_fraction(output, result->objective_value);
        g_string_append(output, "$; cada vértice es una solución básica factible.");
        if (!result->has_artificials && result->step_count > 1) {
            g_string_append(output, " Las flechas punteadas son el camino del Símplex.");
        }
        if (result->multiple_kind == MULTIPLE_SEGMENT) {
            g_string_append(output, " Todo el segmento marcado es óptimo.");
        } else if (result->multiple_kind == MULTIPLE_RAY) {
            g_string_append(output, " Todo el rayo marcado es óptimo.");
        }
        if (touches_box) {
            g_string_append(output, " La región no está acotada: el dibujo se corta en el borde.");
        }
    } else if (result->status == SIMPLEX_UNBOUNDED) {
        g_string_append(output, "{\\color{tecred}\\bfseries La región continúa sin límite} (el dibujo se corta "
                                "en el borde de la caja) y $Z$ mejora sin tope en esa dirección.");
    }
    if (had_overflow) {
        g_string_append(output, " Algunos vértices no se pudieron calcular de forma exacta.");
    }
    g_string_append(output, "\\par}\n");
    free(vertices);
    free(points);
}
