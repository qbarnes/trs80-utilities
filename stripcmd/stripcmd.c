/*
 * Parse a TRS-80 CMD file and report on it, and if an output file
 * is provided, create a copy without extraneous trailing bytes.
 *
 *
 * Some TRS-80 DOSes did not keep accurate end-of-file markers which
 * could result in trailing bytes.
 *
 * File name records are supposed to be up to 6 characters long, but
 * have found them much longer and being used like comment records.
 *
 * Has support for comment records marked with 0x1f.  According to
 * docs I found, comment records are limited to 127 characters, but
 * I've found CMD files with records that exceed that, so I'll assume
 * a new limit of 255.  Could they have a size of 0 represent 256?
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


#define	BLK_LEN(n)	((n) < 3 ? (n) + 254 : (n) - 2)

#define	FNAME_SIZE	0xff
#define	COMMENT_SIZE	0xff

enum cmd_header {
	LOADBLK	 = 0x01,
	XFERADDR = 0x02,
	FNAMEREC = 0x05,
	COMMREC  = 0x1f		/* L/LS-DOS specific */
};

enum cmd_state {
	CMD_HDR,
	LOADBLK_LEN,
	LOADBLK_ADDRLO,
	LOADBLK_ADDRHI,
	XFER_LEN,
	XFER_ADDRLO,
	XFER_ADDRHI,
	FNAMEREC_LEN,
	FNAMEREC_NAME,
	COMMREC_LEN,
	COMMREC_STRING,
	SKIP_BYTES,
	EXTRA_BYTES
};


int	Quiet = 0;
FILE	*InputFile;
FILE	*OutputFile;


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
		"Usage: %s [-q] [{cmd_file|-} [out_file]]\n"
		"Options:\n"
		"\t-q\tRun quietly (repeat for more quiet)\n";

	fatal(1, usage_str, pgmname);
}


int
process_file(FILE *infile, FILE *outfile)
{
	int		ch;
	enum cmd_state	state = CMD_HDR;
	uint16_t	load_addr = 0;
	uint16_t	xfer_addr = 0;
	char		fname[FNAME_SIZE+1];
	uint8_t		fname_len = 0;
	unsigned int	fname_idx = 0;
	char		comment[COMMENT_SIZE+1];
	uint8_t		comment_len = 0;
	unsigned int	comment_idx = 0;
	unsigned int	chskip = 0;
	unsigned int	extra_bytes = 0;

	while ((ch = fgetc(infile)) != -1) {
		if ((state != EXTRA_BYTES) && OutputFile)
			fwrite(&ch, 1, 1, OutputFile);

		switch(state) {
		case CMD_HDR:
			switch ((enum cmd_header)ch) {
			case LOADBLK:
				state = LOADBLK_LEN;
				break;
			case XFERADDR:
				state = XFER_LEN;
				break;
			case FNAMEREC:
				state = FNAMEREC_LEN;
				break;
			case COMMREC:
				state = COMMREC_LEN;
				break;
			default:
				fprintf(stderr,
					"Unexpected header byte (0x%02x).\n",
					ch);
					return 2;
				break;
			}
			break;

		case LOADBLK_LEN:
			chskip = (uint16_t)BLK_LEN(ch);
			state = LOADBLK_ADDRLO;
			break;

		case LOADBLK_ADDRLO:
			load_addr = ch;
			state = LOADBLK_ADDRHI;
			break;

		case LOADBLK_ADDRHI:
			load_addr |= ch << 8;
			if (!Quiet)
				printf("Load address == 0x%04x "
					"(len == 0x%02x)\n",
					(int)load_addr, (int)chskip);
			state = SKIP_BYTES;
			break;

		case XFER_LEN:
			chskip = (uint16_t)(ch - 2);
			state = XFER_ADDRLO;
			break;

		case XFER_ADDRLO:
			xfer_addr = ch;
			state = XFER_ADDRHI;
			break;

		case XFER_ADDRHI:
			xfer_addr |= ch << 8;
			if (!Quiet)
				printf("Transfer address == 0x%04x\n",
					(int)xfer_addr);
			if (chskip != 0) {
				fprintf(stderr, "Unexpected transfer "
					"address length (%d).\n", (int)chskip);
				return 2;
			}
			extra_bytes = 0;
			state = EXTRA_BYTES;
			break;

		case FNAMEREC_LEN:
			fname_len = ch;
			if (fname_len > FNAME_SIZE) {
				fprintf(stderr,
					"Unexpected file name size (0x%02x).\n",
					(int)fname_len);
				return 2;
			}
			fname_idx = 0;
			state = FNAMEREC_NAME;
			break;

		case FNAMEREC_NAME:
			fname[fname_idx++] = ch;
			if (fname_idx == fname_len) {
				/* Remove trailing spaces? */
				fname[fname_idx] = '\0';
				if (!Quiet)
					printf("Filename = \"%s\"\n", fname);
				state = CMD_HDR;
			}
			break;

		case COMMREC_LEN:
			comment_len = ch;
			if (comment_len > COMMENT_SIZE) {
				fprintf(stderr,
					"Unexpected comment size (0x%02x).\n",
					(int)comment_len);
				return 2;
			}
			comment_idx = 0;
			state = COMMREC_STRING;
			break;

		case COMMREC_STRING:
			comment[comment_idx++] = ch;
			if (comment_idx == comment_len) {
				comment[comment_idx] = '\0';
				/* Translate ^Ms and other non-printable
				 * characters to newlines? */
				if (!Quiet)
					printf("Comment = \"%s\"\n", comment);
				state = CMD_HDR;
			}
			break;

		case SKIP_BYTES:
			if (--chskip == 0)
				state = CMD_HDR;
			break;

		case EXTRA_BYTES:
			++extra_bytes;
			break;

		default:
			fprintf(stderr, "Unexpected state == %d\n", (int)state);
			return 3;
		}
	}

	if (Quiet < 2) {
		if (extra_bytes)
			printf("Found %u extraneous bytes at end of file.\n",
				extra_bytes);

		printf("CMD file looks good!\n");
	}

	return 0;
}


/*
 * Returns 0 on success, -1 on failure.
 */

static int
process_args(int argc, char **argv)
{
	int	opt;

	while ((opt = getopt(argc, argv, "q")) != -1) {
		switch (opt) {
		case 'q':
			++Quiet;
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

	if (((argc - optind) > 0) &&
	    (argv[optind][0] != '-') &&
	    (argv[optind][1] != '\0')) {
		const char	*ifile = argv[optind];
		FILE		*ifp;

		if ((ifp = fopen(ifile, "r"))) {
			InputFile = ifp;
		} else {
			fprintf(stderr, "Failed to open file '%s', %s (%d)\n\n",
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
			fprintf(stderr, "Failed to open file '%s', %s (%d)\n\n",
				ofile,  strerror(errno), errno);
			return -1;
		}
	} else {
		OutputFile = 0;
	}

	return 0;
}


/*
 * Exit --
 *      0: Success
 *      1: User error (bad args)
 *      2: Input file error (bad EDTASM file)
 *      3: Internal error (bad programmer!)
 */

int
main(int argc, char **argv)
{
	int	ret = 0;

	if (process_args(argc, argv))
		usage(argv[0]);

	ret = process_file(InputFile, OutputFile);

	if (ret == 0 && OutputFile) {
		/* We think we succeeded, but let's be sure. */

		if (ferror(OutputFile))
			fatal(3, "Error detected after writing output file.\n");

		if (fclose(OutputFile) == EOF)
			fatal(3, "Error detected when closing output file, "
					"%s (%d)\n", strerror(errno), errno);
	}

	return ret;
}
