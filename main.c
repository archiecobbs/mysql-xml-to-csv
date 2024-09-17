#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Sanity check expat gives us UTF-8 characters */
#ifdef XML_UNICODE
#error "Only UTF-8 encoding is supported"
#endif

#define COPYRIGHT   "\xc2\xa9"

/*
    Example of MySQL XML output:

    <?xml version="1.0"?>

    <resultset xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" statement="SELECT id as IdNum, lastName, firstName FROM User">
        <row>
            <field name="IdNum">100040</field>
            <field name="lastName" xsi:nil="true"/>
            <field name="firsttName">Cher</field>
        </row>
    </resultset>
*/

#define BUFFER_SIZE                 (1 << 16)
#define DEFAULT_COLUMN_SEPARATOR    ","

// Version string
extern const char *const mysql_xml_to_csv_version;

// These accumulate the first row column names and values until first row is entirely read (unless the "-N" flag is given)
static XML_Char **column_names;
static size_t num_column_names;
static XML_Char **first_row_values;
static size_t num_first_row_values;

// Column separator
static const char *column_separator = DEFAULT_COLUMN_SEPARATOR;

// This accumulates one column's value
static XML_Char *elem_text;                     // note: not nul-terminated
static size_t elem_text_len;

// Flags
static int first_column;                        // the current column is the first column
static int reading_value;                       // we are reading the current column's value
static int num_rows_seen;                       // the number of rows we have seen so far
static int value_is_null;                       // the current column value is null
static const char *null_value;                  // string to output for null values (default empty)

// Expat callback functions
static void handle_elem_start(void *data, const XML_Char *el, const XML_Char **attrs);
static void handle_elem_text(void *data, const XML_Char *s, int len);
static void handle_elem_end(void *data, const XML_Char *el);

// Helper functions
static const XML_Char *find_attribute(const XML_Char **const attrs, const char *name);
static void output_csv_row(XML_Char **values, size_t num);
static void output_csv_text(const char *s, size_t len);
static void add_string(XML_Char ***arrayp, size_t *lengthp, const XML_Char *string, size_t len);
static void add_chars(XML_Char **strp, size_t *lenp, const XML_Char *string, size_t nchars);
static size_t xml_strlen(const XML_Char *string);
static void free_strings(XML_Char ***arrayp, size_t *lengthp);
static void usage(void);

int
main(int argc, char **argv)
{
    char buf[BUFFER_SIZE];
    int want_column_names = 1;                  // output column names as the first CSV row
    const char *empty_output = NULL;            // what to print if there are zero rows
    XML_Parser p;
    FILE *fp;
    size_t r;
    int i;

    // Parse command line
    while ((i = getopt(argc, argv, "E:hn:Ns:v")) != -1) {
        switch (i) {
        case 'E':
            empty_output = optarg;
            break;
        case 'n':
            null_value = optarg;
            break;
        case 'N':
            want_column_names = 0;
            break;
        case 'h':
            usage();
            exit(0);
       case 's':
            if (*optarg == '\0' || *optarg == '"')
                errx(1, "invalid separator: %s", optarg);
            column_separator = optarg;
            break;
        case 'v':
            fprintf(stderr, "mysql-xml-to-csv %s\n", mysql_xml_to_csv_version);
            fprintf(stderr, "Copyright %s Archie L. Cobbs. All rights reserved.\n", COPYRIGHT);
            exit(0);
        case '?':
        default:
            usage();
            exit(1);
        }
    }
    argv += optind;
    argc -= optind;
    switch (argc) {
    case 0:
        fp = stdin;
        break;
    case 1:
        if (strcmp(argv[0], "-") == 0)
            fp = stdin;
        else if ((fp = fopen(argv[0], "r")) == NULL)
            err(1, "%s", argv[0]);
        break;
    default:
        usage();
        exit(1);
    }

    // Initialize arrays for column names and first row values
    if (want_column_names) {
        if ((column_names = malloc(10 * sizeof(*column_names))) == NULL)
            err(1, "malloc");
        if ((first_row_values = malloc(10 * sizeof(*first_row_values))) == NULL)
            err(1, "malloc");
    }

    // Initialize parser
    if ((p = XML_ParserCreate(NULL)) == NULL)
        errx(1, "can't initialize parser");
    XML_SetElementHandler(p, handle_elem_start, handle_elem_end);
    XML_SetCharacterDataHandler(p, handle_elem_text);

    // Process file
    while (1) {
        if ((r = fread(buf, 1, sizeof(buf), fp)) == 0 && ferror(fp))
            errx(1, "error reading input");
        if (XML_Parse(p, buf, r, r == 0) == XML_STATUS_ERROR)
            errx(1, "line %u: %s", (unsigned int)XML_GetCurrentLineNumber(p), XML_ErrorString(XML_GetErrorCode(p)));
        if (r == 0)
            break;
    }

    // Any rows encountered?
    if (num_rows_seen == 0 && empty_output != NULL)
        printf("%s\n", empty_output);

    // Clean up
    XML_ParserFree(p);
    fclose(fp);

    // Done
    return 0;
}

