#include "latex_report.h"

#include <glib.h>
#include <stdio.h>

// =============================================
// Constants
// ---------------------------------------------
// Datos de la portada
#define PROJECT_TITLE "Proyecto 1: Otro SIMPLEX más"
#define PROJECT_PART "Parte 1"
#define INSTITUTION_NAME "Instituto Tecnológico de Costa Rica"
#define PROGRAM_NAME "Ingeniería en Computación"
#define COURSE_NAME "Investigación de Operaciones"
#define SEMESTER_NAME "Segundo semestre, 2026"
#define PROFESSOR_NAME "Francisco Torres Rojas"

static const char *STUDENT_NAMES[] = {
    "Ben Farzamipour Alfaro",
    "Johel Soto Jara",
    "Anthony Camacho Palma",
};

// Logo relativo a la carpeta de salida, donde corre pdflatex; si no existe se omite
#define LOGO_PATH "../assets/tec_logo.png"

// Alto maximo de una tabla segun lo que la acompana en la pagina
#define TABLE_HEIGHT_WITH_DECISIONS "0.5\\textheight"
#define TABLE_HEIGHT_WITH_TEXT "0.5\\textheight"
#define VALUES_HEIGHT "0.52\\textheight"
#define POINTS_HEIGHT "0.36\\textheight"

// Columnas de una tabla (de Z al lado derecho) a partir de las cuales se reduce el relleno entre columnas
#define WIDE_TABLE_COLUMNS 14

// Filas de una tabla (con la objetivo) a partir de las cuales se juntan las filas y se da mas alto
#define TALL_TABLE_ROWS 8
#define TABLE_HEIGHT_TALL "0.6\\textheight"

// Soluciones adicionales que se reportan cuando hay soluciones multiples
#define EXTRA_POINT_COUNT 3

// =============================================
// Text helpers
// ---------------------------------------------

// Agrega una fraccion en modo matematico: 3, -3, \frac{1}{3} o -\frac{1}{3}
static void append_fraction(GString *output, Fraction value) {
    if (value.denominator == 1) {
        g_string_append_printf(output, "%lld", value.numerator);
        return;
    }
    if (value.numerator < 0) {
        g_string_append_printf(output, "-\\frac{%lld}{%lld}", -value.numerator, value.denominator);
        return;
    }
    g_string_append_printf(output, "\\frac{%lld}{%lld}", value.numerator, value.denominator);
}

// Agrega texto del usuario escapando los caracteres especiales de LaTeX
static void append_escaped(GString *output, const char *text) {
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        switch (*cursor) {
        case '&':
        case '%':
        case '$':
        case '#':
        case '_':
        case '{':
        case '}':
            g_string_append_c(output, '\\');
            g_string_append_c(output, *cursor);
            break;
        case '~':
            g_string_append(output, "\\textasciitilde{}");
            break;
        case '^':
            g_string_append(output, "\\textasciicircum{}");
            break;
        case '\\':
            g_string_append(output, "\\textbackslash{}");
            break;
        default:
            g_string_append_c(output, *cursor);
            break;
        }
    }
}

// Agrega el nombre de una columna: Z, el nombre de la variable del usuario, su holgura o LD
static void append_column_label(GString *output, const Problem *problem, const Table *table, int column) {
    char slack_name[NAME_BUFFER_SIZE];
    ColumnType type = table->column_types[column];

    if (type == COLUMN_Z) {
        g_string_append(output, "$Z$");
        return;
    }
    if (type == COLUMN_ORIGINAL) {
        // Los nombres de variables son LaTeX del usuario ($x_1$ por defecto) y se escriben tal cual
        g_string_append(output, problem->variable_names[column - 1]);
        return;
    }
    if (type == COLUMN_SLACK) {
        problem_slack_name(column - problem->variable_count - 1, slack_name, NAME_BUFFER_SIZE);
        g_string_append(output, slack_name);
        return;
    }
    g_string_append(output, "LD");
}

// Agrega la lista de nombres de variables originales separados por comas
static void append_variable_names(GString *output, const Problem *problem) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (variable > 0) {
            g_string_append(output, ", ");
        }
        g_string_append(output, problem->variable_names[variable]);
    }
}

// Agrega una lista de numeros de tabla como "1, 2 y 5"
static void append_step_list(GString *output, const int steps[], int count) {
    for (int index = 0; index < count; index++) {
        if (index > 0 && index == count - 1) {
            g_string_append(output, " y ");
        } else if (index > 0) {
            g_string_append(output, ", ");
        }
        g_string_append_printf(output, "%d", steps[index]);
    }
}

// =============================================
// Document setup
// ---------------------------------------------

