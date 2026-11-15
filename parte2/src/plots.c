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
        g_string_append_printf(output, "%g};\n", tick);
    }
    for (double tick = step_y; tick <= limit_y + EPSILON; tick += step_y) {
        g_string_append(output, "\\draw[textgray] (-0.05,");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ") -- (0.05,");
        latex_append_decimal(output, tick * scale);
        g_string_append(output, ") node[left=1.5pt,textgray]{\\tiny ");
        g_string_append_printf(output, "%g};\n", tick);
    }
}

// Indica si el vértice está sobre un lado de la caja (entonces no es un vértice real de la región)
static int is_on_box(const Vertex *vertex, const Fraction limits[]) {
    return fraction_compare(vertex->coordinates[0], limits[0]) == 0 ||
           fraction_compare(vertex->coordinates[1], limits[1]) == 0;
}

// Leyenda: un cuadrito de color y la restricción de cada recta, más lo que significan las marcas
static void append_legend(GString *output, const Problem *problem, const SimplexResult *result,
                          int has_region) {
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
        g_string_append(output, "\\tikz\\node[star,star point ratio=2.2,fill=tecred,inner sep=1.2pt]{};\\ Óptimo\\\\[1.5pt]\n"
                                "\\tikz\\draw[tecred,dashed,thick] (0,0) -- (2.4mm,0);\\ Recta de $Z$\n");
    }
    g_string_append(output, "\\end{minipage}\n");
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
    geometry_box_limits(problem, limits);
    int space_count = geometry_build_half_spaces(problem, limits, spaces);
    int vertex_count = geometry_find_vertices(spaces, space_count, 2, vertices, &had_overflow);
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
        g_string_append_printf(output, "\\draw[plotc%d,line width=%.1fpt] ", constraint % PLOT_COLOR_COUNT + 1,
                               width);
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
    append_legend(output, problem, result, polygon_count > 0);
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
