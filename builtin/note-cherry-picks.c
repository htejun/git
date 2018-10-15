#include "builtin.h"
#include "cache.h"
#include "repository.h"
#include "config.h"
#include "commit.h"
#include "trailer.h"
#include "revision.h"
#include "commit-slab.h"
#include "list-objects.h"
#include "object-store.h"
#include "parse-options.h"

define_commit_slab(commit_cherry_picks, struct object_array *);

static const char * const note_cherry_picks_usage[] = {
	N_("git note-cherry-picks [<options>] [<commit-ish>...]"),
	NULL
};

static const char cherry_picked_prefix[] = "(cherry picked from commit ";
static int verbose, override, clear;
static struct object_array cherry_picked = OBJECT_ARRAY_INIT;
static struct commit_cherry_picks cherry_picks;

static struct object_array *get_commit_cherry_picks(struct commit *commit)
{
	struct object_array **slot =
		commit_cherry_picks_peek(&cherry_picks, commit);

	return slot ? *slot : NULL;
}

static struct object_array *get_create_commit_cherry_picks(struct commit *commit)
{
	struct object_array **slot =
		commit_cherry_picks_at(&cherry_picks, commit);

	if (!*slot) {
		*slot = xmalloc(sizeof(struct object_array));
		**slot = (struct object_array)OBJECT_ARRAY_INIT;
	}
	return *slot;
}

static void record_cherry_pick(struct commit *commit, void *unused)
{
	struct process_trailer_options opts = PROCESS_TRAILER_OPTIONS_INIT;
	enum object_type type;
	unsigned long size;
	void *buffer;
	struct trailer_info info;
	int i;

	buffer = read_object_file(&commit->object.oid, &type, &size);
	trailer_info_get(&info, buffer, &opts);

	/* when nested, the last trailer describes the latest cherry-pick */
	for (i = info.trailer_nr - 1; i >= 0; i--) {
		const int prefix_len = sizeof(cherry_picked_prefix) - 1;
		char *line = info.trailers[i];

		if (!strncmp(line, cherry_picked_prefix, prefix_len)) {
			struct object_id from_oid;
			struct object *from_object;
			struct commit *from_commit;
			struct object_array *from_cps;
			char commit_hex[GIT_MAX_HEXSZ + 1];
			char from_hex[GIT_MAX_HEXSZ + 1];

			if (get_oid_hex(line + prefix_len, &from_oid))
				continue;

			from_object = parse_object(the_repository, &from_oid);
			if (!from_object || from_object->type != OBJ_COMMIT)
				continue;

			from_commit = (struct commit *)from_object;
			from_cps = get_create_commit_cherry_picks(from_commit);

			oid_to_hex_r(commit_hex, &commit->object.oid);
			oid_to_hex_r(from_hex, &from_commit->object.oid);

			if (!object_array_contains_name(from_cps, commit_hex)) {
				add_object_array(&commit->object, commit_hex,
						 from_cps);
				add_object_array(&from_commit->object, from_hex,
						 &cherry_picked);
				record_cherry_pick(from_commit, unused);
			}
			break;
		}
	}

	free(buffer);
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
	argc = parse_options(argc, argv, prefix, options,
			     note_cherry_picks_usage, 0);
	if (argc > 1)
		die(_("unrecognized argument: %s"), argv[1]);

	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");

	init_commit_cherry_picks(&cherry_picks);
	traverse_commit_list(&revs, record_cherry_pick, NULL, NULL);

	return 0;
}
