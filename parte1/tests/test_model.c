#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "fraction.h"
#include "problem.h"

// =============================================
// Test helpers
// ---------------------------------------------
static int checks_run = 0;
static int checks_failed = 0;

// Registra una verificacion y avisa si fallo
static void check(int condition, const char *description) {
    checks_run++;
    if (!condition) {
        checks_failed++;
        printf("FAIL: %s\n", description);
    }
}

// Verifica que una fraccion se escriba como el texto esperado
static void check_fraction(Fraction value, const char *expected, const char *description) {
    char text[FRACTION_STRING_SIZE];
    fraction_to_string(value, text, FRACTION_STRING_SIZE);
    if (strcmp(text, expected) != 0) {
        printf("FAIL: %s (expected %s, got %s)\n", description, expected, text);
        checks_failed++;
    }
    checks_run++;
}

// Crea una fraccion y falla el programa si los datos de la prueba son invalidos
static Fraction make(long long numerator, long long denominator) {
    Fraction value;
    FractionStatus status = fraction_make(numerator, denominator, &value);
    if (status != FRACTION_OK) {
        printf("Invalid test data: %s\n", fraction_status_message(status));
        value = fraction_from_int(0);
        checks_failed++;
    }
    return value;
}

// Verifica el resultado de leer un texto: estado esperado y, si es valido, el valor
static void check_parse(const char *text, FractionStatus expected_status, const char *expected_value) {
    Fraction value = fraction_from_int(0);
    FractionStatus status = fraction_parse(text, &value);
    char description[128];
    snprintf(description, sizeof(description), "parse \"%s\"", text);

    check(status == expected_status, description);
    if (status == FRACTION_OK && expected_status == FRACTION_OK) {
        check_fraction(value, expected_value, description);
    }
}

// =============================================
// Fraction tests
// ---------------------------------------------

// Normalizacion al crear fracciones
static void test_fraction_make(void) {
    check_fraction(make(2, 4), "1/2", "2/4 is simplified");
    check_fraction(make(3, -6), "-1/2", "sign moves to the numerator");
    check_fraction(make(0, 5), "0", "zero has denominator 1");
    check_fraction(make(-4, -8), "1/2", "two negatives give a positive");

    Fraction value;
    check(fraction_make(1, 0, &value) == FRACTION_ERROR_ZERO_DENOMINATOR, "zero denominator is rejected");
    check(fraction_make(LLONG_MIN, 1, &value) == FRACTION_ERROR_OVERFLOW, "LLONG_MIN numerator is rejected");
}

// Aritmetica exacta
static void test_fraction_arithmetic(void) {
    Fraction result;

    fraction_add(make(1, 3), make(1, 6), &result);
    check_fraction(result, "1/2", "1/3 + 1/6");

    fraction_subtract(make(1, 3), make(1, 2), &result);
    check_fraction(result, "-1/6", "1/3 - 1/2");

    fraction_multiply(make(2, 3), make(-9, 4), &result);
    check_fraction(result, "-3/2", "2/3 * -9/4");

    fraction_divide(make(1, 2), make(-1, 3), &result);
    check_fraction(result, "-3/2", "1/2 / -1/3");

    fraction_multiply(make(0, 1), make(7, 3), &result);
    check_fraction(result, "0", "0 * 7/3");

    check(fraction_divide(make(1, 2), make(0, 1), &result) == FRACTION_ERROR_ZERO_DENOMINATOR,
          "division by zero is rejected");
}

// Deteccion de desbordamiento
static void test_fraction_overflow(void) {
    Fraction result;
    Fraction largest = make(LLONG_MAX, 1);

    check(fraction_add(largest, make(1, 1), &result) == FRACTION_ERROR_OVERFLOW, "addition overflow");
    check(fraction_multiply(largest, make(2, 1), &result) == FRACTION_ERROR_OVERFLOW, "multiplication overflow");
    check(fraction_subtract(fraction_negate(largest), make(2, 1), &result) == FRACTION_ERROR_OVERFLOW,
          "subtraction overflow");
    check(fraction_add(make(1, 3), make(2, 3), &result) == FRACTION_OK, "no false overflow");
    check_fraction(result, "1", "1/3 + 2/3");
}

// Comparacion y signo
static void test_fraction_compare(void) {
    check(fraction_compare(make(1, 3), make(2, 6)) == 0, "1/3 equals 2/6");
    check(fraction_compare(make(1, 3), make(1, 2)) == -1, "1/3 < 1/2");
    check(fraction_compare(make(-1, 3), make(-1, 2)) == 1, "-1/3 > -1/2");
    check(fraction_compare(make(LLONG_MAX, 1), make(LLONG_MAX - 1, 1)) == 1, "comparison does not overflow");
    check(fraction_sign(make(-5, 7)) == -1, "negative sign");
    check(fraction_sign(make(0, 7)) == 0, "zero sign");
    check(fraction_sign(make(5, 7)) == 1, "positive sign");
}

