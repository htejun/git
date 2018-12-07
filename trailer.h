#ifndef TRAILER_H
#define TRAILER_H

#include "list.h"
#include "object.h"
#include "commit-slab.h"

struct strbuf;

enum trailer_where {
	WHERE_DEFAULT,
	WHERE_END,
	WHERE_AFTER,
	WHERE_BEFORE,
	WHERE_START
};
enum trailer_if_exists {
	EXISTS_DEFAULT,
	EXISTS_ADD_IF_DIFFERENT_NEIGHBOR,
	EXISTS_ADD_IF_DIFFERENT,
	EXISTS_ADD,
	EXISTS_REPLACE,
	EXISTS_DO_NOTHING
};
enum trailer_if_missing {
	MISSING_DEFAULT,
	MISSING_ADD,
	MISSING_DO_NOTHING
};

int trailer_set_where(enum trailer_where *item, const char *value);
int trailer_set_if_exists(enum trailer_if_exists *item, const char *value);
int trailer_set_if_missing(enum trailer_if_missing *item, const char *value);

struct trailer_info {
	/*
	 * True if there is a blank line before the location pointed to by
	 * trailer_start.
	 */
	int blank_line_before_trailer;

	/*
	 * Pointers to the start and end of the trailer block found. If there
	 * is no trailer block found, these 2 pointers point to the end of the
	 * input string.
	 */
	const char *trailer_start, *trailer_end;

	/*
	 * Array of trailers found.
	 */
	char **trailers;
	size_t trailer_nr;
};

/*
 * A list that represents newly-added trailers, such as those provided
 * with the --trailer command line option of git-interpret-trailers.
 */
struct new_trailer_item {
	struct list_head list;

	const char *text;

	enum trailer_where where;
	enum trailer_if_exists if_exists;
	enum trailer_if_missing if_missing;
};

struct process_trailer_options {
	int in_place;
	int trim_empty;
	int only_trailers;
	int only_input;
	int unfold;
	int no_divider;
};

#define PROCESS_TRAILER_OPTIONS_INIT {0}

void process_trailers(const char *file,
		      const struct process_trailer_options *opts,
		      struct list_head *new_trailer_head);

void trailer_info_get(struct trailer_info *info, const char *str,
		      const struct process_trailer_options *opts);

void trailer_info_release(struct trailer_info *info);

/*
 * Format the trailers from the commit msg "msg" into the strbuf "out".
 * Note two caveats about "opts":
 *
 *   - this is primarily a helper for pretty.c, and not
 *     all of the flags are supported.
 *
 *   - this differs from process_trailers slightly in that we always format
 *     only the trailer block itself, even if the "only_trailers" option is not
 *     set.
 */
void format_trailers_from_commit(struct strbuf *out, const char *msg,
				 const struct process_trailer_options *opts);

/*
 * Helpers to reverse trailers referencing to other commits.
 *
 * Some trailers, e.g. "(cherry picked from...)", references other commits.
 * The following helpers can be used to reverse map those references.  See
 * builtin/reverse-trailer-xrefs.c for a usage example.
 */
declare_commit_slab(trailer_rxrefs_slab, struct object_array *);

struct trailer_rev_xrefs {
	char *trailer_prefix;
	int trailer_prefix_len;
	struct trailer_rxrefs_slab slab;
	struct object_array dst_commits;
};

void trailer_rev_xrefs_init(struct trailer_rev_xrefs *rxrefs,
			    const char *trailer_prefix);
void trailer_rev_xrefs_record(struct trailer_rev_xrefs *rxrefs,
			      struct commit *commit);
void trailer_rev_xrefs_release(struct trailer_rev_xrefs *rxrefs);

void trailer_rev_xrefs_next(struct trailer_rev_xrefs *rxrefs,
			    int *idx_p, struct commit **dst_commit_p,
			    struct object_array **src_objs_p);

/*
 * Iterate the recorded reverse mappings - @dst_commit was pointed to by
 * commits in @src_objs.
 */
#define trailer_rev_xrefs_for_each(rxrefs, idx, dst_commit, src_objs)		\
	for ((idx) = 0,								\
	     trailer_rev_xrefs_next(rxrefs, &(idx), &(dst_commit), &(src_objs));\
	     (dst_commit);							\
	     trailer_rev_xrefs_next(rxrefs, &(idx), &(dst_commit), &(src_objs)))

#endif /* TRAILER_H */