// Agrega el preambulo: paginas 16:9, colores del TEC, encabezado y comandos de diseno
static void append_preamble(GString *output, const Problem *problem) {
    g_string_append(output,
                    "\\documentclass[10pt]{article}\n"
                    "\\usepackage[T1]{fontenc}\n"
                    "\\usepackage[utf8]{inputenc}\n"
                    "\\usepackage{lmodern}\n"
                    "\\renewcommand{\\familydefault}{\\sfdefault}\n"
                    "\\usepackage[spanish,es-noshorthands]{babel}\n"
                    "\\usepackage[paperwidth=160mm,paperheight=90mm,left=8mm,right=8mm,top=15mm,bottom=8mm,"
                    "headheight=9mm,headsep=2mm,footskip=4.5mm]{geometry}\n"
                    "\\usepackage{amsmath,graphicx,array,fancyhdr}\n"
                    "\\usepackage[table]{xcolor}\n"
                    "\\usepackage{tikz}\n"
                    "\\usetikzlibrary{arrows.meta}\n"
                    "\n"
                    "% Colores del TEC y de las marcas de las tablas\n"
                    "\\definecolor{tecblue}{RGB}{0,41,84}\n"
                    "\\definecolor{tecred}{RGB}{228,0,43}\n"
                    "\\definecolor{textgray}{RGB}{88,96,108}\n"
                    "\\definecolor{zrow}{RGB}{238,241,246}\n"
                    "\\definecolor{entering}{RGB}{206,226,246}\n"
                    "\\definecolor{leaving}{RGB}{253,225,208}\n"
                    "\\definecolor{pivot}{RGB}{255,206,84}\n"
                    "\\definecolor{okgreen}{RGB}{24,128,72}\n"
                    "\\definecolor{warnorange}{RGB}{204,102,0}\n"
                    "\n"
                    "% Tablas y parrafos\n"
                    "\\setlength{\\parindent}{0pt}\n"
                    "\\hyphenpenalty=10000\n"
                    "\\exhyphenpenalty=10000\n"
                    "\\setlength{\\parskip}{3pt}\n"
                    "\\renewcommand{\\arraystretch}{1.25}\n"
                    "\\setlength{\\tabcolsep}{5pt}\n"
                    "\\arrayrulecolor{black!25}\n"
                    "\n"
                    "% Encabezado con el proyecto y el logo, pie con el problema y la pagina\n"
                    "\\pagestyle{fancy}\n"
                    "\\fancyhf{}\n"
                    "\\fancyhead[L]{\\color{tecblue}\\scriptsize\\bfseries " PROJECT_TITLE " \\textperiodcentered{} "
                    PROJECT_PART "}\n"
                    "\\fancyhead[R]{\\IfFileExists{" LOGO_PATH "}{\\includegraphics[height=3.6mm]{" LOGO_PATH
                    "}}{\\color{tecblue}\\scriptsize\\bfseries TEC}}\n"
                    "\\fancyfoot[L]{\\color{textgray}\\scriptsize ");
    append_escaped(output, problem->name);
    g_string_append(output,
                    "}\n"
                    "\\fancyfoot[R]{\\color{textgray}\\scriptsize\\thepage}\n"
                    "\\renewcommand{\\headrulewidth}{0.5pt}\n"
                    "\\renewcommand{\\headrule}{\\hbox to\\headwidth{\\color{tecred}\\leaders\\hrule height "
                    "\\headrulewidth\\hfill}}\n"
                    "\n"
                    "% Cada seccion es una pagina con titulo azul y una raya roja\n"
                    "\\newcommand{\\slide}[1]{\\clearpage{\\color{tecblue}\\Large\\bfseries #1\\par}"
                    "\\vspace{0.5mm}{\\color{tecred}\\rule{16mm}{1.2pt}}\\par\\vspace{2mm}}\n"
                    "\n"
                    "% Recuadro de color con titulo: color, titulo y contenido\n"
                    "\\newcommand{\\callout}[3]{\\par\\noindent\\tikz\\node[fill=#1!8,draw=#1,line width=0.6pt,"
                    "rounded corners=3pt,inner xsep=6pt,inner ysep=5pt,text width=\\linewidth-13.5pt,align=left]"
                    "{{\\color{#1}\\bfseries #2}\\\\[1pt]#3};\\par}\n"
                    "\n"
                    "% Etiqueta de color en linea (entra, sale, pivote)\n"
                    "\\newcommand{\\chip}[2]{\\tikz[baseline=(c.base)]\\node[fill=#1,rounded corners=2pt,"
                    "inner xsep=4pt,inner ysep=2.5pt](c){\\strut #2};}\n"
                    "\n"
                    "% Pivote encerrado en una cajita y flechas de la columna que entra y la fila que sale\n"
                    "\\newcommand{\\pivotcell}[1]{\\tikz[baseline=(p.base)]\\node[draw=tecred,fill=pivot,"
                    "rounded corners=2pt,line width=0.8pt,inner sep=1.8pt](p){#1};}\n"
                    "\\newcommand{\\enterarrow}{\\tikz\\draw[-{Stealth[length=2.2mm]},line width=1.1pt,tecred]"
                    "(0,0.42)--(0,0);}\n"
                    "\\newcommand{\\leavearrow}{\\tikz[baseline=-0.6ex]\\draw[-{Stealth[length=2.2mm]},"
                    "line width=1.1pt,tecred](0.5,0)--(0,0);}\n"
                    "\n"
                    "% Ajusta el contenido guardado en \\fitbox al ancho de la pagina y al alto indicado\n"
                    "\\newsavebox{\\fitbox}\n"
                    "\\newlength{\\fitheight}\n"
                    "\\newcommand{\\showfitted}[1]{%\n"
                    "\\ifdim\\wd\\fitbox>\\linewidth\\sbox{\\fitbox}{\\resizebox{\\linewidth}{!}"
                    "{\\usebox{\\fitbox}}}\\fi\n"
                    "\\setlength{\\fitheight}{\\ht\\fitbox}\\addtolength{\\fitheight}{\\dp\\fitbox}%\n"
                    "\\ifdim\\fitheight>#1\\sbox{\\fitbox}{\\resizebox*{!}{#1}{\\usebox{\\fitbox}}}\\fi\n"
                    "\\par\\centerline{\\usebox{\\fitbox}}\\par}\n"
                    "\n"
                    "\\begin{document}\n"
                    "\\raggedright\n");
}

