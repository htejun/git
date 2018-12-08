#include "builtin.h"
#include "cache.h"
#include "repository.h"
#include "config.h"
#include "trailer.h"
#include "revision.h"
#include "parse-options.h"

static const char * const reverse_trailer_xrefs_usage[] = {
	N_("git reverse_trailer_xrefs --xref-cherry-picks [--clear] [<options>] [<commit-ish>...]"),
	N_("git reverse_trailer_xrefs --trailer-prefix=<prefix> --notes=<notes-ref> [--tag=<tag>] [<options>] [<commit-ish>...]"),
	N_("git reverse_trailer_xrefs --notes=<notes-ref> --clear [<options>] [<commit-ish>...]"),
	NULL
};

int cmd_reverse_trailer_xrefs(int argc, const char **argv, const char *prefix)
{
	struct rev_info revs;
	struct setup_revision_opt s_r_opt = {
		.def = "HEAD",
		.revarg_opt = REVARG_CANNOT_BE_FILENAME
	};
	int cherry = 0, clear = 0, verbose = 0;
	struct reverse_trailer_xrefs_args rtx_args = { };
	struct option options[] = {
		OPT_BOOL(0, "xref-cherry-picks", &cherry, N_("use preset for xref-cherry-picks notes")),
		OPT_STRING(0, "trailer-prefix", &rtx_args.trailer_prefix, N_("prefix"), N_("process trailers starting with <prefix>")),
		OPT_STRING(0, "notes", &rtx_args.notes_ref, N_("notes-ref"), N_("update notes in <notes-ref>")),
		OPT_STRING(0, "tag", &rtx_args.tag, N_("tag"), N_("tag xref notes with <tag>")),
		OPT_BOOL(0, "clear", &clear, N_("clear trailer xref notes from the specified commits")),
		OPT__VERBOSE(&verbose, N_("verbose")),
		OPT_END()
	};

	git_config(git_default_config, NULL);

	argc = parse_options(argc, argv, prefix, options,
			     reverse_trailer_xrefs_usage,
			     PARSE_OPT_KEEP_ARGV0 | PARSE_OPT_KEEP_UNKNOWN);

	init_revisions(&revs, prefix);
	argc = setup_revisions(argc, argv, &revs, &s_r_opt);

	if (cherry)
		rtx_args = reverse_cherry_pick_xrefs_args;

	if (!rtx_args.notes_ref || (!clear && !rtx_args.trailer_prefix))
		die(_("insufficient arguments"));

	if (argc > 1)
		die(_("unrecognized argument: %s"), argv[1]);

	if (clear)
		return clear_reverse_trailer_xrefs(&rtx_args, &revs, verbose);
	else
		return record_reverse_trailer_xrefs(&rtx_args, &revs, verbose);
}
