#ifndef PLOTS_H
#define PLOTS_H

#include <glib.h>

#include "problem.h"
#include "simplex.h"

// =============================================
// Public functions
// ---------------------------------------------

// Agrega la página con el gráfico de un problema de 2 variables: cada restricción como una recta, la región
// factible rellena, el óptimo (o los óptimos) marcado y la recta de la función objetivo. Con una región
// vacía, no acotada o con soluciones múltiples el gráfico lo muestra y una nota lo explica.
// Solo dibuja si el problema tiene exactamente 2 variables
void plots_append_region_2d(GString *output, const Problem *problem, const SimplexResult *result);

#endif