// Agrega la portada: logo, curso, nombre del proyecto, problema, estudiantes y profesor
static void append_cover(GString *output, const Problem *problem) {
    int student_count = (int)(sizeof(STUDENT_NAMES) / sizeof(STUDENT_NAMES[0]));

    // Logo a la izquierda y datos del curso a la derecha
    g_string_append(output,
                    "\\newgeometry{left=8mm,right=8mm,top=7mm,bottom=7mm}\n"
                    "\\thispagestyle{empty}\n"
                    "\\begin{minipage}[c]{0.5\\linewidth}\n"
                    "\\IfFileExists{" LOGO_PATH "}{\\includegraphics[height=10mm]{" LOGO_PATH "}}"
                    "{{\\color{tecblue}\\Large\\bfseries " INSTITUTION_NAME "}}\n"
                    "\\end{minipage}\\hfill\n"
                    "\\begin{minipage}[c]{0.45\\linewidth}\\raggedleft\\color{tecblue}\\small\n"
                    INSTITUTION_NAME "\\\\\n"
                    PROGRAM_NAME "\\\\\n"
                    "\\textbf{" COURSE_NAME "}\\\\\n"
                    SEMESTER_NAME "\n"
                    "\\end{minipage}\n"
                    "\\par\\vfill\n");

    // Nombre del proyecto y del problema
    g_string_append(output,
                    "{\\color{tecblue}\\fontsize{21}{25}\\selectfont\\bfseries " PROJECT_TITLE "\\par}\n"
                    "\\vspace{1mm}{\\color{tecred}\\rule{30mm}{1.6pt}}\\par\\vspace{1.5mm}\n"
                    "{\\large\\color{textgray}" PROJECT_PART " \\textperiodcentered{} Método Símplex\\par}\n"
                    "\\vspace{2.5mm}{\\large Problema: \\textbf{");
    append_escaped(output, problem->name);
    g_string_append(output, "}\\par}\n\\vfill\n");

    // Franja azul con estudiantes, profesor y fecha; el relleno lo da fboxsep
    g_string_append(output,
                    "{\\setlength{\\fboxsep}{2.5mm}\\noindent\\colorbox{tecblue}{"
                    "\\begin{minipage}{\\dimexpr\\linewidth-2\\fboxsep\\relax}\\color{white}\n"
                    "\\begin{minipage}[t]{0.5\\linewidth}\\small\\textbf{Estudiantes}\\\\[1mm]\n");
    for (int index = 0; index < student_count; index++) {
        // Sin salto despues del ultimo nombre para no dejar un renglon vacio
        if (index < student_count - 1) {
            g_string_append_printf(output, "%s\\\\\n", STUDENT_NAMES[index]);
        } else {
            g_string_append_printf(output, "%s\n", STUDENT_NAMES[index]);
        }
    }
    g_string_append(output,
                    "\\end{minipage}\\hfill\n"
                    "\\begin{minipage}[t]{0.42\\linewidth}\\small\\raggedleft\\textbf{Profesor}\\\\[1mm]\n"
                    PROFESSOR_NAME "\\\\[2mm]\n"
                    "\\textbf{Fecha}\\\\[1mm]\\today\n"
                    "\\end{minipage}\n"
                    "\\end{minipage}}}\n"
                    "\\restoregeometry\n");
}

// Agrega la historia del algoritmo Simplex
static void append_simplex_history(GString *output) {
    g_string_append(output,
                    "\\slide{El algoritmo Símplex: historia}\n"
                    "\\small\n"
                    "En \\textbf{1947}, \\textbf{George B. Dantzig} trabajaba en problemas de planificación para "
                    "la Fuerza Aérea de los Estados Unidos. Allí formuló el problema general de programación "
                    "lineal y diseñó el \\textbf{método Símplex} para resolverlo. Ese mismo año, John von "
                    "Neumann planteó la teoría de dualidad, que relaciona cada problema lineal con otro.\n\n"
                    "Antes, en 1939, \\textbf{Leonid Kantoróvich} había usado modelos lineales para planificar "
                    "la producción en la Unión Soviética, y \\textbf{Tjalling Koopmans} estudió problemas de "
                    "transporte. Ambos recibieron en 1975 el Premio Nobel de Economía por sus aportes a la "
                    "asignación óptima de recursos.\n\n"
                    "El nombre, sugerido por Theodore Motzkin, viene de una interpretación geométrica del "
                    "método en términos de \\emph{símplices}. En 1972, Victor Klee y George Minty construyeron "
                    "problemas en los que el Símplex recorre una cantidad exponencial de vértices; eso motivó "
                    "algoritmos de tiempo polinomial, como el del elipsoide (Khachiyan, 1979) y los de punto "
                    "interior (Karmarkar, 1984). Aun así, el Símplex sigue siendo uno de los métodos más usados "
                    "en la práctica, y en el año 2000 fue incluido entre los diez algoritmos más influyentes "
                    "del siglo XX.\n");
}

// Agrega las propiedades del algoritmo Simplex
static void append_simplex_properties(GString *output) {
    g_string_append(output,
                    "\\slide{El algoritmo Símplex: propiedades}\n"
                    "\\small\n"
                    "\\begin{itemize}\\setlength{\\itemsep}{1pt}\n"
                    "\\item La región factible es un poliedro convexo y, si hay un óptimo, se alcanza en un "
                    "\\textbf{vértice}.\n"
                    "\\item Cada tabla es una \\textbf{solución básica factible}: las variables básicas valen el "
                    "lado derecho de su fila y las no básicas valen 0.\n"
                    "\\item Cada pivoteo pasa a un vértice vecino sin empeorar $Z$; si la fracción mínima es "
                    "positiva, $Z$ mejora.\n"
                    "\\item La tabla es \\textbf{óptima} cuando ningún valor de la fila $Z$ puede mejorar el "
                    "objetivo.\n"
                    "\\item Si la columna que entra no tiene coeficientes positivos, el problema es "
                    "\\textbf{no acotado}.\n"
                    "\\item Una variable no básica con 0 en la fila $Z$ de la tabla óptima indica "
                    "\\textbf{soluciones múltiples}.\n"
                    "\\item Si una variable básica vale 0 el problema es \\textbf{degenerado} y existe el riesgo "
                    "de ciclar.\n"
                    "\\item En la práctica suele terminar en pocas iteraciones, aunque su peor caso es "
                    "exponencial.\n"
                    "\\end{itemize}\n");
}

