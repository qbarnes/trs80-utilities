/*
 * Copyright 2023, Quentin L. Barnes
 *
 * Convert a file in TRS-80 EDTASM format to plain text.
 *
 * The TRS-80 EDTASM file format has at least two known variations.
 *
 * The first variation is an older format first used when EDTASM moved
 * from cassette based to disk files.  It's identical except for the
 * starting cassette head block (256 zeros + 0xA5).
 *
 * The second, newer format came along later, unclear when and
 * if its different aspects evolved differently.
 *
 * The first format starts with a filename header which is absent
 * in the later format starting directly with a line format.
 *
 * Header format:
 *   Bytes	Value	Definition
 *   0x00	0xD3	Marker of start of filename
 *   0x01-0x06		ASCII file name.  If shorter than 6 character,
 *   			filename is padded with spaces [0x20]
 *
 * Line format:
 *   Bytes	Value	   Definition
 *   0x00-0x04	0xB0-0xB9  5 digit line number in base 10 (0-9), but with
 *   			   their high bit set.
 *   0x05	0x20|0x09  Older format the character immediately following
 *   			   the line number is a space [0x20].  The newer
 *   			   format has a tab [0x09].
 *   0x06-*		   Assembly directives and comments
 *   End	0x0D	   End of Line character [0x0D]
 *
 * The line format repeats indefinitely until end of file.  The End of
 * File is marked with a 0x1A character.
 *
 * https://www.trs-80.com/wordpress/tips/formats/#edasfile
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


#define	HEADERCHAR	0xd3
#define	EOLCHAR		0x0d
#define	EOFCHAR		0x1a
#define	LINENUMCHAR(c)	((c) >= 0xb0 && (c) <= 0xb9)


enum edtasm_state {
	ES_HDR,
	ES_FNAME,
	ES_LINENUM,
	ES_LINETXT
};


/* Command line argument values. */
FILE	*InputFile;
FILE	*OutputFile;
int	Cvt_Newer_Format;
int	Show_File_Hdr;
int	Show_Linenums;


void
vfatal(int exitval, const char *fmsg, va_list ap)
{
	vfprintf(stderr, fmsg, ap);
	exit(exitval);
}


void
fatal(int exitval, const char *fmsg, ...)
{
	va_list ap;

	va_start(ap, fmsg);
	vfatal(exitval, fmsg, ap);
	va_end(ap);
}


void
usage(const char *pgmname)
{
	static const char usage_str[] =
		"Usage: %s [-cfs] [[edtasm_file] out_file]\n"
		"Options:\n"
		"\t-c\tConvert to newer format\n"
		"\t-f\tShow file header if present\n"
		"\t-s\tStrip line numbers\n";

	fatal(1, usage_str, pgmname);
}


static int
process_file(FILE *infile, FILE *outfile, int show_hdr, int cvt_newerfmt)
{
	int			ch;
	enum edtasm_state	state = ES_HDR;
	int			fnc = 0;
	int			lncc = 0;
	int			tlcc = 0;

	while ((ch = fgetc(infile)) != -1) {
		if (state == ES_HDR) {
			/* Look at first char to determine file format
			 * and starting state. */
			if (ch == HEADERCHAR) {
				state = ES_FNAME;
				continue;
			} else if (LINENUMCHAR(ch)) {
				state = ES_LINENUM;
			} else {
				fprintf(stderr, "Unexpected file format.\n");
				return 2;
			}
		}

		switch (state) {
		case ES_FNAME:
			/* Process filename. */
			if (show_hdr) {
				if (fnc == 0)
					fprintf(outfile, "FILENAME: ");

				fprintf(outfile, "%c", ch);
			}

			if (fnc++ == 5) {
				if (show_hdr) fprintf(outfile, "\n\n");
				fnc = 0;
				state = ES_LINENUM;
			}
			break;

		case ES_LINENUM:
			/* Process line number. */
			if (LINENUMCHAR(ch)) {
				if (Show_Linenums)
					fprintf(outfile, "%c", ch & 0x7f);
				if (++lncc == 5) {
					lncc = 0;
					state = ES_LINETXT;
				}
			} else if (ch == EOFCHAR) {
				return 0;
			} else {
				fprintf(stderr, "Bad line number.\n");
				return 2;
			}
			break;

		case ES_LINETXT:
			/* Process line text. */
			if (tlcc++ == 0) {
				if (ch == ' ' || ch == '\t') {
					if (Show_Linenums) {
						if (ch == ' ' &&
						    (cvt_newerfmt == 1)) {
						    	ch = '\t';
						}
						fprintf(outfile, "%c", ch);
					}
				} else {
					fprintf(stderr, "Unexpected character "
							"following line number "
							"(%02x).\n", ch);
					return 2;
				}
			} else if (ch == EOLCHAR) {
				fprintf(outfile, "\n");
				tlcc = 0;
				state = ES_LINENUM;
			} else {
				fprintf(outfile, "%c", ch);
			}
			break;

		default:
			fprintf(stderr, "Bad state (%d, 0x%02x).\n",
				state, ch);
			return 3;
		}
	}

	return 0;
}


static int
process_args(int argc, char **argv)
{
	int	opt;

	Show_File_Hdr = 0;
	Cvt_Newer_Format = 0;
	Show_Linenums = 1;

	while ((opt = getopt(argc, argv, "cfs")) != -1) {
		switch (opt) {
		case 'c':
			Cvt_Newer_Format = 1;
			break;

		case 'f':
			Show_File_Hdr = 1;
			break;

		case 's':
			Show_Linenums = 0;
			break;

		default:
			fprintf(stderr, "\n");
			return -1;
		}
	}

	if ((argc - optind) > 2) {
		fprintf(stderr, "Too many operands.\n\n");
		return -1;
	}

	if ((argc - optind) > 0) {
		const char	*ifile = argv[optind];
		FILE		*ifp;

		if ((ifp = fopen(ifile, "r"))) {
			InputFile = ifp;
		} else {
			fprintf(stderr,
				"Failed to open file '%s', %s (%d)\n\n",
				ifile,  strerror(errno), errno);
			return -1;
		}
	} else {
		InputFile = stdin;
	}

	if ((argc - optind) > 1) {
		const char	*ofile = argv[optind+1];
		FILE		*ofp;

		if ((ofp = fopen(ofile, "w+"))) {
			OutputFile = ofp;
		} else {
			fprintf(stderr,
				"Failed to open file '%s', %s (%d)\n\n",
				ofile,  strerror(errno), errno);
			return -1;
		}
	} else {
		OutputFile = stdout;
	}

	return 0;
}


/*
 * Exit --
 * 	0: Success
 * 	1: User error (bad args)
 * 	2: Input file error (bad EDTASM file)
 * 	3: Internal error (bad programmer!)
 */

int
main(int argc, char **argv)
{
	int	ret = 0;

	if (process_args(argc, argv))
		usage(argv[0]);

	ret = process_file(InputFile, OutputFile,
				Show_File_Hdr, Cvt_Newer_Format);

	if (ret == 0) {
		/* We think we succeeded, but let's be sure. */

		if (ferror(OutputFile))
			fatal(3, "Error detected after writing output file.\n");

		if (fclose(OutputFile) == EOF)
			fatal(3, "Error detected when closing output file, "
					"%s (%d)\n", strerror(errno), errno);
	}

	return ret;
}
