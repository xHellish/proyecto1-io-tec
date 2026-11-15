#ifndef REFERENCE_PROBLEMS_H
#define REFERENCE_PROBLEMS_H

#include "problem.h"

// Pesadilla de Dantzig (tarea de clase): 4 variables, 2 restricciones, lados derechos en 0
void build_dantzig_nightmare(Problem *problem);

// Wyndor Glass: maximizar 3x1 + 5x2 con 3 restricciones; el optimo es Z = 36 en (2, 6)
void build_wyndor_glass(Problem *problem);

// Cuaderno de clase (04/09): maximizar 3x1 + 4x2 con x1 + x2 <= 40 y x1 + 2x2 <= 60; Z = 140 en (20, 20)
void build_class_notes_maximization(Problem *problem);

// Dakota (tarea de clase): maximizar 60x1 + 30x2 + 20x3 con 4 restricciones; Z = 280 en (2, 0, 8)
void build_dakota(Problem *problem);

// Dakota con x2 en 35 (cuaderno 09/09): soluciones multiples entre (2, 0, 8) y (0, 8/5, 56/5), Z = 280
void build_dakota_multiple(Problem *problem);

// Cuaderno: maximizar 4x1 + 14x2 con 2x1 + 7x2 <= 21 y 7x1 + 2x2 <= 21; Z = 42 en (0, 3) y (7/3, 7/3)
void build_class_four_fourteen(Problem *problem);

// Minimizacion del cuaderno (09/09): minimizar 2x1 - 3x2 con x1 + x2 <= 4 y x1 - x2 <= 6; Z = -12 en (0, 4)
void build_class_minimization(Problem *problem);

// Panaderia BreadCo (cuaderno): maximizar 36x1 + 30x2 - 3x3 - 4x4; no acotado al entrar x3 en la tercera tabla
void build_breadco_unbounded(Problem *problem);

// Minimizacion de ejemplo: minimizar x1 - 3x2 con -x1 + x2 <= 2 y x1 + x2 <= 6; Z = -10 en (2, 4)
void build_minimization_example(Problem *problem);

// Soluciones multiples de clase: maximizar 2x1 + 4x2 con x1 + 2x2 <= 5 y x1 + x2 <= 4; Z = 10 en (0, 5/2) y (3, 1)
void build_class_multiple_solutions(Problem *problem);

// Degenerado de clase: maximizar 2x1 + x2 con 3x1 + x2 <= 6, x1 - x2 <= 2 y x2 <= 3; Z = 5 en (1, 3)
void build_class_degenerate(Problem *problem);

#endif
