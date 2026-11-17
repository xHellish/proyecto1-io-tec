#include "latex_report.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "latex_text.h"
#include "plots.h"

// =============================================
// Constants
// ---------------------------------------------
#define PROJECT_TITLE "Proyecto 2: Otro SIMPLEX más"
#define PROJECT_PART "Parte 2"
#define INSTITUTION_NAME "Instituto Tecnológico de Costa Rica"
#define SCHOOL_NAME "Escuela de Ingeniería en Computación"
#define COURSE_NAME "Investigación de Operaciones"
#define SEMESTER_NAME "Segundo Semestre, 2026"
#define PROFESSOR_NAME "Francisco Torres Rojas"

static const char *STUDENT_NAMES[] = {
    "Ben Farzamipour Alfaro",
    "José Johel Jara Álvarez",
    "Anthony Camacho Palma",
};

#define LOGO_PATH "../assets/tec_logo.png"

#define WIDE_TABLE_COLUMNS 11
#define EXTRA_POINT_COUNT 3

// =============================================
// Text helpers
// ---------------------------------------------

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

// Escribe el nombre de una columna de la tabla (variable original, holgura, exceso o artificial)
static void append_column_label(GString *output, const Problem *problem, const Table *table, int column) {
    char name[NAME_BUFFER_SIZE];
    ColumnType type = table->column_types[column];
    int owner = table->column_owner[column];

    if (type == COLUMN_Z) {
        g_string_append(output, "$Z$");
        return;
    }
    if (type == COLUMN_ORIGINAL) {
        g_string_append(output, problem->variable_names[owner]);
        return;
    }
    if (type == COLUMN_SLACK) {
        problem_slack_name(owner, name, NAME_BUFFER_SIZE);
        g_string_append(output, name);
        return;
    }
    if (type == COLUMN_SURPLUS) {
        problem_surplus_name(owner, name, NAME_BUFFER_SIZE);
        g_string_append(output, name);
        return;
    }
    if (type == COLUMN_ARTIFICIAL) {
        problem_artificial_name(owner, name, NAME_BUFFER_SIZE);
        g_string_append(output, name);
        return;
    }
    g_string_append(output, "LD");
}

// Escribe un valor de la fila objetivo como bM + a (la parte de M primero, como en clase)
static void append_objective_value(GString *output, const Table *table, int column) {
    Fraction constant = table->cells[0][column];
    Fraction m_part = table->m_cells[column];

    // Sin M es un numero normal
    if (fraction_sign(m_part) == 0) {
        latex_append_fraction(output, constant);
        return;
    }

    // Parte de M: M, -M o fraccion por M
    if (m_part.numerator == 1 && m_part.denominator == 1) {
        g_string_append(output, "M");
    } else if (m_part.numerator == -1 && m_part.denominator == 1) {
        g_string_append(output, "-M");
    } else {
        latex_append_fraction(output, m_part);
        g_string_append(output, "M");
    }

    // Constante con su signo; si es 0 no se escribe
    if (fraction_sign(constant) > 0) {
        g_string_append(output, " + ");
        latex_append_fraction(output, constant);
    } else if (fraction_sign(constant) < 0) {
        g_string_append(output, " - ");
        latex_append_fraction(output, fraction_negate(constant));
    }
}

// Grupo de una columna para separar con lineas verticales: originales, holguras y excesos, artificiales
static int column_group(ColumnType type) {
    if (type == COLUMN_ORIGINAL) {
        return 0;
    }
    if (type == COLUMN_SLACK || type == COLUMN_SURPLUS) {
        return 1;
    }
    if (type == COLUMN_ARTIFICIAL) {
        return 2;
    }
    return 3;
}

static void append_variable_names(GString *output, const Problem *problem) {
    for (int variable = 0; variable < problem->variable_count; variable++) {
        if (variable > 0) {
            g_string_append(output, ", ");
        }
        g_string_append(output, problem->variable_names[variable]);
    }
}

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