// Lectura de texto escrito por el usuario
static void test_fraction_parse(void) {
    // Formatos validos
    check_parse("3", FRACTION_OK, "3");
    check_parse("-3", FRACTION_OK, "-3");
    check_parse("+3", FRACTION_OK, "3");
    check_parse("0", FRACTION_OK, "0");
    check_parse("0.25", FRACTION_OK, "1/4");
    check_parse("-0.25", FRACTION_OK, "-1/4");
    check_parse("0,5", FRACTION_OK, "1/2");
    check_parse(".5", FRACTION_OK, "1/2");
    check_parse("12.50", FRACTION_OK, "25/2");
    check_parse("1/3", FRACTION_OK, "1/3");
    check_parse("-1/3", FRACTION_OK, "-1/3");
    check_parse("2/4", FRACTION_OK, "1/2");
    check_parse("  7  ", FRACTION_OK, "7");

    // Texto vacio
    check_parse("", FRACTION_ERROR_EMPTY_TEXT, "");
    check_parse("   ", FRACTION_ERROR_EMPTY_TEXT, "");

    // Se valida el texto completo, no solo el principio
    check_parse("12abc", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("abc", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1e5", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("--1", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("-", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse(".", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("5.", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1.2.3", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1 2", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1/", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("/3", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1/-3", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1/2/3", FRACTION_ERROR_INVALID_FORMAT, "");
    check_parse("1.5/2", FRACTION_ERROR_INVALID_FORMAT, "");

    // Division entre cero y numeros enormes
    check_parse("1/0", FRACTION_ERROR_ZERO_DENOMINATOR, "");
    check_parse("99999999999999999999", FRACTION_ERROR_OVERFLOW, "");
    check_parse("0.99999999999999999999", FRACTION_ERROR_OVERFLOW, "");
}

// =============================================
// Problem tests
// ---------------------------------------------

// Valores por defecto
static void test_problem_defaults(void) {
    Problem problem;
    problem_init(&problem);

    check(problem.variable_count == 2 && problem.constraint_count == 2, "default size is 2 x 2");
    check(problem.objective_type == OBJECTIVE_MAXIMIZE, "default objective is maximize");
    check(strcmp(problem.variable_names[0], "$x_1$") == 0, "first default name is $x_1$");
    check(strcmp(problem.variable_names[8], "$x_9$") == 0, "ninth default name is $x_9$");
    check(strcmp(problem.variable_names[9], "$x_{10}$") == 0, "tenth default name uses braces");
    check(problem.constraint_types[0] == CONSTRAINT_LESS_EQUAL, "default constraint is <=");
    check_fraction(problem.objective[14], "0", "default objective coefficient is 0");
    check_fraction(problem.coefficients[14][14], "0", "default constraint coefficient is 0");
    check_fraction(problem.right_hand_sides[14], "0", "default right-hand side is 0");

    char slack_name[NAME_BUFFER_SIZE];
    problem_slack_name(0, slack_name, NAME_BUFFER_SIZE);
    check(strcmp(slack_name, "$s_1$") == 0, "first slack name");
    problem_slack_name(11, slack_name, NAME_BUFFER_SIZE);
    check(strcmp(slack_name, "$s_{12}$") == 0, "twelfth slack name uses braces");
}

// Cambio de cantidades y de nombres
static void test_problem_setters(void) {
    Problem problem;
    problem_init(&problem);

    check(problem_set_sizes(&problem, 1, 2) == PROBLEM_ERROR_VARIABLE_COUNT, "1 variable is rejected");
    check(problem_set_sizes(&problem, 16, 2) == PROBLEM_ERROR_VARIABLE_COUNT, "16 variables are rejected");
    check(problem_set_sizes(&problem, 2, 1) == PROBLEM_ERROR_CONSTRAINT_COUNT, "1 constraint is rejected");
    check(problem_set_sizes(&problem, 2, 16) == PROBLEM_ERROR_CONSTRAINT_COUNT, "16 constraints are rejected");
    check(problem_set_sizes(&problem, 15, 15) == PROBLEM_OK, "15 x 15 is accepted");
    check(problem_set_sizes(&problem, 2, 2) == PROBLEM_OK, "2 x 2 is accepted");
    check(problem.variable_count == 2, "rejected sizes do not change the problem");

    // Los datos escritos se conservan al achicar y volver a agrandar
    problem_set_sizes(&problem, 6, 6);
    problem.coefficients[4][5] = make(7, 3);
    problem_set_sizes(&problem, 3, 3);
    problem_set_sizes(&problem, 6, 6);
    check_fraction(problem.coefficients[4][5], "7/3", "data survives a size change");

    // Nombres
    check(problem_set_name(&problem, "Wyndor Glass") == PROBLEM_OK, "problem name is set");
    check(strcmp(problem.name, "Wyndor Glass") == 0, "problem name is stored");
    check(problem_set_variable_name(&problem, 0, "$doors$") == PROBLEM_OK, "variable name is set");
    check(problem_set_variable_name(&problem, 15, "x") == PROBLEM_ERROR_INDEX_OUT_OF_RANGE, "bad index is rejected");
    check(problem_set_variable_name(&problem, -1, "x") == PROBLEM_ERROR_INDEX_OUT_OF_RANGE, "negative index is rejected");

    char long_name[200];
    memset(long_name, 'a', sizeof(long_name));
    long_name[199] = '\0';
    check(problem_set_name(&problem, long_name) == PROBLEM_ERROR_NAME_TOO_LONG, "long problem name is rejected");
    check(problem_set_variable_name(&problem, 1, long_name) == PROBLEM_ERROR_NAME_TOO_LONG,
          "long variable name is rejected");
    check(strcmp(problem.name, "Wyndor Glass") == 0, "rejected name leaves the old one");
}

// Validacion con mensajes que indican el campo
static void test_problem_validation(void) {
    Problem problem;
    char message[MESSAGE_BUFFER_SIZE];
    problem_init(&problem);

    // Un problema recien creado no tiene nombre
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_EMPTY_PROBLEM_NAME,
          "fresh problem has no name");
    check(strstr(message, "problem name") != NULL, "message mentions the problem name");

    problem_set_name(&problem, "   ");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_EMPTY_PROBLEM_NAME,
          "blank problem name is rejected");

    problem_set_name(&problem, "Test");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_OK, "default problem with a name is valid");
    check(strlen(message) == 0, "message is empty when valid");

    // Nombre de variable vacio
    problem_set_variable_name(&problem, 1, "  ");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_EMPTY_VARIABLE_NAME,
          "blank variable name is rejected");
    check(strstr(message, "variable 2") != NULL, "message names variable 2");

    // Nombres repetidos (ignorando espacios en los extremos)
    problem_set_variable_name(&problem, 1, " $x_1$ ");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_DUPLICATE_VARIABLE_NAME,
          "duplicate names are rejected");
    check(strstr(message, "1 and 2") != NULL, "message names both variables");

    // Nombre igual al de una holgura
    problem_set_variable_name(&problem, 1, "$s_2$");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_SLACK_NAME_CONFLICT,
          "slack name conflict is rejected");
    check(strstr(message, "variable 2") != NULL, "message names the conflicting variable");

    // Una holgura que no existe en este tamano no choca
    problem_set_variable_name(&problem, 1, "$s_3$");
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_OK,
          "name of a slack beyond the constraint count is allowed");
    problem_set_variable_name(&problem, 1, "$x_2$");

    // Lado derecho negativo
    problem.right_hand_sides[1] = make(-1, 3);
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_NEGATIVE_RIGHT_HAND_SIDE,
          "negative right-hand side is rejected");
    check(strstr(message, "constraint 2") != NULL, "message names constraint 2");

    // Un lado derecho negativo fuera de las restricciones usadas no cuenta
    problem.right_hand_sides[1] = make(0, 1);
    problem.right_hand_sides[5] = make(-4, 1);
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_OK,
          "unused constraints are not validated");

    // Cantidades fuera de rango
    problem.variable_count = 20;
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_VARIABLE_COUNT,
          "variable count out of range");
    problem.variable_count = 2;
    problem.constraint_count = 1;
    check(problem_validate(&problem, message, sizeof(message)) == PROBLEM_ERROR_CONSTRAINT_COUNT,
          "constraint count out of range");
}

// =============================================
// Main
// ---------------------------------------------

// Corre todas las pruebas del modelo e informa el resultado
int main(void) {
    test_fraction_make();
    test_fraction_arithmetic();
    test_fraction_overflow();
    test_fraction_compare();
    test_fraction_parse();
    test_problem_defaults();
    test_problem_setters();
    test_problem_validation();

    printf("%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed > 0) {
        return 1;
    }
    return 0;
}
