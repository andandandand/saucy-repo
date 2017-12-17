/*
 * main.c
 * Program entry and option parsing
 *
 * by Paul T. Darga <pdarga@umich.edu>
 * and Mark Liffiton <liffiton@umich.edu>
 * and Hadi Katebi <hadik@eecs.umich.edu>
 *
 * Copyright (C) 2004, The Regents of the University of Michigan
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "saucy.h"
#include "amorph.h"
#include "util.h"
#include "platform.h"

static char *filename;   /* Graph file we're reading */
static int timeout = 0;      /* Seconds before quitting after refinement */
static sig_atomic_t timeout_flag = 0; /* Has the alarm gone off yet? */
static int stats_mode;  /* Print out stats when we're done */
static int quiet_mode;  /* Don't output automorphisms */
static int gap_mode;    /* Do GAP I/O (interface with Shatter) */
static int cnf_mode;   /* Read CNF instead of graphs */
static int digraph_mode; /* Read digraphs; order matters in input */
static int repeat = 1; /* Repeat count, for benchmarking */
static int first;      /* Have we seen the first automorphism? (for gap) */
static char *marks;    /* "Bit" vector for printing */

/* Stats are global so we can print them from the signal handler */
struct saucy_stats stats;

static void arg_cnf(char *arg) { cnf_mode = 1; }
static void arg_digraph(char *arg) { digraph_mode = 1; }
static void arg_shatter(char *arg) { gap_mode = 1; }
static void arg_stats(char *arg) { stats_mode = 1; }
static void arg_quiet(char *arg) { quiet_mode = 1; }

static void
arg_timeout(char *arg)
{
	timeout = atoi(arg);
	if (timeout <= 0) die("timeout must be positive");
}

static void
arg_repeat(char *arg)
{
	repeat = atoi(arg);
	if (repeat <= 0) die("repeat count must be positive");
}

static void
arg_version(char *arg)
{
	printf("saucy %s\n", SAUCY_VERSION);
	exit(0);
}

static void arg_help(char *arg);

static struct option options[] = {
	{ "cnf", 'c', 0, arg_cnf,
	"treat the input as a DIMACS-format CNF formula" },
	{ "digraph", 'd', 0, arg_digraph,
	"treat the (amorph-format) input as a directed graph" },
	{ "shatter", 'g', 0, arg_shatter,
	"treat the input as coming in GAP mode, for Shatter" },
	{ "stats", 's', 0, arg_stats,
	"output various statistics after the generators" },
	{ "quiet", 'q', 0, arg_quiet,
	"do not print the generators found" },
	{ "timeout", 't', "N", arg_timeout,
	"after N seconds, the next generator will be the last" },
	{ "repeat", 'r', "N", arg_repeat,
	"run saucy N times; used for benchmarking (implies -sq)" },
	{ "help", 0, 0, arg_help,
	"output this help message" },
	{ "version", 0, 0, arg_version,
	"version information" },
	{ 0, 0, 0, 0, 0 }
};

static void
arg_help(char *arg)
{
	printf("usage: saucy [OPTION]... FILE\n");
	print_options(options);
	exit(0);
}

static void
timeout_handler(void)
{
	/* Do nothing but set a flag to be tested during the search */
	timeout_flag = 1;
}

static void
print_stats(FILE *f)
{
	fprintf(f, "group size = %fe%d\n",
		stats.grpsize_base, stats.grpsize_exp);
	fprintf(f, "levels = %d\n", stats.levels);
	fprintf(f, "nodes = %d\n", stats.nodes);
	fprintf(f, "generators = %d\n", stats.gens);
	fprintf(f, "total support = %d\n", stats.support);
	fprintf(f, "average support = %.2f\n",
		divide(stats.support, stats.gens));
	fprintf(f, "nodes per generator = %.2f\n",
		divide(stats.nodes, stats.gens));
	fprintf(f, "bad nodes = %d\n", stats.bads);
}

static void
stats_handler(void)
{
	fprintf(stderr, "========= intermediate stats ===========\n");
	print_stats(stderr);
	fprintf(stderr, "========================================\n");
}

static int
on_automorphism(int n, const int *gamma, int k, int *support, void *arg)
{
	struct amorph_graph *g = arg;
	if (!quiet_mode) {
		qsort_integers(support, k);
		if (gap_mode) {
			putchar(!first ? '[' : ',');
			putchar('\n');
			first = 1;
		}
		g->consumer(n, gamma, k, support, g, marks);
	}
	return !timeout_flag;
}

int
main(int argc, char **argv)
{
	struct saucy *s;
	struct amorph_graph *g = NULL;
	long cpu_time;
	int i, n;
	FILE *f;

	/* Option handling */
	parse_arguments(&argc, &argv, options);
	if (argc < 1) die("missing filename");
	if (argc > 1) die("trailing arguments");
	filename = *argv;

	/* Repeating is for benchmarking */
	if (repeat > 1) quiet_mode = stats_mode = 1;
	if (gap_mode + cnf_mode + digraph_mode > 1) {
		die("--cnf, --digraph, and --shatter are mutually exclusive");
	}

	/* Read the input file */
	if (gap_mode) {
		g = amorph_read_gap(filename);
	}
	else if (cnf_mode) {
		g = amorph_read_dimacs(filename);
	}
	else {
		g = amorph_read(filename, digraph_mode);
	}
	if (!g) {
		die("unable to read input file");
	}
	n = g->sg.n;

	/* Allocate some memory to facilitate printing */
	marks = calloc(n, sizeof(char));
	if (!marks) {
		die("out of memory");
	}

	/* Allocate saucy space */
	s = saucy_alloc(n);
	if (s == NULL) {
		die("saucy initialization failed");
	}

	/* Set up the alarm for timeouts */
	if (timeout > 0) {
		platform_set_timer(timeout, timeout_handler);
	}

	/* Print statistics when signaled */
	platform_set_user_signal(stats_handler);

	/* Start timing */
	cpu_time = platform_clock();

	/* Run the search */
	for (i = 0; i < repeat; ++i) {
		saucy_search(s, &g->sg, digraph_mode, g->colors,
			on_automorphism, g, &stats);
	}

	/* Finish timing */
	cpu_time = platform_clock() - cpu_time;

	if (gap_mode && !quiet_mode) printf("\n]\n");

	/* Warn if timeout */
	if (timeout_flag) warn("search timed out");

	/* Print out stats if requested */
	if (stats_mode) {
		fflush(stdout);
		f = quiet_mode ? stdout : stderr;
		fprintf(f, "input file = %s\n", filename);
		if (g->stats) g->stats(g, f);
		fprintf(f, "vertices = %d\n", n);
		fprintf(f, "edges = %d\n", g->sg.e);
		print_stats(f);
		fprintf(f, "cpu time (s) = %.2f\n",
			divide(cpu_time, PLATFORM_CLOCKS_PER_SEC));
	}

	/* Cleanup */
	saucy_free(s);
	g->free(g);
	free(marks);

	/* That's it, have a nice day */
	return EXIT_SUCCESS;
}