static void append_preamble(GString *output, const Problem *problem) {
    g_string_append(output,
                    "\\documentclass[11pt,letterpaper]{article}\n"
                    "\\usepackage[T1]{fontenc}\n"
                    "\\usepackage[utf8]{inputenc}\n"
                    "\\usepackage{lmodern}\n"
                    "\\usepackage[spanish,es-noshorthands,es-nodecimaldot]{babel}\n"
                    "\\usepackage[letterpaper,left=2.2cm,right=2.2cm,top=2.4cm,bottom=2.4cm,"
                    "headheight=14pt]{geometry}\n"
                    "\\usepackage{amsmath,amssymb,graphicx,array,booktabs,float,fancyhdr,titlesec}\n"
                    "\\usepackage[table]{xcolor}\n"
                    "\\usepackage[most]{tcolorbox}\n"
                    "\\usepackage{tikz}\n"
                    "\\usetikzlibrary{arrows.meta,shadows,shapes.geometric}\n"
                    "\n"
                    "% Paleta de colores institucional y sobria\n"
                    "\\definecolor{tecblue}{RGB}{0,41,84}\n"
                    "\\definecolor{tecred}{RGB}{200,16,46}\n"
                    "\\definecolor{slatebg}{RGB}{246,248,251}\n"
                    "\\definecolor{textgray}{RGB}{75,85,99}\n"
                    "\\definecolor{zrow}{RGB}{236,241,247}\n"
                    "\\definecolor{entering}{RGB}{214,232,248}\n"
                    "\\definecolor{leaving}{RGB}{253,230,216}\n"
                    "\\definecolor{pivot}{RGB}{255,214,102}\n"
                    "\\definecolor{okgreen}{RGB}{21,115,71}\n"
                    "\\definecolor{warnorange}{RGB}{194,94,0}\n"
                    "\\definecolor{plotc1}{RGB}{0,114,178}\n"
                    "\\definecolor{plotc2}{RGB}{230,159,0}\n"
                    "\\definecolor{plotc3}{RGB}{0,158,115}\n"
                    "\\definecolor{plotc4}{RGB}{204,121,167}\n"
                    "\\definecolor{plotc5}{RGB}{86,150,200}\n"
                    "\\definecolor{plotc6}{RGB}{213,94,0}\n"
                    "\\definecolor{plotc7}{RGB}{120,120,30}\n"
                    "\\definecolor{plotc8}{RGB}{90,90,90}\n"
                    "\n"
                    "% Tipografia y espaciado de documento formal\n"
                    "\\setlength{\\parindent}{0pt}\n"
                    "\\setlength{\\parskip}{6pt}\n"
                    "\\renewcommand{\\arraystretch}{1.35}\n"
                    "\\setlength{\\tabcolsep}{6pt}\n"
                    "\\arrayrulecolor{tecblue!35}\n"
                    "\n"
                    "% Estilo de secciones y subsecciones\n"
                    "\\titleformat{\\section}{\\Large\\bfseries\\color{tecblue}}{\\thesection.}{0.6em}{}"
                    "[\\vspace{2pt}{\\color{tecred}\\titlerule[1.2pt]}]\n"
                    "\\titlespacing*{\\section}{0pt}{16pt}{8pt}\n"
                    "\\titleformat{\\subsection}{\\large\\bfseries\\color{tecblue!90}}{\\thesubsection}{0.6em}{}\n"
                    "\\titlespacing*{\\subsection}{0pt}{12pt}{5pt}\n"
                    "\n"
                    "% Encabezado y pie de pagina\n"
                    "\\pagestyle{fancy}\n"
                    "\\fancyhf{}\n"
                    "\\fancyhead[L]{\\color{textgray}\\small\\textsc{" COURSE_NAME " --- " PROJECT_TITLE "}}\n"
                    "\\fancyhead[R]{\\IfFileExists{" LOGO_PATH "}{\\includegraphics[height=4.5mm]{" LOGO_PATH
                    "}}{\\color{tecblue}\\small\\bfseries TEC}}\n"
                    "\\fancyfoot[L]{\\color{textgray}\\small Problema: ");
    append_escaped(output, problem->name);
    g_string_append(output,
                    "}\n"
                    "\\fancyfoot[R]{\\color{textgray}\\small Página \\thepage}\n"
                    "\\renewcommand{\\headrulewidth}{0.4pt}\n"
                    "\\renewcommand{\\headrule}{\\hbox to\\headwidth{\\color{tecblue!30}\\leaders\\hrule height "
                    "\\headrulewidth\\hfill}}\n"
                    "\n"
                    "% Cajas esteticas para bloques de informacion y resultados\n"
                    "\\newtcolorbox{calloutbox}[2]{\n"
                    "  enhanced, breakable,\n"
                    "  colback=#1!5, colframe=#1!80,\n"
                    "  boxrule=0.6pt, leftrule=3pt, arc=2pt,\n"
                    "  top=6pt, bottom=6pt, left=9pt, right=9pt,\n"
                    "  fonttitle=\\bfseries\\color{#1}, title={#2},\n"
                    "  coltitle=#1, attach title to upper={\\\\[2pt]}\n"
                    "}\n"
                    "\n"
                    "\\newtcolorbox{modelbox}{\n"
                    "  enhanced,\n"
                    "  colback=slatebg, colframe=tecblue!35,\n"
                    "  boxrule=0.6pt, arc=3pt,\n"
                    "  top=8pt, bottom=8pt, left=10pt, right=10pt\n"
                    "}\n"
                    "\n"
                    "% Etiquetas y marcas en tablas\n"
                    "\\newcommand{\\chip}[2]{\\tikz[baseline=(c.base)]\\node[fill=#1,rounded corners=2pt,"
                    "inner xsep=5pt,inner ysep=2.5pt](c){\\strut #2};}\n"
                    "\\newcommand{\\pivotcell}[1]{\\tikz[baseline=(p.base)]\\node[draw=tecred,fill=pivot,"
                    "rounded corners=2pt,line width=0.9pt,inner sep=2.2pt](p){#1};}\n"
                    "\\newcommand{\\enterarrow}{\\tikz\\draw[-{Stealth[length=2.2mm]},line width=1.1pt,tecred]"
                    "(0,0.42)--(0,0);}\n"
                    "\\newcommand{\\leavearrow}{\\tikz[baseline=-0.6ex]\\draw[-{Stealth[length=2.2mm]},"
                    "line width=1.1pt,tecred](0.5,0)--(0,0);}\n"
                    "\n"
                    "% Ajuste automatico solo si la tabla o grafico supera el ancho de pagina\n"
                    "\\newsavebox{\\fitbox}\n"
                    "\\newcommand{\\showfitted}[1]{%\n"
                    "  \\ifdim\\wd\\fitbox>\\textwidth\n"
                    "    \\begin{center}\\resizebox{\\textwidth}{!}{\\usebox{\\fitbox}}\\end{center}\n"
                    "  \\else\n"
                    "    \\begin{center}\\usebox{\\fitbox}\\end{center}\n"
                    "  \\fi\n"
                    "}\n"
                    "\n"
                    "\\begin{document}\n");
}