// Agrega la guia de lectura: las reglas usadas en este problema y el significado de los colores
static void append_reading_guide(GString *output, const Problem *problem) {
    g_string_append(output, "\\slide{Cómo leer las tablas}\n\\small\n");

    // Regla de la columna que entra segun el tipo de objetivo
    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "\\chip{entering}{Columna que entra} El valor \\textbf{más positivo} de la fila "
                                "$Z$ (segundo algoritmo visto en clase para minimizar). La tabla es óptima "
                                "cuando todos son $\\leq 0$.\\\\[1.5mm]\n");
    } else {
        g_string_append(output, "\\chip{entering}{Columna que entra} El valor \\textbf{más negativo} de la fila "
                                "$Z$. La tabla es óptima cuando todos son $\\geq 0$.\\\\[1.5mm]\n");
    }
    g_string_append(output,
                    "\\chip{leaving}{Fila que sale} La de \\textbf{fracción mínima} LD $\\div$ coeficiente, "
                    "contando solo los coeficientes positivos de la columna que entra.\\\\[1.5mm]\n"
                    "\\chip{pivot}{Pivote} Su fila se divide entre el pivote y el resto de la columna queda en 0."
                    "\\\\[2.5mm]\n"
                    "\\callout{tecblue}{Desempates}{Si dos columnas empatan entra la de menor índice. Si dos "
                    "filas empatan en la fracción mínima sale la variable básica de menor índice (primero las "
                    "variables originales y después las holguras $s_i$). Todos los números son fracciones "
                    "exactas.}\n");
}

// =============================================
// Problem and tables
// ---------------------------------------------

// Agrega una fila del modelo alineada por variable: un signo y un termino por columna
static void append_model_row(GString *output, const Problem *problem, const Fraction coefficients[]) {
    int is_first = 1;
    int is_all_zero = 1;

    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (fraction_sign(coefficients[variable]) != 0) {
            is_all_zero = 0;
        }
    }

    for (int variable = 0; variable < problem->variable_count; variable++) {
        Fraction coefficient = coefficients[variable];
        int sign = fraction_sign(coefficient);

        // Termino en 0: signo y termino vacios (si toda la fila es 0, la primera columna muestra el 0)
        if (sign == 0) {
            if (is_all_zero && variable == 0) {
                g_string_append(output, " & & $0$");
            } else {
                g_string_append(output, " & & ");
            }
            continue;
        }

        // Signo: el primer termino de la fila solo lo lleva si es negativo
        g_string_append(output, " & ");
        if (sign < 0) {
            g_string_append(output, "$-$");
        } else if (!is_first) {
            g_string_append(output, "$+$");
        }

        // Coeficiente sin signo (el 1 no se escribe) y nombre de la variable
        g_string_append(output, " & ");
        Fraction magnitude = coefficient;
        if (sign < 0) {
            magnitude = fraction_negate(coefficient);
        }
        if (magnitude.numerator != 1 || magnitude.denominator != 1) {
            g_string_append(output, "$");
            append_fraction(output, magnitude);
            g_string_append(output, "\\,$");
        }
        g_string_append(output, problem->variable_names[variable]);
        is_first = 0;
    }
}

// Agrega el problema original en notacion matematica, alineado por variable
static void append_problem(GString *output, const Problem *problem) {
    int term_columns = 2 * problem->variable_count;

    g_string_append(output, "\\slide{Problema original}\n\\begin{lrbox}{\\fitbox}\\large\n"
                            "\\tikz\\node[fill=zrow,draw=tecblue!40,rounded corners=4pt,inner xsep=12pt,inner ysep=9pt]{"
                            "\\begin{tabular}{l@{\\quad}r");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append(output, "@{\\,}c@{\\,}r");
    }
    g_string_append(output, "@{\\;}c@{\\;}l}\n");

    // Funcion objetivo
    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "{\\color{tecblue}\\bfseries Minimizar} & $Z =$");
    } else {
        g_string_append(output, "{\\color{tecblue}\\bfseries Maximizar} & $Z =$");
    }
    append_model_row(output, problem, problem->objective);
    g_string_append(output, " & & \\\\[2mm]\n");

    // Restricciones (en esta version todas son <=)
    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (constraint == 0) {
            g_string_append(output, "{\\color{tecblue}\\bfseries sujeto a} &");
        } else {
            g_string_append(output, " &");
        }
        append_model_row(output, problem, problem->coefficients[constraint]);
        g_string_append(output, " & $\\leq$ & $");
        append_fraction(output, problem->right_hand_sides[constraint]);
        g_string_append(output, "$ \\\\\n");
    }

    // No negatividad
    g_string_append_printf(output, " & & \\multicolumn{%d}{l}{", term_columns + 2);
    append_variable_names(output, problem);
    g_string_append(output, " $\\geq 0$} \\\\\n\\end{tabular}};\n\\end{lrbox}\n");
    g_string_append(output, "\\vspace*{\\fill}\n\\showfitted{0.8\\textheight}\n\\vspace*{\\fill}\n");
}

