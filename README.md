```
MYSQL-XML-TO-CSV(1)         BSD General Commands Manual         MYSQL-XML-TO-CSV(1)

NAME
     mysql-xml-to-csv -- Convert MySQL XML output to CSV

SYNOPSIS
     mysql-xml-to-csv [-E line] [-n value] [-N] [-s separator] [input.xml]

DESCRIPTION
     mysql-xml-to-csv converts MySQL XML query results (i.e., produced using the
     mysql(1) command when given the --xml flag) into a CSV file.

     The XML input is read from input.xml, if specified, otherwise standard input.
     The CSV output is written to standard output.

     The first row of the CSV output contains column names unless the -N flag is
     given.

     If the query result set contains zero rows, then by default nothing is output;
     this behavior can be altered using the -E flag.

     By default NULL column values are converted into the empty string; this behav-
     ior can be altered using the -n flag.

     The character encoding used for the CVS output is UTF-8.

OPTIONS
     -E      If the result set contains zero rows, output line followed by a new-
             line character instead of outputting nothing.  No validation of line
             is performed.

     -n      Output value instead of the empty string for NULL values.

     -N      Do not output the column names in the first row of the output file.

     -s      Specify an alternate column separator (default is comma).

RETURN VALUES
     mysql-xml-to-csv exits with one of the following values:

     0       Input was successfully processed.

     1       An error occurred.

SEE ALSO
     mysql-xml-to-csv: Convert MySQL XML output to CSV,
     https://github.com/archiecobbs/mysql-xml-to-csv.

AUTHOR
     Archie L. Cobbs <archie.cobbs@gmail.com>

BSD                              September 16, 2024                             BSD
```