static void append_cover(GString *output, const Problem *problem) {
    int student_count = (int)(sizeof(STUDENT_NAMES) / sizeof(STUDENT_NAMES[0]));

    g_string_append(output,
                    "\\begin{titlepage}\n"
                    "\\thispagestyle{empty}\n"
                    "\\centering\n"
                    "\\vspace*{0.5cm}\n"
                    "\\IfFileExists{" LOGO_PATH "}{\\includegraphics[height=1.8cm]{" LOGO_PATH "}\\par\\vspace{0.5cm}}{}\n"
                    "{\\Large\\bfseries\\color{tecblue} " INSTITUTION_NAME "\\par}\n"
                    "\\vspace{0.2cm}\n"
                    "{\\large\\color{textgray} " SCHOOL_NAME "\\par}\n"
                    "\\vspace{0.2cm}\n"
                    "{\\normalsize\\textbf{" COURSE_NAME "} \\textbullet{} " SEMESTER_NAME "\\par}\n"
                    "\n"
                    "\\vspace{2.2cm}\n"
                    "{\\color{tecred}\\rule{0.75\\textwidth}{1.5pt}\\par}\n"
                    "\\vspace{0.6cm}\n"
                    "{\\Huge\\bfseries\\color{tecblue} " PROJECT_TITLE "\\par}\n"
                    "\\vspace{0.3cm}\n"
                    "{\\Large\\color{textgray} " PROJECT_PART " --- Algoritmo Símplex y Método de la Gran M\\par}\n"
                    "\\vspace{0.6cm}\n"
                    "{\\color{tecred}\\rule{0.75\\textwidth}{1.5pt}\\par}\n"
                    "\n"
                    "\\vspace{1.8cm}\n"
                    "\\begin{tcolorbox}[width=0.85\\textwidth,colback=slatebg,colframe=tecblue!40,arc=3pt,boxrule=0.8pt]\n"
                    "\\centering\\large\n"
                    "{\\color{textgray}\\small PROBLEMA ANALIZADO}\\\\[3pt]\n"
                    "\\textbf{\\color{tecblue} ");
    append_escaped(output, problem->name);
    g_string_append(output,
                    "}\n"
                    "\\end{tcolorbox}\n"
                    "\n"
                    "\\vfill\n"
                    "\\begin{minipage}[t]{0.48\\textwidth}\n"
                    "\\raggedright\\normalsize\n"
                    "{\\bfseries\\color{tecblue} Integrantes del grupo:}\\\\[6pt]\n");
    for (int index = 0; index < student_count; index++) {
        g_string_append_printf(output, "\\textbullet\\ %s\\\\[3pt]\n", STUDENT_NAMES[index]);
    }
    g_string_append(output,
                    "\\end{minipage}\\hfill\n"
                    "\\begin{minipage}[t]{0.45\\textwidth}\n"
                    "\\raggedleft\\normalsize\n"
                    "{\\bfseries\\color{tecblue} Profesor:}\\\\[6pt]\n"
                    PROFESSOR_NAME "\\\\[12pt]\n"
                    "{\\bfseries\\color{tecblue} Fecha de generación:}\\\\[6pt]\n"
                    "\\today\n"
                    "\\end{minipage}\n"
                    "\\vspace*{1cm}\n"
                    "\\end{titlepage}\n"
                    "\\setcounter{page}{1}\n");
}

static void append_theory_section(GString *output) {
    g_string_append(output,
                    "\\section{Fundamentos Teóricos: Algoritmo Símplex y Gran M}\n"
                    "\n"
                    "\\subsection{Historia y contexto del Algoritmo Símplex}\n"
                    "En \\textbf{1947}, el matemático estadounidense \\textbf{George B. Dantzig} formuló el modelo "
                    "general de programación lineal y diseñó el \\textbf{método Símplex} mientras trabajaba en "
                    "problemas de planificación logística para la Fuerza Aérea de los Estados Unidos. Ese mismo año, "
                    "John von Neumann desarrolló la teoría de la dualidad, estableciendo el vínculo fundamental entre "
                    "todo problema lineal primal y su dual.\n\n"
                    "Previamente, en 1939, \\textbf{Leonid Kantoróvich} había aplicado modelos lineales a la "
                    "planificación industrial en la Unión Soviética, y \\textbf{Tjalling Koopmans} estudió problemas "
                    "clásicos de transporte; ambos fueron galardonados con el Premio Nobel de Economía en 1975 por sus "
                    "contribuciones a la asignación óptima de recursos.\n\n"
                    "El nombre \\emph{Símplex}, sugerido por Theodore Motzkin, proviene de la interpretación geométrica "
                    "del politopo de soluciones. Aunque Victor Klee y George Minty demostraron en 1972 que su complejidad "
                    "en el peor caso es exponencial ---lo que impulsó métodos de tiempo polinomial como el del elipsoide "
                    "(Khachiyan, 1979) y puntos interiores (Karmarkar, 1984)---, el algoritmo Símplex continúa siendo "
                    "uno de los diez algoritmos más influyentes de la ciencia y la ingeniería moderna por su extraordinaria "
                    "eficiencia práctica.\n"
                    "\n"
                    "\\subsection{Propiedades fundamentales}\n"
                    "\\begin{itemize}\\setlength{\\itemsep}{3pt}\n"
                    "  \\item \\textbf{Geometría convexa:} La región factible conforma un poliedro convexo; si existe "
                    "solución óptima finita, al menos un vértice (punto extremo) alcanza dicho valor óptimo.\n"
                    "  \\item \\textbf{Soluciones básicas factibles (SBF):} Cada tabla Símplex representa un vértice donde "
                    "las variables básicas toman el valor del lado derecho y las no básicas valen cero.\n"
                    "  \\item \\textbf{Mejora monótona:} Cada operación de pivoteo desplaza la solución hacia un vértice "
                    "adyacente sin empeorar el valor de la función objetivo $Z$.\n"
                    "  \\item \\textbf{Criterios de parada:} El proceso finaliza cuando ningún coeficiente en la fila $Z$ "
                    "mejora el objetivo (optimalidad), cuando la columna entrante carece de entradas positivas (problema "
                    "no acotado), o cuando una variable artificial permanece positiva en la base óptima (infactibilidad).\n"
                    "\\end{itemize}\n"
                    "\n"
                    "\\subsection{El Método de la Gran M}\n"
                    "El algoritmo Símplex estándar requiere una solución básica factible inicial evidente, proporcionada "
                    "naturalmente por las variables de holgura $s_i$ en restricciones de tipo $\\leq$ con lado derecho "
                    "$b_i \\geq 0$. Cuando el modelo incluye restricciones de tipo $\\geq$ o $=$, se recurre al "
                    "\\textbf{Método de la Gran M}:\n"
                    "\\begin{enumerate}\\setlength{\\itemsep}{3pt}\n"
                    "  \\item \\textbf{Formulación aumentada:} A cada restricción $\\geq$ se le resta una variable de exceso "
                    "$e_i$ y se le suma una variable artificial $a_i$; a cada restricción $=$ se le suma una variable "
                    "artificial $a_i$.\n"
                    "  \\item \\textbf{Penalización en el objetivo:} Dado que las variables artificiales carecen de sentido "
                    "físico en el modelo original, se penalizan en la función objetivo mediante un parámetro $M > 0$ "
                    "arbitrariamente grande (mayor que cualquier número real del problema): se resta $M a_i$ al maximizar "
                    "y se suma $M a_i$ al minimizar.\n"
                    "  \\item \\textbf{Canonización:} Antes de iniciar las iteraciones, se eliminan los términos en $M$ de "
                    "las columnas de las variables artificiales básicas en la fila $Z$, expresando cada entrada de dicha "
                    "fila en la forma simbólica $bM + a$.\n"
                    "  \\item \\textbf{Exclusión de artificiales:} Una vez que una variable artificial abandona la base, "
                    "nunca volverá a entrar; por ello, su columna se omite en todas las tablas posteriores.\n"
                    "\\end{enumerate}\n");
}

