/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdio.h>
#include <string.h>

#include "code.h"
#include "prx.h"
#include "output.h"
#include "nids.h"
#include "hash.h"
#include "utils.h"

int g_verbosity;
int g_printoptions;

static
void print_help(char *prgname) {
	report("Usage:\n"
			"  %s [-g] [-n nidsfile] [-v] prxfile\n"
			"Where:\n"
			"  -c    output code\n"
			"  -d    print the dominator\n"
			"  -e    print edge types\n"
			"  -f    print the frontier\n"
			"  -g    output graphviz dot\n"
			"  -i    print prx info\n"
			"  -n    specify nids xml file\n"
			"  -q    print code into nodes\n"
			"  -r    print the reverse depth first search number\n"
			"  -s    print structures\n"
			"  -t    print depth first search number\n"
			"  -v    increase verbosity\n"
			"  -x    print the reverse dominator\n"
			"  -z    print the reverse frontier\n", prgname);
}

int main(int argc, char **argv) {
	char *prxfilename = NULL;
	char *nidsfilename = NULL;

	int i, j;
	int printgraph = FALSE;
	int printcode = FALSE;
	int printinfo = FALSE;

	struct nidstable *nids = NULL;
	struct prx *p = NULL;
	struct code *c;

	g_verbosity = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp("--help", argv[i]) == 0) {
			print_help(argv[0]);
			return 0;
		} else if (argv[i][0] == '-') {
			char *s = argv[i];
			for (j = 0; s[j]; j++) {
				switch (s[j]) {
				case 'v':
					g_verbosity++;
					break;
				case 'g':
					printgraph = TRUE;
					break;
				case 'c':
					printcode = TRUE;
					break;
				case 'i':
					printinfo = TRUE;
					break;
				case 't':
					g_printoptions |= OUT_PRINT_DFS;
					break;
				case 'r':
					g_printoptions |= OUT_PRINT_RDFS;
					break;
				case 'd':
					g_printoptions |= OUT_PRINT_DOMINATOR;
					break;
				case 'x':
					g_printoptions |= OUT_PRINT_RDOMINATOR;
					break;
				case 'f':
					g_printoptions |= OUT_PRINT_FRONTIER;
					break;
				case 'z':
					g_printoptions |= OUT_PRINT_RFRONTIER;
					break;
				case 'q':
					g_printoptions |= OUT_PRINT_CODE;
					break;
				case 's':
					g_printoptions |= OUT_PRINT_STRUCTURES;
					break;
				case 'e':
					g_printoptions |= OUT_PRINT_EDGE_TYPES;
					break;
				case 'n':
					if (i == (argc - 1))
						fatal(__FILE__ ": missing nids file");

					nidsfilename = argv[++i];
					break;
				}
			}
		} else {
			prxfilename = argv[i];
		}
	}

	printf("Welcome to PSP decompiler\n");
	if (!prxfilename) {
		print_help(argv[0]);
		return 0;
	}

	if (nidsfilename)
		nids = nids_load(nidsfilename);

	p = prx_load(prxfilename);
	if (!p)
		fatal(__FILE__ ": can't load prx `%s'", prxfilename);

	if (nids)
		prx_resolve_nids(p, nids);

	if (g_verbosity > 2 && nids && printinfo)
		nids_print(nids);

	if (g_verbosity > 0 && printinfo)
		prx_print(p, (g_verbosity > 1));

	c = code_analyse(p);
	if (!c)
		fatal(__FILE__ ": can't analyse code `%s'", prxfilename);

	if (printgraph)
		print_graph(c, prxfilename);

	if (printcode)
		print_code(c, prxfilename);

	code_free(c);

	prx_free(p);

	if (nids)
		nids_free(nids);
	printf("Done, exiting now\n");

	return 0;
}