// Agrega una tabla: encabezado azul, fila Z gris, columna que entra con flecha, fila que sale con
// flecha, pivote en una cajita y la columna de fracciones con su calculo
static void append_table(GString *output, const Problem *problem, const Step *step, const char *max_height) {
    const Table *table = &step->table;
    int entering_column = step->entering_column;
    int show_ratios = (entering_column != -1);
    int right_hand_side_column = table->column_count - 1;
    int leaving_row = -1;
    if (step->leaving_constraint != -1) {
        leaving_row = step->leaving_constraint + 1;
    }
    int tabular_columns = table->column_count + 1;
    if (show_ratios) {
        tabular_columns++;
    }

    // Formato: Base | Z | originales | holguras | LD | Fraccion
    // Las tablas anchas usan menos relleno entre columnas para que quepan con letra mas grande
    g_string_append(output, "\\begin{lrbox}{\\fitbox}\\large\n");
    if (table->column_count > WIDE_TABLE_COLUMNS) {
        g_string_append(output, "\\setlength{\\tabcolsep}{2pt}\n");
    }
    if (table->row_count > TALL_TABLE_ROWS) {
        g_string_append(output, "\\renewcommand{\\arraystretch}{1.1}\n");
    }
    g_string_append(output, "\\begin{tabular}{c|c|");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append(output, "c");
    }
    g_string_append(output, "|");
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        g_string_append(output, "c");
    }
    g_string_append(output, "|c");
    if (show_ratios) {
        g_string_append(output, "|c");
    }
    g_string_append(output, "}\n");

    // Flecha sobre la columna que entra (renglon sin bordes ni color)
    if (show_ratios) {
        for (int index = 0; index < tabular_columns; index++) {
            if (index > 0) {
                g_string_append(output, " & ");
            }
            if (index == entering_column + 1) {
                g_string_append(output, "\\multicolumn{1}{c}{\\enterarrow}");
            } else {
                g_string_append(output, "\\multicolumn{1}{c}{}");
            }
        }
        g_string_append(output, " \\\\[-2pt]\n");
    }

    // Encabezado azul; la columna que entra va en rojo
    g_string_append(output, "\\rowcolor{tecblue}\\color{white}\\bfseries Base");
    for (int column = 0; column < table->column_count; column++) {
        g_string_append(output, " & ");
        if (column == entering_column) {
            g_string_append(output, "\\cellcolor{tecred}");
        }
        g_string_append(output, "\\color{white}\\bfseries\\boldmath ");
        append_column_label(output, problem, table, column);
    }
    if (show_ratios) {
        g_string_append(output, " & \\color{white}\\bfseries Fracción");
    }
    g_string_append(output, " \\\\\n");

    // Filas: la objetivo se llama Z y las demas llevan su variable basica
    for (int row = 0; row < table->row_count; row++) {
        if (row == 0) {
            g_string_append(output, "\\rowcolor{zrow}$Z$");
        } else {
            if (row == leaving_row) {
                g_string_append(output, "\\rowcolor{leaving}");
            }
            append_column_label(output, problem, table, table->basis[row - 1]);
        }

        // Celdas, con el pivote en una cajita y la columna que entra marcada
        for (int column = 0; column < table->column_count; column++) {
            g_string_append(output, " & ");
            if (column == entering_column) {
                g_string_append(output, "\\cellcolor{entering}");
            }
            if (row == leaving_row && column == entering_column) {
                g_string_append(output, "\\pivotcell{$");
                append_fraction(output, table->cells[row][column]);
                g_string_append(output, "$}");
            } else {
                g_string_append(output, "$");
                append_fraction(output, table->cells[row][column]);
                g_string_append(output, "$");
            }
        }

        // Fraccion de la fila con su calculo; la fila que sale lleva una flecha
        if (show_ratios) {
            g_string_append(output, " & ");
            if (row > 0 && step->ratio_is_valid[row - 1]) {
                g_string_append(output, "$");
                append_fraction(output, table->cells[row][right_hand_side_column]);
                g_string_append(output, " \\div ");
                append_fraction(output, table->cells[row][entering_column]);
                g_string_append(output, " = ");
                append_fraction(output, step->ratios[row - 1]);
                g_string_append(output, "$");
                if (row == leaving_row) {
                    g_string_append(output, "\\ \\leavearrow");
                }
            } else if (row > 0) {
                g_string_append(output, "{\\color{textgray}--}");
            }
        }
        g_string_append(output, " \\\\\n");
        if (row == 0) {
            g_string_append(output, "\\hline\n");
        }
    }
    g_string_append_printf(output, "\\hline\n\\end{tabular}\n\\end{lrbox}\n\\showfitted{%s}\n", max_height);
}

// Agrega las variables basicas que valen 0 en una tabla (solucion basica degenerada)
static void append_zero_basics(GString *output, const Problem *problem, const Table *table) {
    int right_hand_side_column = table->column_count - 1;
    int is_first = 1;

    g_string_append(output, "{\\footnotesize{\\color{warnorange}\\bfseries Solución básica degenerada:} ");
    for (int row = 1; row < table->row_count; row++) {
        if (fraction_sign(table->cells[row][right_hand_side_column]) != 0) {
            continue;
        }
        if (!is_first) {
            g_string_append(output, ", ");
        }
        append_column_label(output, problem, table, table->basis[row - 1]);
        is_first = 0;
    }

    // Concordancia: una variable "esta" y "vale"; varias "estan" y "valen"
    if (simplex_count_zero_basics(table) > 1) {
        g_string_append(output, " están en la base pero valen 0.\\par}\n");
    } else {
        g_string_append(output, " está en la base pero vale 0.\\par}\n");
    }
}

// Agrega las decisiones de una tabla: etiquetas de entra, sale y pivote, y la explicacion de cada una
static void append_decisions(GString *output, const Problem *problem, const Step *step, const char *reason,
                             int show_no_leaving) {
    const Table *table = &step->table;

    // Etiquetas de color iguales a las marcas de la tabla
    g_string_append(output, "\\chip{entering}{Entra \\textbf{");
    append_column_label(output, problem, table, step->entering_column);
    g_string_append(output, "}}");
    if (step->leaving_constraint != -1) {
        g_string_append(output, "\\enspace\\chip{leaving}{Sale \\textbf{");
        append_column_label(output, problem, table, table->basis[step->leaving_constraint]);
        g_string_append(output, "}}\\enspace\\chip{pivot}{Pivote $");
        append_fraction(output, step->pivot);
        g_string_append(output, "$}");
    }
    g_string_append(output, "\\par\n{\\footnotesize\\color{textgray}");

    // Por que entra y por que sale
    append_column_label(output, problem, table, step->entering_column);
    g_string_append_printf(output, " tiene el %s.", reason);
    if (step->leaving_constraint != -1) {
        g_string_append(output, " ");
        append_column_label(output, problem, table, table->basis[step->leaving_constraint]);
        g_string_append_printf(output, " sale porque la restricción %d tiene la fracción mínima ($",
                               step->leaving_constraint + 1);
        append_fraction(output, step->ratios[step->leaving_constraint]);
        g_string_append(output, "$).");
        if (step->tied_row_count > 1) {
            g_string_append_printf(output, " {\\color{tecred}Empate entre %d filas:} sale la variable básica de "
                                           "menor índice.",
                                   step->tied_row_count);
        }
        if (fraction_sign(step->ratios[step->leaving_constraint]) == 0) {
            g_string_append(output, " {\\color{warnorange}Pivoteo degenerado:} la fracción mínima es 0 y $Z$ no "
                                    "mejora.");
        }
    } else if (show_no_leaving) {
        g_string_append(output, " {\\color{tecred}Ninguna fila puede salir:} la columna no tiene coeficientes "
                                "positivos.");
    }
    g_string_append(output, "\\par}\n");
}

