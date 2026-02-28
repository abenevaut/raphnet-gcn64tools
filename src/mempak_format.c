/*	gc_n64_usb : Gamecube or N64 controller to USB adapter firmware
	Copyright (C) 2007-2015  Raphael Assenat <raph@raphnet.net>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <getopt.h>
#include "mempak.h"

#define DEFAULT_FORMAT_STR	"n64"

static void print_usage(void)
{
	printf("Usage: ./mempak_format file <options>\n");
	printf("\n");
	printf("Options:\n");
	printf("   -h                Display help\n");
	printf("   -f format         Write file in specified format (default: %s)\n", DEFAULT_FORMAT_STR);
	printf("   -b banks          Number of 32 KiB banks (1=32K, 4=128K/1Meg, 16=512K/4Meg, max 62)\n");
	printf("                     Default: 1 (standard Controller Pak)\n");
	printf("\n");
	printf("Formats:\n");
	printf("   mpk               Standard 32kB .mpk file format\n");
	printf("   mpk4              128kB .mpk file (4 copies or the 32kB block)\n");
	printf("   n64               .N64 file format\n");
}

int main(int argc, char **argv)
{
	mempak_structure_t *mpk;
	const char *outfile;
	unsigned char type;
	struct option long_options[] = {
		{ "format", required_argument, 0, 'f' },
		{ "banks",  required_argument, 0, 'b' },
		{ "help",   no_argument,       0, 'h' },
		{ }, // terminator
	};
	const char *format = DEFAULT_FORMAT_STR;
	unsigned int n_banks = 1;

	if (argc < 2) {
		print_usage();
		return 1;
	}

	while(1) {
		int c;

		c = getopt_long(argc, argv, "f:b:h", long_options, NULL);
		if (c==-1)
			break;

		switch(c)
		{
			case 'h':
				print_usage();
				return 0;
			case 'f':
				format = optarg;
				break;
			case 'b':
				{
					char *endp;
					unsigned long v = strtoul(optarg, &endp, 10);
					if (endp == optarg || v < 1 || v > 62) {
						fprintf(stderr, "Invalid bank count (1-62)\n");
						return -1;
					}
					n_banks = (unsigned int)v;
				}
				break;
			case '?':
				fprintf(stderr, "Unknown argument. Try -h\n");
				return -1;
			default:
				break;
		}
	}

	type = mempak_string2format(format);
	if (type == MPK_FORMAT_INVALID) {
		fprintf(stderr, "Unknown format specified\n");
		return -1;
	}

	outfile = argv[optind];
	mpk = mempak_new_ex(n_banks);
	if (!mpk) {
		return 1;
	}

	mempak_saveToFile(mpk, outfile, type);
	mempak_free(mpk);

	printf("Wrote empty (formatted) memory card file '%s' in '%s' format (%u bank(s), %u bytes)\n",
	       outfile, mempak_format2string(type), n_banks, n_banks * 0x8000);

	return 0;
}
