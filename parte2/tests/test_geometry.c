#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fraction.h"
#include "geometry.h"
#include "problem.h"
#include "simplex.h"

// =============================================
// Constants
// ---------------------------------------------
#define RANDOM_PROBLEM_COUNT 400
#define RANDOM_SEED 2028

// =============================================
// Test helpers
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;

// Cuenta una comprobacion y avisa si falla
static void check(int condition, const char *description) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s\n", description);
    }
}

// Arma un problema de la dimension pedida con el objetivo dado
static void start_problem(Problem *problem, int variable_count, int constraint_count, const int objective[]) {
    problem_init(problem);
    problem_set_sizes(problem, variable_count, constraint_count);
    for (int variable = 0; variable < variable_count; variable++) {
        problem->objective[variable] = fraction_from_int(objective[variable]);
    }
}

// Escribe una restriccion
static void set_constraint(Problem *problem, int constraint, const int coefficients[], ConstraintType type,
                           int right_hand_side) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        problem->coefficients[constraint][variable] = fraction_from_int(coefficients[variable]);
    }
    problem->constraint_types[constraint] = type;
    problem->right_hand_sides[constraint] = fraction_from_int(right_hand_side);
}

// Busca los vertices del problema sin caja y devuelve cuantos hay
static int count_vertices(const Problem *problem, Vertex *vertices, const Fraction box_limits[]) {
    HalfSpace spaces[GEOMETRY_MAX_HALF_SPACES];
    int had_overflow = 0;
    int space_count = geometry_build_half_spaces(problem, box_limits, spaces);
    return geometry_find_vertices(spaces, space_count, problem->variable_count, vertices, &had_overflow);
}

// Indica si la lista tiene un vertice con esas coordenadas (como texto de fraccion)
static int has_vertex(const Vertex vertices[], int vertex_count, int dimension, const char *x, const char *y,
                      const char *z) {
    const char *expected[GEOMETRY_MAX_DIMENSION] = {x, y, z};
    char text[FRACTION_STRING_SIZE];

    for (int index = 0; index < vertex_count; index++) {
        int is_match = 1;
        for (int axis = 0; axis < dimension; axis++) {
            fraction_to_string(vertices[index].coordinates[axis], text, FRACTION_STRING_SIZE);
            if (strcmp(text, expected[axis]) != 0) {
                is_match = 0;
            }
        }
        if (is_match) {
            return 1;
        }
    }
    return 0;
}

// =============================================
// Known regions
// ---------------------------------------------

// Wyndor Glass: 5 vertices
static void test_wyndor_vertices(void) {
    Problem problem;
    const int objective[] = {3, 5};
    const int first[] = {1, 0};
    const int second[] = {0, 2};
    const int third[] = {3, 2};
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 2, 3, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_LESS_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 12);
    set_constraint(&problem, 2, third, CONSTRAINT_LESS_EQUAL, 18);
    int count = count_vertices(&problem, vertices, NULL);
    check(count == 5, "Wyndor has 5 vertices");
    check(has_vertex(vertices, count, 2, "0", "0", "0"), "Wyndor origin");
    check(has_vertex(vertices, count, 2, "4", "0", "0"), "Wyndor (4, 0)");
    check(has_vertex(vertices, count, 2, "4", "3", "0"), "Wyndor (4, 3)");
    check(has_vertex(vertices, count, 2, "2", "6", "0"), "Wyndor (2, 6)");
    check(has_vertex(vertices, count, 2, "0", "6", "0"), "Wyndor (0, 6)");
    free(vertices);
}

// Una igualdad deja solo un segmento: sus dos extremos
static void test_equality_segment(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int first[] = {1, 1};
    const int second[] = {1, 0};
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 3);
    int count = count_vertices(&problem, vertices, NULL);
    check(count == 2, "an equality leaves two vertices");
    check(has_vertex(vertices, count, 2, "3", "1", "0"), "segment end (3, 1)");
    check(has_vertex(vertices, count, 2, "0", "4", "0"), "segment end (0, 4)");
    free(vertices);
}

// Sin region factible no hay vertices
static void test_infeasible_has_no_vertices(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int row[] = {1, 1};
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 2, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 2);
    set_constraint(&problem, 1, row, CONSTRAINT_GREATER_EQUAL, 5);
    check(count_vertices(&problem, vertices, NULL) == 0, "infeasible region has no vertices");
    free(vertices);
}