static void append_reading_guide(GString *output, const Problem *problem) {
    g_string_append(output, "\\subsection{Convenciones de lectura de las tablas}\n");

    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "Dado que el problema es de \\textbf{minimización}, se aplica el segundo algoritmo "
                                "visto en clase: entra a la base la variable con el coeficiente \\textbf{más positivo} "
                                "en la fila $Z$, y la tabla es óptima cuando todos los coeficientes de la fila $Z$ son "
                                "$\\leq 0$. ");
    } else {
        g_string_append(output, "Dado que el problema es de \\textbf{maximización}, entra a la base la variable con el "
                                "coeficiente \\textbf{más negativo} en la fila $Z$, y la tabla es óptima cuando todos "
                                "los coeficientes de la fila $Z$ son $\\geq 0$. ");
    }
    g_string_append(output,
                    "La fila saliente corresponde a la \\textbf{fracción mínima} no negativa ($\\text{LD} \\div "
                    "\\text{coeficiente}$) considerando únicamente coeficientes estrictamente positivos en la columna "
                    "entrante.\n\n"
                    "\\begin{calloutbox}{tecblue}{Reglas de desempate y precisión aritmética}\n"
                    "En caso de empate en la columna entrante, se selecciona la variable de menor índice. Si existe "
                    "empate en la fracción mínima para la fila saliente, abandona la base la variable básica con menor "
                    "índice de columna (priorizando variables originales, luego holguras/excesos y finalmente "
                    "artificiales). Todos los cálculos se realizan mediante aritmética exacta de fracciones.\n"
                    "\\end{calloutbox}\n");
}

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

        if (sign == 0) {
            if (is_all_zero && variable == 0) {
                g_string_append(output, " & & $0$");
            } else {
                g_string_append(output, " & & ");
            }
            continue;
        }

        g_string_append(output, " & ");
        if (sign < 0) {
            g_string_append(output, "$-$");
        } else if (!is_first) {
            g_string_append(output, "$+$");
        }

        g_string_append(output, " & ");
        Fraction magnitude = coefficient;
        if (sign < 0) {
            magnitude = fraction_negate(coefficient);
        }
        if (magnitude.numerator != 1 || magnitude.denominator != 1) {
            g_string_append(output, "$");
            latex_append_fraction(output, magnitude);
            g_string_append(output, "\\,$");
        }
        g_string_append(output, problem->variable_names[variable]);
        is_first = 0;
    }
}

static void append_problem(GString *output, const Problem *problem) {
    int term_columns = 2 * problem->variable_count;

    g_string_append(output, "\\section{Planteamiento del Problema Original}\n"
                            "A continuación se presenta la formulación matemática del modelo ingresado bajo el "
                            "identificador \\textbf{");
    append_escaped(output, problem->name);
    g_string_append(output, "}:\n\n"
                            "\\begin{modelbox}\n"
                            "\\begin{lrbox}{\\fitbox}\n"
                            "\\begin{tabular}{l@{\\quad}r");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append(output, "@{\\,}c@{\\,}r");
    }
    g_string_append(output, "@{\\;}c@{\\;}l}\n");

    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "{\\color{tecblue}\\bfseries Minimizar} & $Z =$");
    } else {
        g_string_append(output, "{\\color{tecblue}\\bfseries Maximizar} & $Z =$");
    }
    append_model_row(output, problem, problem->objective);
    g_string_append(output, " & & \\\\[6pt]\n");

    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (constraint == 0) {
            g_string_append(output, "{\\color{tecblue}\\bfseries Sujeto a:} &");
        } else {
            g_string_append(output, " &");
        }
        append_model_row(output, problem, problem->coefficients[constraint]);
        if (problem->constraint_types[constraint] == CONSTRAINT_GREATER_EQUAL) {
            g_string_append(output, " & $\\geq$ & $");
        } else if (problem->constraint_types[constraint] == CONSTRAINT_EQUAL) {
            g_string_append(output, " & $=$ & $");
        } else {
            g_string_append(output, " & $\\leq$ & $");
        }
        latex_append_fraction(output, problem->right_hand_sides[constraint]);
        g_string_append(output, "$ \\\\\n");
    }

    g_string_append_printf(output, " & & \\multicolumn{%d}{l}{", term_columns + 2);
    append_variable_names(output, problem);
    g_string_append(output, " $\\geq 0$} \\\\\n\\end{tabular}\n\\end{lrbox}\n"
                            "\\showfitted{0pt}\n"
                            "\\end{modelbox}\n");
}

