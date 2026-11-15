#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <stddef.h>

// =============================================
// Public functions
// ---------------------------------------------
// Todas devuelven 1 si salen bien; si fallan devuelven 0 y escriben el motivo en message

// Arma un nombre de archivo seguro a partir del nombre del problema: sin tildes, espacios ni
// simbolos (los espacios pasan a "_"); si no queda nada usa "problema"
void launcher_file_name(const char *problem_name, char *buffer, size_t buffer_size);

// Crea la carpeta de salida si no existe
int launcher_prepare_output_dir(const char *output_dir, char *message, size_t message_size);

// Compila tex_file_name con pdflatex dentro de output_dir, sin interaccion; lo que pdflatex
// escribe queda en su .log y nunca en la terminal
int launcher_compile(const char *output_dir, const char *tex_file_name, char *message, size_t message_size);

// Abre el PDF en modo presentacion con evince (u okular si evince no esta) sin bloquear la interfaz
int launcher_open_pdf(const char *pdf_path, char *message, size_t message_size);

#endif