// Una region no acotada: la caja agrega los vertices que cierran el dibujo
static void test_unbounded_region_with_box(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int first[] = {1, 1};
    const int second[] = {1, 0};
    Fraction limits[GEOMETRY_MAX_DIMENSION];
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_GREATER_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_GREATER_EQUAL, 1);
    check(count_vertices(&problem, vertices, NULL) == 2, "unbounded region has 2 finite vertices");

    geometry_box_limits(&problem, limits);
    check(fraction_compare(limits[0], fraction_from_int(4)) > 0, "box is larger than the largest vertex");
    int count = count_vertices(&problem, vertices, limits);
    check(count > 2, "the box adds vertices to an unbounded region");
    free(vertices);
}

// Un corte lejano con un eje no agranda la caja si los vertices ya dan el tamano
static void test_box_ignores_far_intercepts(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int first[] = {1, 1};
    const int second[] = {0, 1};
    Fraction limits[GEOMETRY_MAX_DIMENSION];

    start_problem(&problem, 2, 2, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_LESS_EQUAL, 4);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 100);
    geometry_box_limits(&problem, limits);
    check(fraction_compare(limits[0], fraction_from_int(5)) == 0, "box x limit is a quarter above the largest vertex");
    check(fraction_compare(limits[1], fraction_from_int(5)) == 0, "box y limit ignores the far intercept");
}

// Sin region factible la caja se arma con los cortes de los ejes para poder dibujar las rectas
static void test_box_without_region_uses_intercepts(void) {
    Problem problem;
    const int objective[] = {1, 1};
    const int row[] = {1, 1};
    Fraction limits[GEOMETRY_MAX_DIMENSION];

    start_problem(&problem, 2, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 2);
    set_constraint(&problem, 1, row, CONSTRAINT_GREATER_EQUAL, 8);
    geometry_box_limits(&problem, limits);
    check(fraction_compare(limits[0], fraction_from_int(10)) == 0, "box x limit uses the intercept when there is no region");
}

// Un cubo de lado 1 tiene 8 vertices
static void test_cube_vertices(void) {
    Problem problem;
    const int objective[] = {1, 1, 1};
    const int first[] = {1, 0, 0};
    const int second[] = {0, 1, 0};
    const int third[] = {0, 0, 1};
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 3, 3, objective);
    set_constraint(&problem, 0, first, CONSTRAINT_LESS_EQUAL, 1);
    set_constraint(&problem, 1, second, CONSTRAINT_LESS_EQUAL, 1);
    set_constraint(&problem, 2, third, CONSTRAINT_LESS_EQUAL, 1);
    int count = count_vertices(&problem, vertices, NULL);
    check(count == 8, "a unit cube has 8 vertices");
    check(has_vertex(vertices, count, 3, "1", "1", "1"), "cube opposite corner");
    free(vertices);
}

// Un tetraedro x1 + x2 + x3 <= 6 tiene 4 vertices aunque haya un plano de sobra
static void test_tetrahedron_vertices(void) {
    Problem problem;
    const int objective[] = {1, 2, 3};
    const int row[] = {1, 1, 1};
    const int other[] = {1, 0, 0};
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);

    start_problem(&problem, 3, 2, objective);
    set_constraint(&problem, 0, row, CONSTRAINT_LESS_EQUAL, 6);
    set_constraint(&problem, 1, other, CONSTRAINT_LESS_EQUAL, 100);
    int count = count_vertices(&problem, vertices, NULL);
    check(count == 4, "a tetrahedron with a redundant plane has 4 vertices");
    free(vertices);
}

// Un plano contiene el vertice solo si cumple la igualdad
static void test_vertex_on_plane(void) {
    HalfSpace space;
    Vertex vertex;
    space.coefficients[0] = fraction_from_int(1);
    space.coefficients[1] = fraction_from_int(1);
    space.coefficients[2] = fraction_from_int(0);
    space.type = CONSTRAINT_LESS_EQUAL;
    space.right_hand_side = fraction_from_int(4);
    vertex.coordinates[0] = fraction_from_int(1);
    vertex.coordinates[1] = fraction_from_int(3);
    vertex.coordinates[2] = fraction_from_int(0);
    check(geometry_vertex_is_on_plane(&space, &vertex, 2), "vertex on the plane");
    vertex.coordinates[1] = fraction_from_int(2);
    check(!geometry_vertex_is_on_plane(&space, &vertex, 2), "vertex off the plane");
}