static void append_table(GString *output, const Problem *problem, const Step *step, const char *caption) {
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

    g_string_append(output, "\\begin{table}[H]\n\\centering\n\\begin{lrbox}{\\fitbox}\n");
    if (table->column_count > WIDE_TABLE_COLUMNS) {
        g_string_append(output, "\\small\\setlength{\\tabcolsep}{3pt}\n");
    }

    // Base, Z y una columna por variable; una linea vertical separa cada grupo de columnas
    g_string_append(output, "\\begin{tabular}{c|c|");
    for (int column = 1; column < table->column_count; column++) {
        g_string_append(output, "c");
        if (column + 1 < table->column_count &&
            column_group(table->column_types[column]) != column_group(table->column_types[column + 1])) {
            g_string_append(output, "|");
        }
    }
    if (show_ratios) {
        g_string_append(output, "|c");
    }
    g_string_append(output, "}\n");

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

    for (int row = 0; row < table->row_count; row++) {
        if (row == 0) {
            g_string_append(output, "\\rowcolor{zrow}$Z$");
        } else {
            if (row == leaving_row) {
                g_string_append(output, "\\rowcolor{leaving}");
            }
            append_column_label(output, problem, table, table->basis[row - 1]);
        }

        for (int column = 0; column < table->column_count; column++) {
            g_string_append(output, " & ");
            if (column == entering_column) {
                g_string_append(output, "\\cellcolor{entering}");
            }
            if (row == leaving_row && column == entering_column) {
                g_string_append(output, "\\pivotcell{$");
                latex_append_fraction(output, table->cells[row][column]);
                g_string_append(output, "$}");
            } else if (row == 0) {
                g_string_append(output, "$");
                append_objective_value(output, table, column);
                g_string_append(output, "$");
            } else {
                g_string_append(output, "$");
                latex_append_fraction(output, table->cells[row][column]);
                g_string_append(output, "$");
            }
        }

        if (show_ratios) {
            g_string_append(output, " & ");
            if (row > 0 && step->ratio_is_valid[row - 1]) {
                g_string_append(output, "$");
                latex_append_fraction(output, table->cells[row][right_hand_side_column]);
                g_string_append(output, " \\div ");
                latex_append_fraction(output, table->cells[row][entering_column]);
                g_string_append(output, " = ");
                latex_append_fraction(output, step->ratios[row - 1]);
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
    g_string_append_printf(output, "\\hline\n\\end{tabular}\n\\end{lrbox}\n\\showfitted{0pt}\n"
                                   "\\caption{%s}\n\\end{table}\n",
                           caption);
}

static void append_zero_basics(GString *output, const Problem *problem, const Table *table) {
    int right_hand_side_column = table->column_count - 1;
    int is_first = 1;

    g_string_append(output, "\\par{\\small{\\color{warnorange}\\bfseries Nota de degeneración:} ");
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

    if (simplex_count_zero_basics(table) > 1) {
        g_string_append(output, " permanecen en la base con valor igual a $0$.}\n");
    } else {
        g_string_append(output, " permanece en la base con valor igual a $0$.}\n");
    }
}

static void append_decisions(GString *output, const Problem *problem, const Step *step, const char *reason,
                             int show_no_leaving) {
    const Table *table = &step->table;

    g_string_append(output, "\\begin{calloutbox}{tecblue}{Criterio de pivoteo en esta iteración}\n"
                            "\\chip{entering}{Entra \\textbf{");
    append_column_label(output, problem, table, step->entering_column);
    g_string_append(output, "}}");
    if (step->leaving_constraint != -1) {
        g_string_append(output, "\\enspace\\chip{leaving}{Sale \\textbf{");
        append_column_label(output, problem, table, table->basis[step->leaving_constraint]);
        g_string_append(output, "}}\\enspace\\chip{pivot}{Pivote: $");
        latex_append_fraction(output, step->pivot);
        g_string_append(output, "$}");
    }
    g_string_append(output, "\\\\[4pt]\n{\\small ");

    append_column_label(output, problem, table, step->entering_column);
    g_string_append_printf(output, " posee el %s.", reason);
    if (step->leaving_constraint != -1) {
        g_string_append(output, " Abandona la base ");
        append_column_label(output, problem, table, table->basis[step->leaving_constraint]);
        g_string_append_printf(output, " debido a que la restricción %d presenta la fracción mínima ($",
                               step->leaving_constraint + 1);
        latex_append_fraction(output, step->ratios[step->leaving_constraint]);
        g_string_append(output, "$).");
        if (step->tied_row_count > 1) {
            g_string_append_printf(output, " {\\color{tecred}\\textbf{Desempate:}} entre %d filas empatadas se "
                                           "eligió la variable básica de menor índice.",
                                   step->tied_row_count);
        }
        if (table->column_types[table->basis[step->leaving_constraint]] == COLUMN_ARTIFICIAL) {
            g_string_append(output, " {\\color{okgreen}\\textbf{Salida de artificial:}} su columna se omite en "
                                    "todas las tablas siguientes.");
        }
        if (fraction_sign(step->ratios[step->leaving_constraint]) == 0) {
            g_string_append(output, " {\\color{warnorange}\\textbf{Pivoteo degenerado:}} la fracción mínima es $0$, "
                                    "por lo que $Z$ no cambia en este paso.");
        }
    } else if (show_no_leaving) {
        g_string_append(output, " {\\color{tecred}\\textbf{Sin variable saliente:}} la columna entrante no tiene "
                                "coeficientes positivos.");
    }
    g_string_append(output, "}\n\\end{calloutbox}\n");
}

static void append_step(GString *output, const Problem *problem, const SimplexResult *result, int step_index,
                        const char *title, const char *note) {
    const Step *step = &result->steps[step_index];
    int is_last_step = (step_index == result->step_count - 1);

    g_string_append_printf(output, "\\subsection{%s}\n", title);
    if (note != NULL) {
        g_string_append_printf(output, "%s\n\n", note);
    }

    append_table(output, problem, step, title);

    if (step->zero_basic_count > 0) {
        append_zero_basics(output, problem, &step->table);
    }

    int is_infeasible_end = (is_last_step && result->status == SIMPLEX_INFEASIBLE);
    if (step->entering_column == -1 && is_infeasible_end) {
        g_string_append(output, "\\begin{calloutbox}{tecred}{Criterio de parada: Problema no factible}\n"
                                "Ningún coeficiente de la fila $Z$ mejora el objetivo, pero al menos una variable "
                                "artificial permanece en la base con valor estrictamente positivo.\n"
                                "\\end{calloutbox}\n");
    } else if (step->entering_column == -1) {
        g_string_append(output, "\\begin{calloutbox}{okgreen}{Criterio de parada: Tabla óptima alcanzada}\n");
        if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
            g_string_append(output, "Todos los coeficientes de la fila $Z$ son $\\leq 0$, por lo que no es posible "
                                    "reducir más el valor de $Z$.");
        } else {
            g_string_append(output, "Todos los coeficientes de la fila $Z$ son $\\geq 0$, por lo que no es posible "
                                    "incrementar más el valor de $Z$.");
        }
        g_string_append(output, "\n\\end{calloutbox}\n");
    } else if (step->table.objective_type == OBJECTIVE_MINIMIZE) {
        append_decisions(output, problem, step, "valor más positivo en la fila $Z$",
                         is_last_step && (result->status == SIMPLEX_UNBOUNDED || is_infeasible_end));
    } else {
        append_decisions(output, problem, step, "valor más negativo en la fila $Z$",
                         is_last_step && (result->status == SIMPLEX_UNBOUNDED || is_infeasible_end));
    }
}

// Tabla inicial de la Gran M (con M en las artificiales), antes de canonizar
static void append_initial_table(GString *output, const Problem *problem, const SimplexResult *result) {
    Step initial_step;
    memset(&initial_step, 0, sizeof(initial_step));
    initial_step.table = result->initial_table;
    initial_step.entering_column = -1;
    initial_step.leaving_constraint = -1;

    g_string_append(output, "\\subsection{Tabla Símplex Inicial (con variables artificiales)}\n"
                            "Se incorporan variables de holgura $s_i$ para restricciones $\\leq$, variables de "
                            "exceso $e_i$ y artificiales $a_i$ para restricciones $\\geq$, y variables artificiales "
                            "$a_i$ para igualdades $=$. ");
    if (problem->objective_type == OBJECTIVE_MINIMIZE) {
        g_string_append(output, "Al minimizar, cada variable artificial penaliza el objetivo con $+M$, "
                                "reflejándose como $-M$ en la fila $Z$ inicial.\n\n");
    } else {
        g_string_append(output, "Al maximizar, cada variable artificial penaliza el objetivo con $-M$, "
                                "reflejándose como $+M$ en la fila $Z$ inicial.\n\n");
    }
    append_table(output, problem, &initial_step, "Tabla Símplex Inicial antes de canonizar");
}

static void append_history(GString *output, const Problem *problem, const SimplexResult *result,
                           int show_intermediate_tables) {
    char title[MESSAGE_BUFFER_SIZE];
    char note[MESSAGE_BUFFER_SIZE];
    int last_index = result->step_count - 1;
    int hidden_count = 0;

    g_string_append(output, "\\section{Desarrollo del Algoritmo Símplex}\n");

    // Con artificiales se muestra primero la tabla inicial y luego la canonizada (la tabla 0)
    if (result->has_artificials) {
        append_initial_table(output, problem, result);
    }

    for (int step_index = 0; step_index <= last_index; step_index++) {
        int is_intermediate = (step_index > 0 && step_index < last_index);

        if (is_intermediate && !show_intermediate_tables) {
            hidden_count++;
            continue;
        }

        if (step_index == 0 && result->has_artificials && step_index == last_index) {
            snprintf(title, sizeof(title), "Canonización de Variables Artificiales y Tabla Final");
        } else if (step_index == 0 && result->has_artificials) {
            snprintf(title, sizeof(title), "Canonización de Variables Artificiales");
        } else if (step_index == 0 && step_index == last_index) {
            snprintf(title, sizeof(title), "Tabla Símplex Inicial y Final");
        } else if (step_index == 0) {
            snprintf(title, sizeof(title), "Tabla Símplex Inicial");
        } else if (step_index == last_index && result->status == SIMPLEX_OPTIMAL) {
            snprintf(title, sizeof(title), "Tabla Símplex Final (Iteración %d)", step_index);
        } else if (step_index == last_index) {
            snprintf(title, sizeof(title), "Última Tabla Símplex (Iteración %d)", step_index);
        } else {
            snprintf(title, sizeof(title), "Tabla Intermedia %d", step_index);
        }

        if (step_index == 0 && result->has_artificials) {
            snprintf(note, sizeof(note),
                     "Se anulan los términos en $M$ de las columnas artificiales en la fila $Z$ mediante "
                     "operaciones elementales de fila, obteniendo la primera tabla canónica válida para iterar.");
            append_step(output, problem, result, step_index, title, note);
        } else if (step_index == last_index && hidden_count > 0) {
            if (hidden_count == 1) {
                snprintf(note, sizeof(note),
                         "\\emph{Nota: Se omitió 1 tabla intermedia según la configuración seleccionada en la "
                         "interfaz gráfica.}");
            } else {
                snprintf(note, sizeof(note),
                         "\\emph{Nota: Se omitieron %d tablas intermedias según la configuración seleccionada en la "
                         "interfaz gráfica.}",
                         hidden_count);
            }
            append_step(output, problem, result, step_index, title, note);
        } else {
            append_step(output, problem, result, step_index, title, NULL);
        }
    }
}

static void append_objective_badge(GString *output, Fraction objective_value) {
    g_string_append(output,
                    "\\begin{center}\n"
                    "\\begin{tcolorbox}[width=0.55\\textwidth,colback=tecblue,colframe=tecblue,"
                    "arc=4pt,boxrule=0pt,halign=center]\n"
                    "{\\Large\\bfseries\\color{white} Valor Óptimo: $Z^{*} = ");
    latex_append_fraction(output, objective_value);
    g_string_append(output, "$}\n\\end{tcolorbox}\n\\end{center}\n");
}

// Variables de decision y holguras o excesos de una solucion
static void append_values(GString *output, const Problem *problem, const Fraction variable_values[],
                          const Fraction slack_values[]) {
    char auxiliary_name[NAME_BUFFER_SIZE];
    int slack_count = 0;
    int surplus_count = 0;

    for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
        if (problem->constraint_types[constraint] == CONSTRAINT_LESS_EQUAL) {
            slack_count++;
        } else if (problem->constraint_types[constraint] == CONSTRAINT_GREATER_EQUAL) {
            surplus_count++;
        }
    }

    g_string_append(output, "\\begin{table}[H]\n\\centering\n"
                            "\\begin{tabular}{@{}c@{\\hspace{1.5cm}}c@{}}\n"
                            "\\begin{tabular}{r@{\\;}c@{\\;}l}\n"
                            "\\toprule\n"
                            "\\multicolumn{3}{c}{\\color{tecblue}\\bfseries Variables de Decisión} \\\\\n"
                            "\\midrule\n");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append_printf(output, "%s & $=$ & $", problem->variable_names[variable]);
        latex_append_fraction(output, variable_values[variable]);
        g_string_append(output, "$ \\\\\n");
    }
    g_string_append(output, "\\bottomrule\n\\end{tabular}");

    if (slack_count + surplus_count > 0) {
        g_string_append(output, " &\n\\begin{tabular}{r@{\\;}c@{\\;}l}\n\\toprule\n");
        if (slack_count > 0 && surplus_count > 0) {
            g_string_append(output, "\\multicolumn{3}{c}{\\color{tecblue}\\bfseries Holguras y Excesos} \\\\\n");
        } else if (slack_count > 0) {
            g_string_append(output, "\\multicolumn{3}{c}{\\color{tecblue}\\bfseries Variables de Holgura} \\\\\n");
        } else {
            g_string_append(output, "\\multicolumn{3}{c}{\\color{tecblue}\\bfseries Variables de Exceso} \\\\\n");
        }
        g_string_append(output, "\\midrule\n");
        for (int constraint = 0; constraint < problem->constraint_count; constraint++) {
            if (problem->constraint_types[constraint] == CONSTRAINT_EQUAL) {
                continue;
            }
            if (problem->constraint_types[constraint] == CONSTRAINT_GREATER_EQUAL) {
                problem_surplus_name(constraint, auxiliary_name, NAME_BUFFER_SIZE);
            } else {
                problem_slack_name(constraint, auxiliary_name, NAME_BUFFER_SIZE);
            }
            g_string_append_printf(output, "%s & $=$ & $", auxiliary_name);
            latex_append_fraction(output, slack_values[constraint]);
            g_string_append(output, "$ \\\\\n");
        }
        g_string_append(output, "\\bottomrule\n\\end{tabular}");
    }
    g_string_append(output, "\n\\end{tabular}\n\\end{table}\n");
}

