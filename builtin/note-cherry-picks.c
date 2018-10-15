#include "cache.h"
#include "config.h"
#include "revision.h"
#include "list-objects.h"
#include "parse-options.h"

static const char * const note_cherry_picks_usage[] = {
	N_("git note-cherry-picks [<options>] [<commit-ish>...]"),
	NULL
};

static int verbose, override, clear;

static void note_commit(struct commit *commit, void *unused)
{
	printf("%s\n", oid_to_hex(&commit->object.oid));
}

int cmd_note_cherry_picks(int argc, const char **argv, const char *prefix)
{
	struct rev_info revs;
	struct setup_revision_opt s_r_opt = {
		.def = "HEAD",
		.revarg_opt = REVARG_CANNOT_BE_FILENAME
	};
	struct option options[] = {
		OPT_BOOL(0, "override", &override, N_("override existing cherry-pick notes")),
		OPT_BOOL(0, "clear", &clear, N_("clear cherry-pick notes from the specified commits")),
		OPT__VERBOSE(&verbose, N_("verbose")),
		OPT_END()
	};

	git_config(git_default_config, NULL);

	init_revisions(&revs, prefix);
	argc = setup_revisions(argc, argv, &revs, &s_r_opt);
	argc = parse_options(argc, argv, prefix, options, note_cherry_picks_usage, 0);
	if (argc > 1)
		die(_("unrecognized argument: %s"), argv[1]);

	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");

	traverse_commit_list(&revs, note_commit, NULL, NULL);

	return 0;
}
