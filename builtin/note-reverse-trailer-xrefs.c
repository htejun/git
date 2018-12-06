#include "builtin.h"
#include "cache.h"
#include "strbuf.h"
#include "repository.h"
#include "config.h"
#include "commit.h"
#include "blob.h"
#include "notes.h"
#include "notes-utils.h"
#include "trailer.h"
#include "revision.h"
#include "list-objects.h"
#include "object-store.h"
#include "parse-options.h"

static const char * const note_reverse_trailer_xrefs_usage[] = {
	N_("git note_reverse_trailer_xrefs [<options>] [<commit-ish>...]"),
	NULL
};

static const char cherry_picked_prefix[] = "(cherry picked from commit ";
static int verbose;

static void clear_trailer_xref_note(struct commit *commit, void *data)
{
	struct notes_tree *tree = data;
	int status;

	status = remove_note(tree, commit->object.oid.hash);

	if (verbose) {
		if (status)
			fprintf(stderr, "Object %s has no note\n",
				oid_to_hex(&commit->object.oid));
		else
			fprintf(stderr, "Removing note for object %s\n",
				oid_to_hex(&commit->object.oid));
	}
}

static void record_trailer_xrefs(struct commit *commit, void *data)
{
	trailer_rev_xrefs_record(data, commit);
}

static int note_trailer_xrefs(struct notes_tree *tree,
			      struct commit *from_commit, struct object_array *to_objs,
			      const char *tag)
{
	char from_hex[GIT_MAX_HEXSZ + 1];
	struct strbuf note = STRBUF_INIT;
	struct object_id note_oid;
	int i, ret;

	oid_to_hex_r(from_hex, &from_commit->object.oid);

	for (i = 0; i < to_objs->nr; i++) {
		const char *hex = to_objs->objects[i].name;

		if (tag)
			strbuf_addf(&note, "%s: %s\n", tag, hex);
		else
			strbuf_addf(&note, "%s\n", tag);
		if (verbose)
			fprintf(stderr, "Adding note %s -> %s\n", from_hex, hex);
	}

	ret = write_object_file(note.buf, note.len, blob_type, &note_oid);
	strbuf_release(&note);
	if (ret)
		return ret;

	ret = add_note(tree, &from_commit->object.oid, &note_oid, NULL);
	return ret;
}

int cmd_note_reverse_trailer_xrefs(int argc, const char **argv,
				   const char *prefix)
{
	static struct notes_tree tree;
	struct rev_info revs;
	int i, ret;
	struct setup_revision_opt s_r_opt = {
		.def = "HEAD",
		.revarg_opt = REVARG_CANNOT_BE_FILENAME
	};
	int cherry = 0, clear = 0;
	const char *trailer_prefix = NULL, *notes_ref = NULL, *tag = NULL;
	struct option options[] = {
		OPT_BOOL(0, "xref-cherry-picks", &cherry, N_("use options for xref-cherry-picks notes")),
		OPT_STRING(0, "trailer-prefix", &trailer_prefix, N_("prefix"), N_("process trailers starting with <prefix>")),
		OPT_STRING(0, "ref", &notes_ref, N_("notes-ref"), N_("update notes in <notes-ref>")),
		OPT_STRING(0, "tag", &tag, N_("tag"), N_("tag xref notes with <tag>")),
		OPT_BOOL(0, "clear", &clear, N_("clear trailer xref notes from the specified commits")),
		OPT__VERBOSE(&verbose, N_("verbose")),
		OPT_END()
	};

	git_config(git_default_config, NULL);

	init_revisions(&revs, prefix);
	argc = setup_revisions(argc, argv, &revs, &s_r_opt);
	argc = parse_options(argc, argv, prefix, options,
			     note_reverse_trailer_xrefs_usage, 0);

	/* allow inidividual options to override parts of --cherry */
	if (cherry) {
		if (!trailer_prefix)
			trailer_prefix = cherry_picked_prefix;
		if (!notes_ref)
			notes_ref = NOTES_CHERRY_PICKS_REF;
		if (!tag)
			tag = NOTES_CHERRY_PICKED_TO_TAG;
	}

	if (!notes_ref || (!clear && (!trailer_prefix || !tag)))
		die(_("insufficient arguments"));

	if (argc > 1)
		die(_("unrecognized argument: %s"), argv[1]);

	if (!tree.initialized)
		init_notes(&tree, notes_ref, NULL, NOTES_INIT_WRITABLE);

	if (prepare_revision_walk(&revs))
		die("revision walk setup failed");

	if (clear) {
		traverse_commit_list(&revs, clear_trailer_xref_note, NULL, &tree);
	} else {
		struct trailer_rev_xrefs rxrefs;
		struct commit *from_commit;
		struct object_array *to_objs;

		trailer_rev_xrefs_init(&rxrefs, trailer_prefix);
		traverse_commit_list(&revs, record_trailer_xrefs, NULL, &rxrefs);

		trailer_rev_xrefs_for_each(&rxrefs, i, from_commit, to_objs) {
			ret = note_trailer_xrefs(&tree, from_commit, to_objs,
						 tag);
			if (ret)
				return ret;
		}
	}

	commit_notes(&tree, "Notes updated by 'git note-reverse-trailer-xrefs'");

	return 0;
}