static void
handle_elem_start(void *data, const XML_Char *name, const XML_Char **attrs)
{
    const XML_Char *field_name;
    const XML_Char *nil_status;

    if (strcmp(name, "row") == 0) {
        first_column = 1;
        num_rows_seen++;
    } else if (strcmp(name, "field") == 0) {
        if (column_names != NULL) {
            if ((field_name = find_attribute(attrs, "name")) == NULL)
                errx(1, "row %d: \"%s\" element is missing \"%s\" attribute", num_rows_seen, "field", "name");
            add_string(&column_names, &num_column_names, field_name, xml_strlen(field_name));
        } else {
            if (!first_column)
                fputs(column_separator, stdout);
            putchar('"');
        }
        value_is_null = (nil_status = find_attribute(attrs, "xsi:nil")) != NULL && strcmp(nil_status, "true") == 0;
        reading_value = 1;
    }
}

static const XML_Char *
find_attribute(const XML_Char **attrs, const char *target)
{
    const XML_Char *name;
    const XML_Char *value;

    while (*attrs != NULL) {
        name = attrs[0];
        value = attrs[1];
        if (strcmp(name, target) == 0)
            return value;
        attrs += 2;
    }
    return NULL;
}

static void
handle_elem_text(void *data, const XML_Char *s, int len)
{
    if (!reading_value)
        return;
    if (value_is_null) {
//       if (len > 0)
//           errx(1, "row %d: non-empty value for field with xsi:nil=\"true\"", num_rows_seen);
        if (null_value != NULL)
            return;
    }
    if (column_names != NULL)
        add_chars(&elem_text, &elem_text_len, s, len);
    else
        output_csv_text(s, len);
}

static void
handle_elem_end(void *data, const XML_Char *name)
{
    if (strcmp(name, "row") == 0) {
        if (column_names != NULL) {
            output_csv_row(column_names, num_column_names);
            output_csv_row(first_row_values, num_first_row_values);
            free_strings(&column_names, &num_column_names);
            free_strings(&first_row_values, &num_first_row_values);
        } else
            putchar('\n');
    } else if (strcmp(name, "field") == 0) {
        if (value_is_null && null_value != NULL) {      // pretend "null_value" was the text content
            value_is_null = 0;
            handle_elem_text(data, null_value, strlen(null_value));
        }
        if (column_names != NULL) {
            add_string(&first_row_values, &num_first_row_values, elem_text, elem_text_len);
            free(elem_text);
            elem_text = NULL;
            elem_text_len = 0;
        } else
            putchar('"');
        first_column = 0;
        reading_value = 0;
    }
}

static void
output_csv_row(XML_Char **values, size_t num_columns)
{
    int i;

    for (i = 0; i < num_columns; i++) {
        if (i > 0)
            fputs(column_separator, stdout);
        putchar('"');
        output_csv_text(values[i], xml_strlen(values[i]));
        putchar('"');
    }
    putchar('\n');
}

static void
output_csv_text(const XML_Char *s, size_t len)
{
    while (len-- > 0) {
        if (*s == '"')
            putchar('"');
        putchar(*s);
        s++;
    }
}

static void
add_string(XML_Char ***arrayp, size_t *lengthp, const XML_Char *string, size_t nchars)
{
    char **new_array;

    if ((new_array = realloc(*arrayp, (*lengthp + 1) * sizeof(**arrayp))) == NULL)
        err(1, "malloc");
    *arrayp = new_array;
    if (((*arrayp)[*lengthp] = malloc((nchars + 1) * sizeof(XML_Char))) == NULL)
        err(1, "malloc");
    memcpy((*arrayp)[*lengthp], string, nchars * sizeof(XML_Char));
    (*arrayp)[*lengthp][nchars] = (XML_Char)0;
    (*lengthp)++;
}

static void
add_chars(XML_Char **strp, size_t *lenp, const XML_Char *string, size_t nchars)
{
    XML_Char *new_array;

    if ((new_array = realloc(*strp, (*lenp + nchars) * sizeof(XML_Char))) == NULL)
        err(1, "malloc");
    *strp = new_array;
    memcpy(*strp + *lenp, string, nchars * sizeof(XML_Char));
    *lenp += nchars;
}

static size_t
xml_strlen(const XML_Char *string)
{
    size_t len;

    len = 0;
    while (string[len] != (XML_Char)0)
        len++;
    return len;
}

static void
free_strings(char ***arrayp, size_t *lengthp)
{
    while (*lengthp > 0)
        free((*arrayp)[--*lengthp]);
    free(*arrayp);
    *arrayp = NULL;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: mysql-xml-to-csv [options] [file.xml]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -E line\tOutput \"line\" if result has zero rows (default output nothing)\n");
    fprintf(stderr, "  -n value\tOutput \"value\" for NULL values (default empty string)\n");
    fprintf(stderr, "  -N\t\tDo not output column names as the first CSV row\n");
    fprintf(stderr, "  -s char\tSpecify column separator (default \"%s\")\n", DEFAULT_COLUMN_SEPARATOR);
    fprintf(stderr, "  -h\t\tShow this usage info\n");
    fprintf(stderr, "  -v\t\tShow program version\n");
}
