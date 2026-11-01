#include "fraction.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

// =============================================
// Helpers
// ---------------------------------------------

// Devuelve el maximo comun divisor de dos enteros no negativos
static long long greatest_common_divisor(long long first, long long second) {
    // Algoritmo de Euclides
    while (second != 0) {
        long long remainder = first % second;
        first = second;
        second = remainder;
    }
    return first;
}

// Devuelve el valor absoluto (los valores validos nunca son LLONG_MIN)
static long long absolute_value(long long value) {
    if (value < 0) {
        return -value;
    }
    return value;
}

// =============================================
// Construction
// ---------------------------------------------

// Crea una fraccion entera (value / 1)
Fraction fraction_from_int(long long value) {
    Fraction result;
    result.numerator = value;
    result.denominator = 1;
    return result;
}

// Crea una fraccion simplificada y con denominador positivo
FractionStatus fraction_make(long long numerator, long long denominator, Fraction *result) {
    // Rechazar el denominador cero
    if (denominator == 0) {
        return FRACTION_ERROR_ZERO_DENOMINATOR;
    }

    // LLONG_MIN no se puede negar ni tomar en valor absoluto
    if (numerator == LLONG_MIN || denominator == LLONG_MIN) {
        return FRACTION_ERROR_OVERFLOW;
    }

    // Dejar el signo en el numerador
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    // Simplificar con el maximo comun divisor
    long long divisor = greatest_common_divisor(absolute_value(numerator), denominator);
    result->numerator = numerator / divisor;
    result->denominator = denominator / divisor;
    return FRACTION_OK;
}

// =============================================
// Parsing
// ---------------------------------------------

// Acumula un digito en el valor (value = value * 10 + digit) revisando desbordamiento
static FractionStatus append_digit(long long *value, int digit) {
    if (__builtin_mul_overflow(*value, 10, value)) {
        return FRACTION_ERROR_OVERFLOW;
    }
    if (__builtin_add_overflow(*value, digit, value)) {
        return FRACTION_ERROR_OVERFLOW;
    }
    return FRACTION_OK;
}

// Lee una fraccion escrita como texto; el texto completo debe ser un numero valido
FractionStatus fraction_parse(const char *text, Fraction *result) {
    // Recortar los espacios del inicio y del final
    const char *cursor = text;
    while (isspace((unsigned char)*cursor)) {
        cursor++;
    }
    const char *end = cursor + strlen(cursor);
    while (end > cursor && isspace((unsigned char)end[-1])) {
        end--;
    }
    if (end == cursor) {
        return FRACTION_ERROR_EMPTY_TEXT;
    }

    // Signo opcional
    int is_negative = 0;
    if (*cursor == '-') {
        is_negative = 1;
        cursor++;
    } else if (*cursor == '+') {
        cursor++;
    }

    // Digitos de la parte entera (todos los digitos se acumulan en el numerador)
    long long numerator = 0;
    int integer_digit_count = 0;
    while (cursor < end && isdigit((unsigned char)*cursor)) {
        FractionStatus status = append_digit(&numerator, *cursor - '0');
        if (status != FRACTION_OK) {
            return status;
        }
        integer_digit_count++;
        cursor++;
    }

    // Entero simple: "3"
    if (cursor == end) {
        if (integer_digit_count == 0) {
            return FRACTION_ERROR_INVALID_FORMAT;
        }
        if (is_negative) {
            numerator = -numerator;
        }
        return fraction_make(numerator, 1, result);
    }

    // Fraccion "a/b": ambos lados son enteros sin signo
    if (*cursor == '/') {
        if (integer_digit_count == 0) {
            return FRACTION_ERROR_INVALID_FORMAT;
        }
        cursor++;
        long long denominator = 0;
        int denominator_digit_count = 0;
        while (cursor < end && isdigit((unsigned char)*cursor)) {
            FractionStatus status = append_digit(&denominator, *cursor - '0');
            if (status != FRACTION_OK) {
                return status;
            }
            denominator_digit_count++;
            cursor++;
        }
        if (denominator_digit_count == 0 || cursor != end) {
            return FRACTION_ERROR_INVALID_FORMAT;
        }
        if (is_negative) {
            numerator = -numerator;
        }
        return fraction_make(numerator, denominator, result);
    }

    // Decimal "a.b" o "a,b": se necesita al menos un digito despues del separador
    if (*cursor == '.' || *cursor == ',') {
        cursor++;
        long long denominator = 1;
        int fraction_digit_count = 0;
        while (cursor < end && isdigit((unsigned char)*cursor)) {
            FractionStatus status = append_digit(&numerator, *cursor - '0');
            if (status != FRACTION_OK) {
                return status;
            }
            status = append_digit(&denominator, 0);
            if (status != FRACTION_OK) {
                return status;
            }
            fraction_digit_count++;
            cursor++;
        }
        if (fraction_digit_count == 0 || cursor != end) {
            return FRACTION_ERROR_INVALID_FORMAT;
        }
        if (is_negative) {
            numerator = -numerator;
        }
        return fraction_make(numerator, denominator, result);
    }

    // Cualquier otro caracter (letras, notacion cientifica, espacios en medio) es invalido
    return FRACTION_ERROR_INVALID_FORMAT;
}