// Agrega un paso del historial en su propia pagina
static void append_step(GString *output, const Problem *problem, const SimplexResult *result, int step_index,
                        const char *title, const char *note) {
    const Step *step = &result->steps[step_index];
    int is_last_step = (step_index == result->step_count - 1);

    g_string_append_printf(output, "\\slide{%s}\n", title);
    if (step->table.row_count > TALL_TABLE_ROWS) {
        append_table(output, problem, step, TABLE_HEIGHT_TALL);
    } else {
        append_table(output, problem, step, TABLE_HEIGHT_WITH_DECISIONS);
    }
    g_string_append(output, "\\vspace{1mm}\n");
    if (step->zero_basic_count > 0) {
        append_zero_basics(output, problem, &step->table);
    }

    // Decisiones, o la condicion de optimo si ninguna columna entra
    if (step->entering_column == -1) {
        g_string_append(output, "\\chip{okgreen!20}{\\color{okgreen}\\bfseries Tabla óptima}\\par\n"
                                "{\\footnotesize\\color{textgray}");
        if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
            g_string_append(output, "Ningún valor de la fila $Z$ es positivo, así que $Z$ ya no puede bajar.");
        } else {
            g_string_append(output, "Ningún valor de la fila $Z$ es negativo, así que $Z$ ya no puede subir.");
        }
        g_string_append(output, "\\par}\n");
    } else if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
        append_decisions(output, problem, step, "valor más positivo de la fila $Z$",
                         is_last_step && result->status == SIMPLEX_UNBOUNDED);
    } else {
        append_decisions(output, problem, step, "valor más negativo de la fila $Z$",
                         is_last_step && result->status == SIMPLEX_UNBOUNDED);
    }

    if (note != NULL) {
        g_string_append_printf(output, "{\\footnotesize\\color{textgray}\\emph{%s}\\par}\n", note);
    }
}

// Agrega las tablas del historial: inicial, intermedias (si se pidieron) y final
static void append_history(GString *output, const Problem *problem, const SimplexResult *result,
                           int show_intermediate_tables) {
    char title[MESSAGE_BUFFER_SIZE];
    char note[MESSAGE_BUFFER_SIZE];
    int last_index = result->step_count - 1;
    int hidden_count = 0;

    for (int step_index = 0; step_index <= last_index; step_index++) {
        int is_intermediate = (step_index > 0 && step_index < last_index);

        // Las intermedias solo aparecen si el usuario las pidio
        if (is_intermediate && !show_intermediate_tables) {
            hidden_count++;
            continue;
        }

        // Titulo segun la posicion de la tabla
        if (step_index == 0 && step_index == last_index) {
            snprintf(title, sizeof(title), "Tabla inicial y final");
        } else if (step_index == 0) {
            snprintf(title, sizeof(title), "Tabla inicial");
        } else if (step_index == last_index && result->status == SIMPLEX_OPTIMAL) {
            snprintf(title, sizeof(title), "Tabla final \\textcolor{textgray}{\\normalsize(tabla %d)}", step_index);
        } else if (step_index == last_index) {
            snprintf(title, sizeof(title), "Última tabla \\textcolor{textgray}{\\normalsize(tabla %d)}", step_index);
        } else {
            snprintf(title, sizeof(title), "Tabla intermedia %d", step_index);
        }

        // La ultima tabla avisa cuantas intermedias se omitieron
        if (step_index == last_index && hidden_count > 0) {
            if (hidden_count == 1) {
                snprintf(note, sizeof(note), "Se omitió 1 tabla intermedia; se puede pedir desde la ventana.");
            } else {
                snprintf(note, sizeof(note), "Se omitieron %d tablas intermedias; se pueden pedir desde la ventana.",
                         hidden_count);
            }
            append_step(output, problem, result, step_index, title, note);
        } else {
            append_step(output, problem, result, step_index, title, NULL);
        }
    }
}

// =============================================
// Solution and special cases
// ---------------------------------------------

// Agrega un recuadro azul con el valor optimo de Z
static void append_objective_badge(GString *output, Fraction objective_value) {
    g_string_append(output, "\\begin{center}\\tikz\\node[fill=tecblue,text=white,rounded corners=4pt,"
                            "inner xsep=12pt,inner ysep=7pt]{\\Large $Z^{*} = ");
    append_fraction(output, objective_value);
    g_string_append(output, "$};\\end{center}\n");
}

// Agrega los valores de una solucion en dos columnas: variables de decision y holguras
static void append_values(GString *output, const Problem *problem, const Fraction variable_values[],
                          const Fraction slack_values[], const char *max_height) {
    char slack_name[NAME_BUFFER_SIZE];

    g_string_append(output, "\\begin{lrbox}{\\fitbox}\\large\n\\begin{tabular}{r@{\\;}c@{\\;}l}\n"
                            "\\multicolumn{3}{l}{\\color{tecblue}\\bfseries Variables de decisión} \\\\\n");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append_printf(output, "%s & $=$ & $", problem->variable_names[variable]);
        append_fraction(output, variable_values[variable]);
        g_string_append(output, "$ \\\\\n");
    }
    g_string_append(output, "\\end{tabular}\\hspace{14mm}\n\\begin{tabular}{r@{\\;}c@{\\;}l}\n"
                            "\\multicolumn{3}{l}{\\color{tecblue}\\bfseries Holguras} \\\\\n");
    for (int slack = 0; slack < problem->constraint_count; slack++) {
        problem_slack_name(slack, slack_name, NAME_BUFFER_SIZE);
        g_string_append_printf(output, "%s & $=$ & $", slack_name);
        append_fraction(output, slack_values[slack]);
        g_string_append(output, "$ \\\\\n");
    }
    g_string_append_printf(output, "\\end{tabular}\n\\end{lrbox}\n\\showfitted{%s}\n", max_height);
}