// Explicacion del problema no factible
static void append_infeasible(GString *output, const Problem *problem, const Step *last_step) {
    const Table *table = &last_step->table;
    int right_hand_side_column = table->column_count - 1;
    int is_first = 1;

    g_string_append(output, "\\begin{calloutbox}{tecred}{Diagnóstico: Problema No Factible}\n"
                            "El algoritmo Símplex con el Método de la Gran M finalizó su ejecución, pero en la "
                            "tabla terminal ");
    for (int constraint = 0; constraint < table->row_count - 1; constraint++) {
        int column = table->basis[constraint];
        if (table->column_types[column] != COLUMN_ARTIFICIAL ||
            fraction_sign(table->cells[constraint + 1][right_hand_side_column]) <= 0) {
            continue;
        }
        if (!is_first) {
            g_string_append(output, ", ");
        }
        append_column_label(output, problem, table, column);
        g_string_append(output, " $= ");
        latex_append_fraction(output, table->cells[constraint + 1][right_hand_side_column]);
        g_string_append(output, "$");
        is_first = 0;
    }
    g_string_append(output, " permanece en la base con un valor \\textbf{estrictamente positivo}.\n\n"
                            "La presencia de una variable artificial positiva en el óptimo de la Gran M demuestra "
                            "que es matemáticamente imposible satisfacer de manera simultánea todas las restricciones "
                            "del modelo. En consecuencia, \\textbf{la región factible es vacía} y el problema carece "
                            "de solución.\n"
                            "\\end{calloutbox}\n");
}

