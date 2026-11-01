#ifndef TEXT_REPORT_H
#define TEXT_REPORT_H

#include <glib.h>

#include "problem.h"
#include "simplex.h"

// =============================================
// Public functions
// ---------------------------------------------

// Agrega a output el problema, el estado, todas las tablas del historial con sus
// decisiones y la solucion. Es temporal: se reemplaza por el documento LaTeX en la Fase 3
void text_report_append(GString *output, const Problem *problem, const SimplexResult *result);

#endif
