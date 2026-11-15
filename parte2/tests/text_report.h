#ifndef TEXT_REPORT_H
#define TEXT_REPORT_H

#include <glib.h>

#include "problem.h"
#include "simplex.h"

// =============================================
// Public functions
// ---------------------------------------------

// Agrega a output el problema, el estado, todas las tablas del historial con sus
// decisiones y la solucion. Solo lo usa la ventana de prueba; el programa genera un PDF
void text_report_append(GString *output, const Problem *problem, const SimplexResult *result);

#endif
