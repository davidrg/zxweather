NAME
     getopt - get option letter from argument vector

SYNOPSIS
     #include <unistd.h>

     int getopt (int argc, char * const argv[], const char *optstring);

     extern char *optarg;

     extern int optind, opterr, optopt;

     extern void getoptreset (void);

DESCRIPTION
     getopt returns the next option letter in argv that matches a letter in
     optstring.  It supports all the rules of the command syntax standard [see
     intro(1)].  Since all new commands are intended to adhere to the command
     syntax standard, they should use getopts(1), getopt(3C), or getsubopt(3C)
     to parse positional parameters and check for options that are legal for
     that command.

     optstring must contain the option letters that the command using getopt
     will recognize.  If a letter is followed by a colon, the option is
     expected to have an argument, or group of arguments, which may be
     separated from it by white space.  optarg is set to point to the start of
     the option argument on return from getopt.

     getopt places in optind the argv index of the next argument to be
     processed.  optind is external and is initialized to 1 before the first
     call to getopt.  When all options have been processed (i.e., up to the
     first non-option argument), getopt returns -1.  The special option ``--''
     (two hyphens) may be used to delimit the end of the options; when it is
     encountered, -1 is returned and ``--'' is skipped. This is useful in
     delimiting non-option arguments that begin with ``-'' (hyphen).

     getoptreset can be used to reset all the internal state of getopt so that
     it may be used again on a different set of arguments.

EXAMPLE
     The following code fragment shows how one might process the arguments for
     a command that can take the mutually exclusive options a and b, and the
     option o, which requires an argument:

     #include <unistd.h>

     main (int argc, char *argv[])
     {
          int c;
          extern char *optarg;
          extern int optind;
          int aflg = 0;
          int bflg = 0;
          int errflg = 0;
          char *ofile = NULL;

          while ((c = getopt(argc, argv, "abo:")) != -1)
               switch (c) {
               case 'a':
                    if (bflg)
                         errflg++;
                    else
                         aflg++;
                    break;
               case 'b':
                    if (aflg)
                         errflg++;
                    else
                         bflg++;
                    break;
               case 'o':
                    ofile = optarg;
                    (void)printf("ofile = %s\n", ofile);
                    break;
               case '?':
                    errflg++;
               }
          if (errflg) {
               (void)fprintf(stderr,
                    "usage: cmd [-a|-b] [-o<file>] files...\n");
               exit (2);
          }
          for ( ; optind < argc; optind++)
               (void)printf("%s\n", argv[optind]);
          return 0;
     }

FILES
     /usr/lib/locale/locale/LC_MESSAGES/uxlibc
          language-specific message file [See LANG on environ(5).]

SEE ALSO
     getopts(1), intro(1), getsubopt(3C), pfmt(3C), setlabel(3C)

RETURN VALUE
     The getopt function returns the next option character specified on the
     command line.

     A colon character ``:''  is returned if getopt detects a missing argument
     and the first character of optstring was a colon character ``:''.

     A question mark character ``?''  is returned if getopt encounters an
     option character not in optstring or detects a missing argument and the
     first character of optstring was not a colon character ``:''.

     Otherwise getopt returns -1 when all command line options are parsed.

DIAGNOSTICS
     If getopt detects a missing option-argument, it returns the ``:''  colon
     character if the first character of optstring was a colon, or a ``?''
     (question mark) character otherwise.  If the application has not set the
     variable opterr to zero and the first character of optstring is not a
     colon, getopt also prints a diagnostic message in the standard error
     format.  The value of the character that caused the error is in optopt.

     The label defined by a call to setlabel(3C) will be used if available;
     otherwise the name of the utility (argv[0]) will be used.  Remember to
     set the environment variable (NOMSGSEVERITY=1) for X/Open conformance.
     Also, the environment variable (NOMSGLABEL) must be not defined in order
     for setlabel(3C) to print labels.

NOTES
     The library routine getopt does not fully check for mandatory arguments.
     That is, given an option string a:b and the input -a -b, getopt assumes
     that -b is the mandatory argument to the option -a and not that -a is
     missing a mandatory argument.

     It is a violation of the command syntax standard [see intro(1)] for
     options with arguments to be grouped with other options, as in cmd
     -aboxxx file, where a and b are options, o is an option that requires an
     argument, and xxx is the argument to o.  Although this syntax is
     permitted in the current implementation, it should not be used because it
     may not be supported in future releases. The correct syntax is cmd -ab -o
     xxx file.