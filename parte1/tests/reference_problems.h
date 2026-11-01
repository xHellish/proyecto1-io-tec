#ifndef REFERENCE_PROBLEMS_H
#define REFERENCE_PROBLEMS_H

#include "problem.h"

// Pesadilla de Dantzig (tarea de clase): 4 variables, 2 restricciones, lados derechos en 0
void build_dantzig_nightmare(Problem *problem);

// Wyndor Glass: maximizar 3x1 + 5x2 con 3 restricciones; el optimo es Z = 36 en (2, 6)
void build_wyndor_glass(Problem *problem);

// Minimizacion de ejemplo: minimizar x1 - 3x2 con -x1 + x2 <= 2 y x1 + x2 <= 6; Z = -10 en (2, 4)
void build_minimization_example(Problem *problem);

#endif