// =============================================
// Random problems (vertices against the simplex engine)
// ---------------------------------------------

// Devuelve un numero al azar entre minimum y maximum (incluidos)
static int random_between(int minimum, int maximum) {
    return minimum + rand() % (maximum - minimum + 1);
}

// El mejor vertice debe dar el mismo Z que el motor, y si no hay vertices el motor debe decir no factible
static void test_vertices_match_engine(void) {
    Vertex *vertices = malloc(sizeof(Vertex) * GEOMETRY_MAX_VERTICES);
    char description[160];
    int optimal_count = 0;
    int infeasible_count = 0;

    srand(RANDOM_SEED);
    for (int index = 0; index < RANDOM_PROBLEM_COUNT; index++) {
        int variable_count = random_between(2, 3);
        int constraint_count = random_between(2, 5);
        Problem problem;
        problem_init(&problem);
        problem_set_sizes(&problem, variable_count, constraint_count);
        problem.objective_type = OBJECTIVE_MAXIMIZE;
        if (random_between(0, 1) == 1) {
            problem.objective_type = OBJECTIVE_MINIMIZE;
        }
        for (int variable = 0; variable < variable_count; variable++) {
            problem.objective[variable] = fraction_from_int(random_between(-5, 9));
        }
        for (int constraint = 0; constraint < constraint_count; constraint++) {
            for (int variable = 0; variable < variable_count; variable++) {
                problem.coefficients[constraint][variable] = fraction_from_int(random_between(-3, 9));
            }
            problem.constraint_types[constraint] = (ConstraintType)random_between(0, 2);
            problem.right_hand_sides[constraint] = fraction_from_int(random_between(0, 20));
        }

        // La ultima restriccion acota la region
        for (int variable = 0; variable < variable_count; variable++) {
            problem.coefficients[constraint_count - 1][variable] = fraction_from_int(1);
        }
        problem.constraint_types[constraint_count - 1] = CONSTRAINT_LESS_EQUAL;
        problem.right_hand_sides[constraint_count - 1] = fraction_from_int(30);

        int count = count_vertices(&problem, vertices, NULL);
        SimplexResult *result = simplex_solve(&problem);
        if (result == NULL) {
            check(0, "random problem returns a result");
            continue;
        }

        if (count == 0) {
            snprintf(description, sizeof(description), "random problem %d without vertices is infeasible", index);
            check(result->status == SIMPLEX_INFEASIBLE, description);
            infeasible_count++;
        } else {
            // Mejor Z entre los vertices, con fracciones exactas
            Fraction best = fraction_from_int(0);
            for (int vertex = 0; vertex < count; vertex++) {
                Fraction total = fraction_from_int(0);
                for (int variable = 0; variable < variable_count; variable++) {
                    Fraction product;
                    fraction_multiply(problem.objective[variable], vertices[vertex].coordinates[variable], &product);
                    fraction_add(total, product, &total);
                }
                int is_better = 0;
                if (vertex == 0) {
                    is_better = 1;
                } else if (problem.objective_type == OBJECTIVE_MAXIMIZE && fraction_compare(total, best) > 0) {
                    is_better = 1;
                } else if (problem.objective_type == OBJECTIVE_MINIMIZE && fraction_compare(total, best) < 0) {
                    is_better = 1;
                }
                if (is_better) {
                    best = total;
                }
            }
            snprintf(description, sizeof(description), "random problem %d is optimal when it has vertices", index);
            check(result->status == SIMPLEX_OPTIMAL, description);
            if (result->status == SIMPLEX_OPTIMAL) {
                snprintf(description, sizeof(description), "random problem %d best vertex matches Z", index);
                check(fraction_compare(best, result->objective_value) == 0, description);
                optimal_count++;
            }
        }
        simplex_result_free(result);
    }
    free(vertices);
    printf("Random problems: %d optimal, %d infeasible\n", optimal_count, infeasible_count);
}

// =============================================
// Main
// ---------------------------------------------
int main(void) {
    test_wyndor_vertices();
    test_equality_segment();
    test_infeasible_has_no_vertices();
    test_unbounded_region_with_box();
    test_box_ignores_far_intercepts();
    test_box_without_region_uses_intercepts();
    test_cube_vertices();
    test_tetrahedron_vertices();
    test_vertex_on_plane();
    test_vertices_match_engine();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
