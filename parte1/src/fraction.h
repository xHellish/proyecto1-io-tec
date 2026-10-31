#ifndef FRACTION_H
#define FRACTION_H

#include <stddef.h>

// =============================================
// Constants
// ---------------------------------------------
// Tamano del buffer para fraction_to_string (dos long long, signo y barra)
#define FRACTION_STRING_SIZE 48

// =============================================
// Types
// ---------------------------------------------

// Fraccion exacta: siempre simplificada y con denominador positivo
typedef struct {
    long long numerator;
    long long denominator;
} Fraction;

typedef enum {
    FRACTION_OK = 0,
    FRACTION_ERROR_OVERFLOW,
    FRACTION_ERROR_ZERO_DENOMINATOR,
    FRACTION_ERROR_EMPTY_TEXT,
    FRACTION_ERROR_INVALID_FORMAT
} FractionStatus;

// =============================================
// Functions
// ---------------------------------------------

// Crea una fraccion entera (value / 1); value debe ser un entero pequeno
Fraction fraction_from_int(long long value);

// Crea una fraccion simplificada a partir de numerador y denominador
FractionStatus fraction_make(long long numerator, long long denominator, Fraction *result);

// Operaciones exactas; devuelven error si el resultado no cabe en long long
FractionStatus fraction_add(Fraction left, Fraction right, Fraction *result);
FractionStatus fraction_subtract(Fraction left, Fraction right, Fraction *result);
FractionStatus fraction_multiply(Fraction left, Fraction right, Fraction *result);
FractionStatus fraction_divide(Fraction left, Fraction right, Fraction *result);

// Lee un numero escrito por el usuario: entero ("3"), decimal con punto o coma ("-0.25", "0,5")
// o fraccion ("1/3"). Valida el texto completo y ignora espacios en los extremos
FractionStatus fraction_parse(const char *text, Fraction *result);

// Cambia el signo (nunca desborda)
Fraction fraction_negate(Fraction value);

// Devuelve -1, 0 o 1 segun el signo
int fraction_sign(Fraction value);

// Devuelve -1, 0 o 1 si left es menor, igual o mayor que right (nunca desborda)
int fraction_compare(Fraction left, Fraction right);

// Escribe la fraccion como "3" o "1/3" en un buffer de FRACTION_STRING_SIZE
void fraction_to_string(Fraction value, char *buffer, size_t buffer_size);

// Mensaje en ingles para un estado de error
const char *fraction_status_message(FractionStatus status);

#endif