// =============================================
// Arithmetic
// ---------------------------------------------

// Suma exacta usando el minimo comun multiplo de los denominadores
FractionStatus fraction_add(Fraction left, Fraction right, Fraction *result) {
    long long common_divisor = greatest_common_divisor(left.denominator, right.denominator);
    long long left_scale = right.denominator / common_divisor;
    long long right_scale = left.denominator / common_divisor;

    // Denominador comun
    long long common_denominator;
    if (__builtin_mul_overflow(left.denominator, left_scale, &common_denominator)) {
        return FRACTION_ERROR_OVERFLOW;
    }

    // Numerador: left * left_scale + right * right_scale
    long long left_part;
    long long right_part;
    long long numerator;
    if (__builtin_mul_overflow(left.numerator, left_scale, &left_part)) {
        return FRACTION_ERROR_OVERFLOW;
    }
    if (__builtin_mul_overflow(right.numerator, right_scale, &right_part)) {
        return FRACTION_ERROR_OVERFLOW;
    }
    if (__builtin_add_overflow(left_part, right_part, &numerator)) {
        return FRACTION_ERROR_OVERFLOW;
    }

    return fraction_make(numerator, common_denominator, result);
}

// Resta exacta: left + (-right)
FractionStatus fraction_subtract(Fraction left, Fraction right, Fraction *result) {
    return fraction_add(left, fraction_negate(right), result);
}

// Multiplicacion exacta simplificando en cruz antes de multiplicar
FractionStatus fraction_multiply(Fraction left, Fraction right, Fraction *result) {
    // Simplificar cada numerador con el denominador contrario
    long long left_divisor = greatest_common_divisor(absolute_value(left.numerator), right.denominator);
    long long right_divisor = greatest_common_divisor(absolute_value(right.numerator), left.denominator);

    long long numerator;
    long long denominator;
    if (__builtin_mul_overflow(left.numerator / left_divisor, right.numerator / right_divisor, &numerator)) {
        return FRACTION_ERROR_OVERFLOW;
    }
    if (__builtin_mul_overflow(left.denominator / right_divisor, right.denominator / left_divisor, &denominator)) {
        return FRACTION_ERROR_OVERFLOW;
    }

    return fraction_make(numerator, denominator, result);
}

// Division exacta: left * (1 / right)
FractionStatus fraction_divide(Fraction left, Fraction right, Fraction *result) {
    // No se puede dividir entre cero
    if (right.numerator == 0) {
        return FRACTION_ERROR_ZERO_DENOMINATOR;
    }

    // Invertir right dejando el signo en el numerador
    Fraction inverse;
    inverse.numerator = right.denominator;
    inverse.denominator = right.numerator;
    if (inverse.denominator < 0) {
        inverse.numerator = -inverse.numerator;
        inverse.denominator = -inverse.denominator;
    }

    return fraction_multiply(left, inverse, result);
}

// Cambia el signo de la fraccion
Fraction fraction_negate(Fraction value) {
    value.numerator = -value.numerator;
    return value;
}

// =============================================
// Queries
// ---------------------------------------------

// Devuelve -1, 0 o 1 segun el signo
int fraction_sign(Fraction value) {
    if (value.numerator < 0) {
        return -1;
    }
    if (value.numerator > 0) {
        return 1;
    }
    return 0;
}

// Compara con productos cruzados en 128 bits (no pueden desbordar)
int fraction_compare(Fraction left, Fraction right) {
    __int128 left_product = (__int128)left.numerator * right.denominator;
    __int128 right_product = (__int128)right.numerator * left.denominator;

    // Los denominadores son positivos, asi que el orden se conserva
    if (left_product < right_product) {
        return -1;
    }
    if (left_product > right_product) {
        return 1;
    }
    return 0;
}

// Escribe la fraccion como "3" o "1/3"
void fraction_to_string(Fraction value, char *buffer, size_t buffer_size) {
    if (value.denominator == 1) {
        snprintf(buffer, buffer_size, "%lld", value.numerator);
        return;
    }
    snprintf(buffer, buffer_size, "%lld/%lld", value.numerator, value.denominator);
}

// Mensaje para el usuario de un estado de error
const char *fraction_status_message(FractionStatus status) {
    switch (status) {
    case FRACTION_OK:
        return "Sin error.";
    case FRACTION_ERROR_OVERFLOW:
        return "Un número creció demasiado para representarlo de forma exacta.";
    case FRACTION_ERROR_ZERO_DENOMINATOR:
        return "Una fracción tiene denominador cero o se dividió entre cero.";
    case FRACTION_ERROR_EMPTY_TEXT:
        return "El campo está vacío.";
    case FRACTION_ERROR_INVALID_FORMAT:
        return "El texto no es un número válido. Use un entero, un decimal como 0.25 o una fracción como 1/3.";
    }
    return "Error desconocido de fracción.";
}
