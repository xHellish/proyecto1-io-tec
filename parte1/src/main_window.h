#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>

// =============================================
// Public functions
// ---------------------------------------------

// Toma los widgets del .glade, arma la cuadricula de coeficientes y conecta las senales.
// output_directory es la carpeta donde quedan el .tex y el .pdf (se copia).
// Devuelve el id del primer widget que falte en el .glade, o NULL si todo salio bien
const char *main_window_setup(GtkBuilder *builder, GtkWidget *window, const char *output_directory);

#endif