// Agrega la solucion final o la explicacion de por que no hay optimo
static void append_solution(GString *output, const Problem *problem, const SimplexResult *result) {
    const Step *last_step = &result->steps[result->step_count - 1];

    g_string_append(output, "\\slide{Solución}\n");
    if (result->status == SIMPLEX_OPTIMAL) {
        append_objective_badge(output, result->objective_value);
        append_values(output, problem, result->variable_values, result->slack_values, VALUES_HEIGHT);
        if (result->multiple_kind != MULTIPLE_NONE) {
            g_string_append(output, "\\vfill{\\small\\color{okgreen}\\bfseries Esta no es la única solución "
                                    "óptima: ver las páginas siguientes.\\par}\n");
        }
        return;
    }

    // Sin optimo: no acotado, limite de iteraciones o error de desbordamiento
    if (result->status == SIMPLEX_UNBOUNDED) {
        g_string_append(output, "\\callout{tecred}{Problema no acotado}{En la última tabla entra ");
        append_column_label(output, problem, &last_step->table, last_step->entering_column);
        g_string_append(output, ", pero su columna no tiene coeficientes positivos. Ninguna restricción limita su "
                                "crecimiento: esa variable puede aumentar sin fin y $Z$ mejora sin límite, así "
                                "que el problema no tiene una solución óptima finita.}\n");
        return;
    }
    if (result->status == SIMPLEX_ITERATION_LIMIT) {
        g_string_append_printf(output, "\\callout{tecred}{Límite de iteraciones}{Se alcanzó el límite de %d "
                                       "pivoteos sin llegar a una tabla óptima.}\n",
                               MAX_ITERATIONS);
        return;
    }
    g_string_append_printf(output, "\\callout{tecred}{Sin solución}{%s}\n", simplex_status_message(result->status));
}

// Agrega un vector columna con los valores de las variables originales
static void append_column_vector(GString *output, const Fraction values[], int variable_count) {
    g_string_append(output, "\\begin{pmatrix}");
    for (int variable = 0; variable < variable_count; variable++) {
        if (variable > 0) {
            g_string_append(output, " \\\\ ");
        }
        append_fraction(output, values[variable]);
    }
    g_string_append(output, "\\end{pmatrix}");
}

// Agrega la tabla de soluciones adicionales con el valor del parametro que genera cada una
static void append_extra_points(GString *output, const Problem *problem, const SimplexResult *result,
                                const char *parameter_symbol, const long long parameters[][2]) {
    Fraction parameter;
    Fraction values[MAX_VARIABLES];

    // Encabezado azul: parametro y variables originales
    g_string_append(output, "\\begin{lrbox}{\\fitbox}\\large\n\\begin{tabular}{c|");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append(output, "c");
    }
    g_string_append_printf(output, "}\n\\rowcolor{tecblue}\\color{white}$%s$", parameter_symbol);
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append_printf(output, " & \\color{white}%s", problem->variable_names[variable]);
    }
    g_string_append(output, " \\\\\n");

    // Un renglon por punto; un punto que no se puede calcular se explica en su renglon
    for (int index = 0; index < EXTRA_POINT_COUNT; index++) {
        FractionStatus fraction_status = fraction_make(parameters[index][0], parameters[index][1], &parameter);
        if (fraction_status != FRACTION_OK) {
            g_string_append_printf(output, "\\multicolumn{%d}{c}{%s} \\\\\n", problem->variable_count + 1,
                                   fraction_status_message(fraction_status));
            continue;
        }
        g_string_append(output, "$");
        append_fraction(output, parameter);
        g_string_append(output, "$");
        if (simplex_alternative_point(result, problem->variable_count, parameter, values) != SIMPLEX_RUNNING) {
            g_string_append_printf(output, " & \\multicolumn{%d}{c}{El punto no se puede representar de forma "
                                           "exacta} \\\\\n",
                                   problem->variable_count);
            continue;
        }
        for (int variable = 0; variable < problem->variable_count; variable++) {
            g_string_append(output, " & $");
            append_fraction(output, values[variable]);
            g_string_append(output, "$");
        }
        g_string_append(output, " \\\\\n");
    }
    g_string_append_printf(output, "\\hline\n\\end{tabular}\n\\end{lrbox}\n\\showfitted{%s}\n", POINTS_HEIGHT);
}

