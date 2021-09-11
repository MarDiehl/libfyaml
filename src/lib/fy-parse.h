/*
 * fy-parse.h - YAML parser internal header file
 *
 * Copyright (c) 2019 Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef FY_PARSE_H
#define FY_PARSE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <libfyaml.h>

#include "fy-ctype.h"
#include "fy-utf8.h"
#include "fy-list.h"
#include "fy-typelist.h"
#include "fy-types.h"
#include "fy-diag.h"
#include "fy-dump.h"
#include "fy-atom.h"
#include "fy-input.h"
#include "fy-ctype.h"
#include "fy-token.h"
#include "fy-event.h"
#include "fy-docstate.h"
#include "fy-doc.h"
#include "fy-emit.h"
#include "fy-accel.h"
#include "fy-emit-accum.h"

struct fy_parser;
struct fy_input;

enum fy_flow_type {
	FYFT_NONE,
	FYFT_MAP,
	FYFT_SEQUENCE,
};

struct fy_flow {
	struct list_head node;
	enum fy_flow_type flow;
	int pending_complex_key_column;
	struct fy_mark pending_complex_key_mark;
	int parent_indent;
};
FY_PARSE_TYPE_DECL(flow);

struct fy_indent {
	struct list_head node;
	int indent;
	bool generated_block_map : 1;
};
FY_PARSE_TYPE_DECL(indent);

struct fy_token;

struct fy_simple_key {
	struct list_head node;
	struct fy_mark mark;
	struct fy_mark end_mark;
	struct fy_token *token;	/* associated token */
	int flow_level;
	bool required : 1;
	bool implicit_complex : 1;
};
FY_PARSE_TYPE_DECL(simple_key);

struct fy_document_state;

enum fy_parser_state {
	/** none state */
	FYPS_NONE,
	/** Expect STREAM-START. */
	FYPS_STREAM_START,
	/** Expect the beginning of an implicit document. */
	FYPS_IMPLICIT_DOCUMENT_START,
	/** Expect DOCUMENT-START. */
	FYPS_DOCUMENT_START,
	/** Expect the content of a document. */
	FYPS_DOCUMENT_CONTENT,
	/** Expect DOCUMENT-END. */
	FYPS_DOCUMENT_END,
	/** Expect a block node. */
	FYPS_BLOCK_NODE,
	/** Expect the first entry of a block sequence. */
	FYPS_BLOCK_SEQUENCE_FIRST_ENTRY,
	/** Expect an entry of a block sequence. */
	FYPS_BLOCK_SEQUENCE_ENTRY,
	/** Expect an entry of an indentless sequence. */
	FYPS_INDENTLESS_SEQUENCE_ENTRY,
	/** Expect the first key of a block mapping. */
	FYPS_BLOCK_MAPPING_FIRST_KEY,
	/** Expect a block mapping key. */
	FYPS_BLOCK_MAPPING_KEY,
	/** Expect a block mapping value. */
	FYPS_BLOCK_MAPPING_VALUE,
	/** Expect the first entry of a flow sequence. */
	FYPS_FLOW_SEQUENCE_FIRST_ENTRY,
	/** Expect an entry of a flow sequence. */
	FYPS_FLOW_SEQUENCE_ENTRY,
	/** Expect a key of an ordered mapping. */
	FYPS_FLOW_SEQUENCE_ENTRY_MAPPING_KEY,
	/** Expect a value of an ordered mapping. */
	FYPS_FLOW_SEQUENCE_ENTRY_MAPPING_VALUE,
	/** Expect the and of an ordered mapping entry. */
	FYPS_FLOW_SEQUENCE_ENTRY_MAPPING_END,
	/** Expect the first key of a flow mapping. */
	FYPS_FLOW_MAPPING_FIRST_KEY,
	/** Expect a key of a flow mapping. */
	FYPS_FLOW_MAPPING_KEY,
	/** Expect a value of a flow mapping. */
	FYPS_FLOW_MAPPING_VALUE,
	/** Expect an empty value of a flow mapping. */
	FYPS_FLOW_MAPPING_EMPTY_VALUE,
	/** Expect only stream end */
	FYPS_SINGLE_DOCUMENT_END,
	/** Expect nothing. */
	FYPS_END
};