static void append_solution(GString *output, const Problem *problem, const SimplexResult *result) {
    const Step *last_step = &result->steps[result->step_count - 1];

    g_string_append(output, "\\section{Solución y Análisis de Resultados}\n");
    if (result->status == SIMPLEX_OPTIMAL) {
        append_objective_badge(output, result->objective_value);
        append_values(output, problem, result->variable_values, result->slack_values);
        if (result->multiple_kind != MULTIPLE_NONE) {
            g_string_append(output, "\\begin{calloutbox}{okgreen}{Detección de Soluciones Múltiples}\n"
                                    "La solución anterior no es única: existen infinitas soluciones óptimas que "
                                    "alcanzan exactamente el mismo valor de $Z^{*}$. A continuación se detalla el "
                                    "pivoteo adicional y la familia de soluciones.\n"
                                    "\\end{calloutbox}\n");
        }
        return;
    }

    if (result->status == SIMPLEX_UNBOUNDED) {
        g_string_append(output, "\\begin{calloutbox}{tecred}{Diagnóstico: Problema No Acotado}\n"
                                "En la última tabla, la variable ");
        append_column_label(output, problem, &last_step->table, last_step->entering_column);
        g_string_append(output, " mejora el valor de la función objetivo, pero todos los coeficientes de su columna "
                                "en las restricciones son menores o iguales a cero. Esto implica que dicha variable "
                                "puede crecer indefinidamente sin abandonar la región factible, haciendo que $Z$ "
                                "mejore sin límite finito.\n"
                                "\\end{calloutbox}\n");
        return;
    }
    if (result->status == SIMPLEX_INFEASIBLE) {
        append_infeasible(output, problem, last_step);
        return;
    }
    if (result->status == SIMPLEX_ITERATION_LIMIT) {
        g_string_append_printf(output, "\\begin{calloutbox}{tecred}{Límite de Iteraciones Alcanzado}\n"
                                       "Se alcanzó el límite máximo de %d pivoteos sin converger a una tabla óptima.\n"
                                       "\\end{calloutbox}\n",
                               MAX_ITERATIONS);
        return;
    }
    g_string_append_printf(output, "\\begin{calloutbox}{tecred}{Sin Solución}%s\\end{calloutbox}\n",
                           simplex_status_message(result->status));
}

static void append_column_vector(GString *output, const Fraction values[], int variable_count) {
    g_string_append(output, "\\begin{pmatrix}");
    for (int variable = 0; variable < variable_count; variable++) {
        if (variable > 0) {
            g_string_append(output, " \\\\ ");
        }
        latex_append_fraction(output, values[variable]);
    }
    g_string_append(output, "\\end{pmatrix}");
}

static void append_extra_points(GString *output, const Problem *problem, const SimplexResult *result,
                                const char *parameter_symbol, const long long parameters[][2]) {
    Fraction parameter;
    Fraction values[MAX_VARIABLES];

    g_string_append(output, "\\begin{table}[H]\n\\centering\n\\begin{tabular}{c|");
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append(output, "c");
    }
    g_string_append_printf(output, "}\n\\rowcolor{tecblue}\\color{white}$%s$", parameter_symbol);
    for (int variable = 0; variable < problem->variable_count; variable++) {
        g_string_append_printf(output, " & \\color{white}%s", problem->variable_names[variable]);
    }
    g_string_append(output, " \\\\\n");

    for (int index = 0; index < EXTRA_POINT_COUNT; index++) {
        FractionStatus fraction_status = fraction_make(parameters[index][0], parameters[index][1], &parameter);
        if (fraction_status != FRACTION_OK) {
            g_string_append_printf(output, "\\multicolumn{%d}{c}{%s} \\\\\n", problem->variable_count + 1,
                                   fraction_status_message(fraction_status));
            continue;
        }
        g_string_append(output, "$");
        latex_append_fraction(output, parameter);
        g_string_append(output, "$");
        if (simplex_alternative_point(result, problem->variable_count, parameter, values) != SIMPLEX_RUNNING) {
            g_string_append_printf(output, " & \\multicolumn{%d}{c}{Punto no representable de forma exacta} \\\\\n",
                                   problem->variable_count);
            continue;
        }
        for (int variable = 0; variable < problem->variable_count; variable++) {
            g_string_append(output, " & $");
            latex_append_fraction(output, values[variable]);
            g_string_append(output, "$");
        }
        g_string_append(output, " \\\\\n");
    }
    g_string_append(output, "\\hline\n\\end{tabular}\n"
                            "\\caption{Ejemplos adicionales de soluciones óptimas}\n\\end{table}\n");
}