// Agrega las soluciones multiples: pivoteo extra, segunda tabla final, ecuacion y puntos adicionales
static void append_multiple_solutions(GString *output, const Problem *problem, const SimplexResult *result) {
    const Step *step = &result->alternative_step;
    const long long segment_parameters[EXTRA_POINT_COUNT][2] = {{1, 4}, {1, 2}, {3, 4}};
    const long long ray_parameters[EXTRA_POINT_COUNT][2] = {{1, 1}, {2, 1}, {3, 1}};

    // Pivoteo extra sobre la tabla final
    g_string_append(output, "\\slide{Soluciones múltiples: un pivoteo más}\n{\\small ");
    append_column_label(output, problem, &step->table, step->entering_column);
    g_string_append(output, " no es básica y tiene \\textbf{0 en la fila $Z$}: puede entrar a la base sin "
                            "cambiar $Z$, así que hay más de una solución óptima.\\par}\n");
    append_table(output, problem, step, TABLE_HEIGHT_WITH_TEXT);
    g_string_append(output, "\\vspace{1mm}\n");
    append_decisions(output, problem, step, "0 en la fila $Z$", 1);

    // Segmento: segunda tabla final, segunda solucion y la combinacion de ambas
    if (result->multiple_kind == MULTIPLE_SEGMENT) {
        Step second_step = *step;
        second_step.table = result->alternative_table;
        second_step.entering_column = -1;
        second_step.leaving_constraint = -1;

        g_string_append(output, "\\slide{Segunda tabla final}\n");
        append_table(output, problem, &second_step, TABLE_HEIGHT_WITH_TEXT);
        g_string_append(output, "\\vspace{1mm}\\chip{okgreen!20}{\\color{okgreen}\\bfseries Segunda solución "
                                "óptima}\\enspace{\\small con el mismo $Z^{*} = ");
        append_fraction(output, result->objective_value);
        g_string_append(output, "$}\\par\n");

        g_string_append(output, "\\slide{Segunda solución}\n");
        append_objective_badge(output, result->objective_value);
        append_values(output, problem, result->alternative_variable_values, result->alternative_slack_values,
                      VALUES_HEIGHT);

        g_string_append(output, "\\slide{Todas las soluciones óptimas}\n{\\small Cualquier punto del segmento "
                                "entre las dos soluciones también es óptimo (vectores con ");
        append_variable_names(output, problem);
        g_string_append(output, "):\\par}\n\\begin{lrbox}{\\fitbox}$\\displaystyle \\alpha ");
        append_column_vector(output, result->variable_values, problem->variable_count);
        g_string_append(output, " + (1 - \\alpha) ");
        append_column_vector(output, result->alternative_variable_values, problem->variable_count);
        g_string_append(output, ", \\qquad 0 \\leq \\alpha \\leq 1$\\end{lrbox}\n\\showfitted{0.3\\textheight}\n"
                                "{\\small\\color{tecblue}\\bfseries Algunas soluciones adicionales\\par}\n");
        append_extra_points(output, problem, result, "\\alpha", segment_parameters);
        return;
    }

    // Rayo: no hay segunda tabla, la variable puede crecer sin cambiar Z
    g_string_append(output, "\\slide{Todas las soluciones óptimas}\n{\\small Como ninguna fila puede salir, ");
    append_column_label(output, problem, &step->table, step->entering_column);
    g_string_append(output, " puede crecer sin límite sin que $Z$ cambie. Las soluciones óptimas forman un rayo "
                            "(vectores con ");
    append_variable_names(output, problem);
    g_string_append(output, "):\\par}\n\\begin{lrbox}{\\fitbox}$\\displaystyle ");
    append_column_vector(output, result->variable_values, problem->variable_count);
    g_string_append(output, " + t ");
    append_column_vector(output, result->ray_direction, problem->variable_count);
    g_string_append(output, ", \\qquad t \\geq 0$\\end{lrbox}\n\\showfitted{0.3\\textheight}\n"
                            "{\\small\\color{tecblue}\\bfseries Algunas soluciones adicionales\\par}\n");
    append_extra_points(output, problem, result, "t", ray_parameters);
}

// Agrega la explicacion del problema degenerado: donde hubo variables basicas en 0 y pivoteos sin mejora
static void append_degenerate(GString *output, const SimplexResult *result) {
    int degenerate_tables[MAX_STEPS] = {0};
    int degenerate_pivots[MAX_STEPS] = {0};
    int table_count = 0;
    int pivot_count = 0;

    // Tablas con variables basicas en 0 y pivoteos con fraccion minima 0
    for (int step_index = 0; step_index < result->step_count; step_index++) {
        const Step *step = &result->steps[step_index];
        if (step->zero_basic_count > 0) {
            degenerate_tables[table_count] = step_index;
            table_count++;
        }
        if (step->leaving_constraint != -1 && fraction_sign(step->ratios[step->leaving_constraint]) == 0) {
            degenerate_pivots[pivot_count] = step_index;
            pivot_count++;
        }
    }

    g_string_append(output, "\\slide{Problema degenerado}\n"
                            "\\callout{warnorange}{¿Qué pasó?}{Al menos una tabla tiene una variable básica que vale "
                            "0, así que el problema es \\textbf{degenerado}. En esos casos se puede pivotear sin "
                            "que $Z$ mejore, y existe el riesgo de \\textbf{ciclar}: repetir tablas sin avanzar.}\n"
                            "\\vspace{1.5mm}{\\small\n");
    g_string_append(output, "\\textbf{Tablas con variables básicas en 0} (la inicial es la 0): ");
    append_step_list(output, degenerate_tables, table_count);
    g_string_append(output, ".\\par\n");
    if (pivot_count > 0) {
        g_string_append(output, "\\textbf{Pivoteos donde $Z$ no cambió}, hechos sobre las tablas ");
        append_step_list(output, degenerate_pivots, pivot_count);
        g_string_append(output, ".\\par\n");
    }
    g_string_append(output, "\\textbf{Desempate:} con filas empatadas en la fracción mínima salió la variable "
                            "básica de menor índice.\\par\n");
    if (result->status == SIMPLEX_ITERATION_LIMIT) {
        g_string_append_printf(output, "{\\color{tecred}\\bfseries El Simplex cicló:} repitió pivoteos sin mejorar "
                                       "$Z$ y se detuvo después de %d pivoteos.\\par\n",
                               MAX_ITERATIONS);
    } else {
        g_string_append(output, "{\\color{okgreen}\\bfseries Aun así, el Simplex logró salir} de las tablas "
                                "degeneradas y terminar.\\par\n");
    }
    g_string_append(output, "}\n");
}

// =============================================
// Public functions
// ---------------------------------------------

// Arma el documento completo y lo guarda en path
int latex_report_write(const char *path, const Problem *problem, const SimplexResult *result,
                       int show_intermediate_tables, char *message, size_t message_size) {
    GString *output = g_string_new(NULL);
    GError *error = NULL;

    // Partes del documento en el orden del enunciado
    append_preamble(output, problem);
    append_cover(output, problem);
    append_simplex_history(output);
    append_simplex_properties(output);
    append_reading_guide(output, problem);
    append_problem(output, problem);
    append_history(output, problem, result, show_intermediate_tables);
    append_solution(output, problem, result);
    if (result->status == SIMPLEX_OPTIMAL && result->multiple_kind != MULTIPLE_NONE) {
        append_multiple_solutions(output, problem, result);
    }
    if (result->is_degenerate) {
        append_degenerate(output, result);
    }
    g_string_append(output, "\\end{document}\n");

    // Guardar el archivo; cualquier fallo se describe en message
    int is_written = g_file_set_contents(path, output->str, (gssize)output->len, &error);
    if (!is_written) {
        snprintf(message, message_size, "No se pudo escribir el archivo LaTeX: %s", error->message);
        g_error_free(error);
    }
    g_string_free(output, TRUE);
    return is_written;
}