struct fy_parse_state_log {
	struct list_head node;
	enum fy_parser_state state;
};
FY_PARSE_TYPE_DECL(parse_state_log);

FY_TYPE_FWD_DECL_LIST(path_component);

enum fy_path_component_type {
	FYPCT_NONE,	/* not yet instantiated */
	FYPCT_MAP,	/* it's a mapping */
	FYPCT_SEQ,	/* it's a sequence */
};

/* fwd declaration */
struct fy_document;
struct fy_document_builder;

struct fy_path_mapping_state {
	bool got_key;
	bool is_complex_key;
	struct fy_token *key;			/* simple scalar key */
	struct fy_document_builder *fydb;	/* for complex keys */
	struct fy_document *fyd;
	char *complex_text;
	size_t complex_size;
};

struct fy_path_sequence_state {
	int idx;
	/* max 64 bit 18446744073709551616 */
	int bufidx;	/* the index the buffer was generated for */
	char buf[22];	/* enough for 64 bit sequence numbers */
	size_t buflen;
};

struct fy_path_component {
	struct fy_path *fypp;
	struct list_head node;
	struct fy_token *tag;
	struct fy_token *anchor;
	enum fy_path_component_type type;
	union {
		struct fy_path_mapping_state map;
		struct fy_path_sequence_state seq;
	};
};
FY_PARSE_TYPE_DECL_AFTER_FWD(path_component);

struct fy_path_cfg {
	struct fy_diag *diag;	/* optional diag */
	struct fy_parser *fyp;	/* optional parser we associate with */
};

struct fy_path {
	struct fy_path_cfg cfg;

	int count;
	struct fy_path_component_list components;
	uint64_t seq;		/* the sequence for list */
	uint64_t textseq;	/* the sequence for the text repr */

	struct fy_emit_accum ea;
	char ea_inplace_buf[256];	/* the in place accumulator buffer before allocating */
};

int fy_path_setup(struct fy_path *fypp, const struct fy_path_cfg *cfg);
void fy_path_cleanup(struct fy_path *fypp);
struct fy_path *fy_path_create(const struct fy_path_cfg *cfg);
void fy_path_destroy(struct fy_path *fypp);

void fy_path_reset(struct fy_path *fypp);

struct fy_path_component *fy_path_component_alloc(struct fy_path *fypp);
void fy_path_component_cleanup(struct fy_path_component *fypc);
void fy_path_component_free(struct fy_path_component *fypc);
void fy_path_component_destroy(struct fy_path_component *fypc);

struct fy_path_component *fy_path_component_create_mapping(struct fy_path *fypp);
struct fy_path_component *fy_path_component_create_sequence(struct fy_path *fypp);

struct fy_parser {
	struct fy_parse_cfg cfg;

	struct fy_input_list queued_inputs;	/* all the inputs queued */
	struct fy_reader builtin_reader;	/* the builtin reader */
	struct fy_reader *reader;		/* the external reader, or ptr to builtin_reader */

	struct fy_version default_version;
	bool suppress_recycling : 1;
	bool stream_start_produced : 1;
	bool stream_end_produced : 1;
	bool stream_end_reached : 1;
	bool simple_key_allowed : 1;
	bool stream_error : 1;
	bool generated_block_map : 1;
	bool last_was_comma : 1;
	bool document_has_content : 1;
	bool document_first_content_token : 1;
	bool bare_document_only : 1;		/* no document start indicators allowed, no directives */
	bool stream_has_content : 1;
	bool parse_flow_only : 1;	/* document is in flow form, and stop parsing at the end */
	bool colon_follows_colon : 1;	/* "foo"::bar -> "foo": :bar */
	int flow_level;
	int pending_complex_key_column;
	struct fy_mark pending_complex_key_mark;
	int last_block_mapping_key_line;
	struct fy_mark last_comma_mark;

	/* copy of stream_end token */
	struct fy_token *stream_end_token;

	/* produced tokens, but not yet consumed */
	struct fy_token_list queued_tokens;
	int token_activity_counter;

