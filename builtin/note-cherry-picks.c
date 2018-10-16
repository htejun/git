#include "builtin.h"
#include "cache.h"
#include "strbuf.h"
#include "repository.h"
#include "config.h"
#include "commit.h"
#include "trailer.h"
#include "revision.h"
#include "argv-array.h"
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
			char cherry_hex[GIT_MAX_HEXSZ + 1];
			char from_hex[GIT_MAX_HEXSZ + 1];

			if (get_oid_hex(line + prefix_len, &from_oid))
				continue;

			from_object = parse_object(the_repository, &from_oid);
			if (!from_object || from_object->type != OBJ_COMMIT)
				continue;

			from_commit = (struct commit *)from_object;
			from_cps = get_create_commit_cherry_picks(from_commit);

			oid_to_hex_r(cherry_hex, &commit->object.oid);
			oid_to_hex_r(from_hex, &from_commit->object.oid);

			if (!object_array_contains_name(from_cps, cherry_hex)) {
				add_object_array(&commit->object, cherry_hex,
						 from_cps);
				add_object_array(&from_commit->object, from_hex,
						 &cherry_picked);
			}
			break;
		}
	}

	free(buffer);
}

static void clear_cherry_pick_notes(struct commit *commit, void *prefix)
{
	struct argv_array args;

	argv_array_init(&args);
	argv_array_pushl(&args, "notes", "--ref", "cherry-picks", "remove",
			 "--ignore-missing",
			 oid_to_hex(&commit->object.oid), NULL);
	cmd_notes(args.argc, args.argv, prefix);
}

static void show_cherry_picks(struct object *obj, int level)
{
	struct object_array *cps;
	int i;

	if (obj->type != OBJ_COMMIT)
		return;

	cps = get_commit_cherry_picks((struct commit *)obj);
	if (!cps)
		return;

	for (i = 0; i < cps->nr; i++) {
		struct object *cherry_pick = cps->objects[i].item;
		int j;

		for (j = 0; j < level; j++)
			fputs(" ", stdout);

		printf("%s\n", oid_to_hex(&cherry_pick->oid));
		show_cherry_picks(cherry_pick, level + 1);
	}
}

static int note_cherry_picks(struct commit *commit, const char *prefix)
{
	char from_hex[GIT_MAX_HEXSZ + 1];
	char cherry_hex[GIT_MAX_HEXSZ + 1];
	struct strbuf note = STRBUF_INIT;
	struct argv_array args;
	struct object_array *cps;
	int i, ret;

	cps = get_commit_cherry_picks(commit);
	if (!cps)
		return 0;

	oid_to_hex_r(from_hex, &commit->object.oid);

	for (i = 0; i < cps->nr; i++) {
		struct object *cherry = cps->objects[i].item;

		oid_to_hex_r(cherry_hex, &cherry->oid);
		strbuf_addf(&note, "Cherry-picked-to: %s\n", cherry_hex);
		if (verbose)
			printf("Noting %s -> %s\n", from_hex, cherry_hex);
	}

	argv_array_init(&args);
	argv_array_pushl(&args, "notes", "--ref", "cherry-picks", "add",
			 "--force", "--message", note.buf, from_hex, NULL);
	ret = cmd_notes(args.argc, args.argv, prefix);
	strbuf_release(&note);
	return ret;
}

int cmd_note_cherry_picks(int argc, const char **argv, const char *prefix)
{
	struct rev_info revs;
	int i, ret;
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

	if (clear) {
		traverse_commit_list(&revs, clear_cherry_pick_notes, NULL,
				     (void *)prefix);
		return 0;
	}

	init_commit_cherry_picks(&cherry_picks);
	traverse_commit_list(&revs, record_cherry_pick, NULL, NULL);

	object_array_remove_duplicates(&cherry_picked);

	for (i = 0; i < cherry_picked.nr; i++) {
		ret = note_cherry_picks((void *)cherry_picked.objects[i].item,
					prefix);
		if (ret)
			return ret;
	}

	return 0;
}
