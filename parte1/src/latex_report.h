#ifndef LATEX_REPORT_H
#define LATEX_REPORT_H

#include <stddef.h>

#include "problem.h"
#include "simplex.h"

// =============================================
// Public functions
// ---------------------------------------------

// Escribe en path el documento LaTeX del problema y su resultado: portada, historia y propiedades
// del Simplex, problema original, tablas con sus decisiones (las intermedias solo si
// show_intermediate_tables es 1), solucion y casos especiales. Devuelve 1 si sale bien; si falla
// devuelve 0 y escribe el motivo en message
int latex_report_write(const char *path, const Problem *problem, const SimplexResult *result,
                       int show_intermediate_tables, char *message, size_t message_size);

#endif