	/* last comment */
	struct fy_atom last_comment;

	/* indent stack */
	struct fy_indent_list indent_stack;
	int indent;
	int parent_indent;
	/* simple key stack */
	struct fy_simple_key_list simple_keys;
	/* state stack */
	enum fy_parser_state state;
	struct fy_parse_state_log_list state_stack;

	/* current path */
	struct fy_path path;

	/* current parse document */
	struct fy_document_state *current_document_state;
	struct fy_document_state *default_document_state;
	bool next_single_document;

	/* flow stack */
	enum fy_flow_type flow;
	struct fy_flow_list flow_stack;

	/* recycling lists */
	struct fy_indent_list recycled_indent;
	struct fy_simple_key_list recycled_simple_key;
	struct fy_parse_state_log_list recycled_parse_state_log;
	struct fy_eventp_list recycled_eventp;
	struct fy_flow_list recycled_flow;
	struct fy_path_component_list recycled_path_component;

	/* the diagnostic object */
	struct fy_diag *diag;

	int err_term_width;
	int err_term_height;
};

static inline struct fy_input *
fyp_current_input(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_current_input(fyp->reader);
}

static inline uint64_t
fyp_current_input_generation(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_current_input_generation(fyp->reader);
}

static inline int fyp_column(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_column(fyp->reader);
}

static inline int fyp_line(const struct fy_parser *fyp)
{
	return fy_reader_line(fyp->reader);
}

static inline int fyp_tabsize(const struct fy_parser *fyp)
{
	return fy_reader_tabsize(fyp->reader);
}

static inline bool fyp_json_mode(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_json_mode(fyp->reader);
}

static inline enum fy_lb_mode fyp_lb_mode(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_lb_mode(fyp->reader);
}

static inline enum fy_flow_ws_mode fyp_fws_mode(const struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_flow_ws_mode(fyp->reader);
}

static inline bool fyp_is_lb(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_lb(fyp->reader, c);
}

static inline bool fyp_is_lbz(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_lbz(fyp->reader, c);
}

static inline bool fyp_is_blankz(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_blankz(fyp->reader, c);
}

static inline bool fyp_is_generic_lb(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_generic_lb(fyp->reader, c);
}

static inline bool fyp_is_generic_lbz(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_generic_lbz(fyp->reader, c);
}

static inline bool fyp_is_generic_blankz(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_generic_blankz(fyp->reader, c);
}

static inline bool fyp_is_flow_ws(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_flow_ws(fyp->reader, c);
}

static inline bool fyp_is_flow_blank(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_flow_blank(fyp->reader, c);
}

static inline bool fyp_is_flow_blankz(const struct fy_parser *fyp, int c)
{
	assert(fyp);
	return fy_reader_is_flow_blankz(fyp->reader, c);
}

static inline const void *
fy_ptr_slow_path(struct fy_parser *fyp, size_t *leftp)
{
	assert(fyp);
	return fy_reader_ptr_slow_path(fyp->reader, leftp);
}

static inline const void *
fy_ensure_lookahead_slow_path(struct fy_parser *fyp, size_t size, size_t *leftp)
{
	assert(fyp);
	return fy_reader_ensure_lookahead_slow_path(fyp->reader, size, leftp);
}

/* only allowed if input does not update */
static inline void
fy_get_mark(struct fy_parser *fyp, struct fy_mark *fym)
{
	assert(fyp);
	return fy_reader_get_mark(fyp->reader, fym);
}

static inline const void *
fy_ptr(struct fy_parser *fyp, size_t *leftp)
{
	assert(fyp);
	return fy_reader_ptr(fyp->reader, leftp);
}

static inline const void *
fy_ensure_lookahead(struct fy_parser *fyp, size_t size, size_t *leftp)
{
	assert(fyp);
	return fy_reader_ensure_lookahead(fyp->reader, size, leftp);
}

/* advance the given number of ascii characters, not utf8 */
static inline void
fy_advance_octets(struct fy_parser *fyp, size_t advance)
{
	assert(fyp);
	return fy_reader_advance_octets(fyp->reader, advance);
}