static void append_multiple_solutions(GString *output, const Problem *problem, const SimplexResult *result) {
    const Step *step = &result->alternative_step;
    const long long segment_parameters[EXTRA_POINT_COUNT][2] = {{1, 4}, {1, 2}, {3, 4}};
    const long long ray_parameters[EXTRA_POINT_COUNT][2] = {{1, 1}, {2, 1}, {3, 1}};

    g_string_append(output, "\\subsection{Análisis de Soluciones Múltiples}\n"
                            "En la tabla óptima anterior, la variable no básica ");
    append_column_label(output, problem, &step->table, step->entering_column);
    g_string_append(output, " presenta un coeficiente igual a \\textbf{0 en la fila $Z$}. Esto indica que puede "
                            "ingresar a la base sin alterar el valor óptimo $Z^{*}$, generando una solución básica "
                            "alternativa.\n\n");

    append_table(output, problem, step, "Selección de pivote para obtener la segunda solución óptima");
    append_decisions(output, problem, step, "coeficiente $0$ en la fila $Z$", 1);

    if (result->multiple_kind == MULTIPLE_SEGMENT) {
        Step second_step = *step;
        second_step.table = result->alternative_table;
        second_step.entering_column = -1;
        second_step.leaving_constraint = -1;

        g_string_append(output, "\\subsubsection{Segunda Tabla Final y Solución Alternativa}\n");
        append_table(output, problem, &second_step, "Segunda Tabla Símplex Final");
        append_values(output, problem, result->alternative_variable_values, result->alternative_slack_values);

        g_string_append(output, "\\subsubsection{Ecuación General de Soluciones Óptimas}\n"
                                "Toda combinación lineal convexa entre ambos vértices óptimos (para variables ");
        append_variable_names(output, problem);
        g_string_append(output, ") también constituye una solución óptima:\n"
                                "\\[\n\\mathbf{x}(\\alpha) = \\alpha ");
        append_column_vector(output, result->variable_values, problem->variable_count);
        g_string_append(output, " + (1 - \\alpha) ");
        append_column_vector(output, result->alternative_variable_values, problem->variable_count);
        g_string_append(output, ", \\qquad 0 \\leq \\alpha \\leq 1\n\\]\n");
        append_extra_points(output, problem, result, "\\alpha", segment_parameters);
        return;
    }

    g_string_append(output, "\\subsubsection{Ecuación General de Soluciones Óptimas (Rayo)}\n"
                            "Dado que ninguna fila limita el crecimiento de ");
    append_column_label(output, problem, &step->table, step->entering_column);
    g_string_append(output, ", las soluciones óptimas describen una semirrecta infinita (rayo) para las variables ");
    append_variable_names(output, problem);
    g_string_append(output, ":\n"
                            "\\[\n\\mathbf{x}(t) = ");
    append_column_vector(output, result->variable_values, problem->variable_count);
    g_string_append(output, " + t ");
    append_column_vector(output, result->ray_direction, problem->variable_count);
    g_string_append(output, ", \\qquad t \\geq 0\n\\]\n");
    append_extra_points(output, problem, result, "t", ray_parameters);
}

static void append_degenerate(GString *output, const SimplexResult *result) {
    int degenerate_tables[MAX_STEPS] = {0};
    int degenerate_pivots[MAX_STEPS] = {0};
    int table_count = 0;
    int pivot_count = 0;

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

    g_string_append(output, "\\subsection{Análisis de Degeneración}\n"
                            "\\begin{calloutbox}{warnorange}{Problema Degenerado Detectado}\n"
                            "Durante la ejecución se presentó al menos una solución básica factible donde una o más "
                            "variables básicas tomaron el valor $0$. En estas situaciones, un pivoteo puede mantener "
                            "inalterado el valor de $Z$, abriendo la posibilidad teórica de ciclado.\n\n"
                            "\\textbf{Tablas con variables básicas en $0$} (numeradas desde la tabla canónica 0): ");
    append_step_list(output, degenerate_tables, table_count);
    g_string_append(output, ".\\\\\n");
    if (pivot_count > 0) {
        g_string_append(output, "\\textbf{Iteraciones donde $Z$ no cambió:} ");
        append_step_list(output, degenerate_pivots, pivot_count);
        g_string_append(output, ".\\\\\n");
    }
    g_string_append(output, "\\textbf{Resolución de empates:} Ante empates en la fracción mínima, se aplicó la regla "
                            "determinística de seleccionar la variable básica de menor índice de columna.\n"
                            "\\end{calloutbox}\n");
}

int latex_report_write(const char *path, const Problem *problem, const SimplexResult *result,
                       int show_intermediate_tables, char *message, size_t message_size) {
    GString *output = g_string_new(NULL);
    GError *error = NULL;

    append_preamble(output, problem);
    append_cover(output, problem);
    append_theory_section(output);
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
    // Extras: con 2 variables el gráfico de la región factible y con 3 el cuerpo Símplex
    if (problem->variable_count == 2) {
        g_string_append(output, "\\section{Representación Gráfica (2 Variables)}\n");
        plots_append_region_2d(output, problem, result);
    }
    if (problem->variable_count == 3) {
        g_string_append(output, "\\section{Representación Gráfica Tridimensional (3 Variables)}\n");
        plots_append_body_3d(output, problem, result);
    }
    g_string_append(output, "\\end{document}\n");

    int is_written = g_file_set_contents(path, output->str, (gssize)output->len, &error);
    if (!is_written) {
        snprintf(message, message_size, "No se pudo escribir el archivo LaTeX: %s", error->message);
        g_error_free(error);
    }
    g_string_free(output, TRUE);
    return is_written;
}