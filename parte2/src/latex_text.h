#ifndef LATEX_TEXT_H
#define LATEX_TEXT_H

#include <glib.h>

#include "fraction.h"
#include "problem.h"

// =============================================
// Public functions
// ---------------------------------------------

// Escribe una fraccion como LaTeX para usar dentro de modo matematico: 3, \frac{1}{2} o -\frac{1}{2}
void latex_append_fraction(GString *output, Fraction value);

// Escribe un numero decimal con punto (nunca coma, sin importar el idioma del sistema) para TikZ
void latex_append_decimal(GString *output, double value);

// Escribe una restriccion en una linea de texto: cada termino en su propio modo matematico, la relacion
// y el lado derecho. Los nombres de las variables se insertan tal como los escribio el usuario
void latex_append_constraint(GString *output, const Problem *problem, int constraint);

#endif