/* compare string at the current point (n max) */
static inline int
fy_parse_strncmp(struct fy_parser *fyp, const char *str, size_t n)
{
	assert(fyp);
	return fy_reader_strncmp(fyp->reader, str, n);
}

static inline int
fy_parse_peek_at_offset(struct fy_parser *fyp, size_t offset)
{
	assert(fyp);
	return fy_reader_peek_at_offset(fyp->reader, offset);
}

static inline int
fy_parse_peek_at_internal(struct fy_parser *fyp, int pos, ssize_t *offsetp)
{
	assert(fyp);
	return fy_reader_peek_at_internal(fyp->reader, pos, offsetp);
}

static inline bool
fy_is_blank_at_offset(struct fy_parser *fyp, size_t offset)
{
	assert(fyp);
	return fy_is_blank(fy_reader_peek_at_offset(fyp->reader, offset));
}

static inline bool
fy_is_blankz_at_offset(struct fy_parser *fyp, size_t offset)
{
	assert(fyp);
	return fy_reader_is_blankz(fyp->reader, fy_reader_peek_at_offset(fyp->reader, offset));
}

static inline bool
fy_is_generic_blankz_at_offset(struct fy_parser *fyp, size_t offset)
{
	assert(fyp);
	return fy_reader_is_generic_blankz(fyp->reader, fy_reader_peek_at_offset(fyp->reader, offset));
}

static inline int
fy_parse_peek_at(struct fy_parser *fyp, int pos)
{
	assert(fyp);
	return fy_reader_peek_at_internal(fyp->reader, pos, NULL);
}

static inline int
fy_parse_peek(struct fy_parser *fyp)
{
	assert(fyp);
	return fy_parse_peek_at_offset(fyp, 0);
}

static inline void
fy_advance(struct fy_parser *fyp, int c)
{
	assert(fyp);
	fy_reader_advance(fyp->reader, c);
}

static inline int
fy_parse_get(struct fy_parser *fyp)
{
	assert(fyp);
	return fy_reader_get(fyp->reader);
}

static inline int
fy_advance_by(struct fy_parser *fyp, int count)
{
	assert(fyp);
	return fy_reader_advance_by(fyp->reader, count);
}

/* compare string at the current point */
static inline bool
fy_parse_strcmp(struct fy_parser *fyp, const char *str)
{
	assert(fyp);
	return fy_reader_strcmp(fyp->reader, str);
}

static inline void
fy_fill_atom_start(struct fy_parser *fyp, struct fy_atom *handle)
{
	assert(fyp);
	fy_reader_fill_atom_start(fyp->reader, handle);
}

static inline void
fy_fill_atom_end_at(struct fy_parser *fyp, struct fy_atom *handle, struct fy_mark *end_mark)
{
	assert(fyp);
	fy_reader_fill_atom_end_at(fyp->reader, handle, end_mark);
}

static inline void
fy_fill_atom_end(struct fy_parser *fyp, struct fy_atom *handle)
{
	assert(fyp);
	fy_reader_fill_atom_end(fyp->reader, handle);
}

static inline struct fy_atom *
fy_fill_atom(struct fy_parser *fyp, int advance, struct fy_atom *handle)
{
	assert(fyp);
	return fy_reader_fill_atom(fyp->reader, advance, handle);
}

static inline struct fy_atom *
fy_fill_atom_mark(struct fy_parser *fyp, const struct fy_mark *start_mark,
		  const struct fy_mark *end_mark, struct fy_atom *handle)
{
	assert(fyp);
	return fy_reader_fill_atom_mark(fyp->reader, start_mark, end_mark, handle);
}

static inline struct fy_atom *
fy_fill_atom_at(struct fy_parser *fyp, int advance, int count, struct fy_atom *handle)
{
	assert(fyp);
	return fy_reader_fill_atom_at(fyp->reader, advance, count, handle);
}

static inline void
fy_parser_set_reader(struct fy_parser *fyp, struct fy_reader *fyr)
{
	if (!fyp)
		return;
	fyp->reader = fyr ? : &fyp->builtin_reader;
}

static inline void
fy_parser_set_flow_only_mode(struct fy_parser *fyp, bool flow_only_mode)
{
	fyp->parse_flow_only = flow_only_mode;
}

#define fy_fill_atom_a(_fyp, _advance) \
	fy_fill_atom((_fyp), (_advance), alloca(sizeof(struct fy_atom)))

struct fy_token *fy_token_vqueue(struct fy_parser *fyp, enum fy_token_type type, va_list ap);
struct fy_token *fy_token_queue(struct fy_parser *fyp, enum fy_token_type type, ...);

struct fy_token *
fy_token_vqueue_internal(struct fy_parser *fyp, struct fy_token_list *fytl,
			 enum fy_token_type type, va_list ap);
struct fy_token *
fy_token_queue_internal(struct fy_parser *fyp, struct fy_token_list *fytl,
			enum fy_token_type type, ...);

int fy_parse_setup(struct fy_parser *fyp, const struct fy_parse_cfg *cfg);
void fy_parse_cleanup(struct fy_parser *fyp);

int fy_parse_input_append(struct fy_parser *fyp, const struct fy_input_cfg *fyic);

struct fy_eventp *fy_parse_private(struct fy_parser *fyp);

extern const char *fy_event_type_txt[];

enum fy_parse_cfg_flags fy_parser_get_cfg_flags(const struct fy_parser *fyp);

extern const struct fy_tag * const fy_default_tags[];

extern const struct fy_version fy_default_version;	/* usually highest stable */

bool fy_tag_handle_is_default(const char *handle, size_t handle_size);
bool fy_tag_is_default_internal(const char *handle, size_t handle_size,
				const char *prefix, size_t prefix_size);
bool fy_token_tag_directive_is_overridable(struct fy_token *fyt_td);

int fy_parser_set_default_document_state(struct fy_parser *fyp,
					 struct fy_document_state *fyds);
void fy_parser_set_next_single_document(struct fy_parser *fyp);

void *fy_alloc_default(void *userdata, size_t size);
void fy_free_default(void *userdata, void *ptr);
void *fy_realloc_default(void *userdata, void *ptr, size_t size);

int fy_reader_fetch_flow_scalar_handle(struct fy_reader *fyr, int c, int indent, struct fy_atom *handle, bool sloppy_indent);
int fy_reader_fetch_plain_scalar_handle(struct fy_reader *fyr, int c, int indent, int flow_level, struct fy_atom *handle, bool directive0);

static inline int fy_document_state_version_compare(struct fy_document_state *fyds, const struct fy_version *vb)
{
	return fy_version_compare(fy_document_state_version(fyds), vb);
}

int fy_path_setup(struct fy_path *fypp, const struct fy_path_cfg *cfg);
void fy_path_cleanup(struct fy_path *fypp);
struct fy_path *fy_path_create(const struct fy_path_cfg *cfg);
void fy_path_destroy(struct fy_path *fypp);

void fy_path_reset(struct fy_path *fypp);

struct fy_path_component *fy_path_component_alloc(struct fy_path *fypp);
void fy_path_component_cleanup(struct fy_path_component *fypc);
void fy_path_component_free(struct fy_path_component *fypc);
void fy_path_component_destroy(struct fy_path_component *fypc);

struct fy_path_component *fy_path_component_create_mapping(struct fy_path *fypp);
struct fy_path_component *fy_path_component_create_sequence(struct fy_path *fypp);

void fy_path_component_set_tag(struct fy_path_component *fypc, struct fy_token *tag);
void fy_path_component_set_anchor(struct fy_path_component *fypc, struct fy_token *anchor);

bool fy_path_component_is_complete(struct fy_path_component *fypc);

const char *fy_path_component_get_text(struct fy_path_component *fypc, size_t *lenp);
const char *fy_path_component_get_text0(struct fy_path_component *fypc);

int fy_path_rebuild(struct fy_path *fypp);

const char *fy_path_get_text(struct fy_path *fypp, size_t *lenp);
const char *fy_path_get_text0(struct fy_path *fypp);

struct fy_path *fy_parse_path(struct fy_parser *fyp);

int fy_parse_path_event(struct fy_parser *fyp, struct fy_eventp *fyep);

#endif
