/*
 *	(c) Copyright 1990, Kim Fabricius Storm.  All rights reserved.
 *      Copyright (c) 1996-2005 Michael T Pins.  All rights reserved.
 *
 *	Variable setting and display
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "global.h"
#include "chset.h"
#include "folder.h"
#include "init.h"
#include "keymap.h"
#include "regexp.h"
#include "sort.h"
#include "nn_term.h"

/* variable.c */

static struct variable_defs *lookup_variable(char *variable);
static char     escaped_char(char c);
static void     adjust(register char *str);
static char    *var_value(register struct variable_defs * var, char *tag);
static int      var_on_stack(register struct variable_defs * var);

extern int      prompt_length, prompt_line;
extern regexp  *pg_regexp;
extern int      pg_new_regexp;


/*
 * Variable types and variants
 *
 * Boolean: type 'int'.
 *
 *	BOOL 0		Ordinary boolean variable.
 *	BOOL 1		As 0 + redraw screen on return.
 *	BOOL 2		Special: reorder menu according to new value.
 *	BOOL 4		Inverse boolean varible ("set" clears internal var).
 *
 * Numeric: type 'int'.
 *
 *	INT 0		Ordinary numeric variable ("unset" set var to 0).
 *	INT 1		As 0 + redraw screen on return.
 *	INT 2		As 0, but "unset" set var to -1.
 *	INT 3		As 2 + redraw screen on return.
 *
 * Strings: type 'char *'
 *
 *	STR 0		Ordinary string ("unset" set var to NULL).
 *	STR 2		(home relative) file name or full path.
 *			Automatically expanded.  ("unset" set var to NULL).
 *	STR 3		Ordinary string, but cannot be "unset".
 *	STR 4		(Expanded) file name - cannot be unset.
 *	STR 5		Ordinary string ("unset" set var to "").
 *
 *	CODES n		String initialized by list of key names.
 *			A maximum of 16 CODES variables (n = 0 - 15) exist.
 *
 * Strings: type 'char []'
 *
 *	STR 1		Ordinary string saved in static array.
 *			"unset" set array to empty string.
 *			Warning: no bounds checking!
 *			Notice: Variable address is "(char **)var"  (no &).
 *
 * Keys: type 'key_type'
 *
 *	KEY 0		Ordinary key.
 *
 * Pseudo variables:
 *
 *	SPEC n		Treated by V_SPECIAL 'case n'.
 *
 * Modifiers:
 *
 *	INIT		Can only be set in init file.
 *	SAFE		Cannot be changed if "shell-restrictions" is set.
 */

extern int      in_init;

extern char			/* string variables */
               *backup_folder_path, *bak_suffix, *bug_address, *counter_delim_left,
               *counter_delim_right, *decode_header_file, *default_distribution,
               *default_save_file, *distribution_follow, *distribution_post,
               *editor_program, *extra_mail_headers, *extra_news_headers,
               *folder_save_file, *header_lines, *folder_directory, included_mark[],
               *inews_program, *initial_newsrc_path, *mail_alias_expander,
               *mail_box, *mail_record, *mail_script, *mailer_program,
                attributes[], *newsrc_file, *news_record, *news_script,
               *pager, patch_command[], printer[], *print_header_lines,
               *response_dflt_answer, *save_counter_format, *save_header_lines,
               *saved_header_escape, *shade_on_attr, *shade_off_attr, *spell_checker,
               *sign_type, *trusted_escapes, unshar_command[], *unshar_header_file,
               *user_shell;

extern int			/* boolean variables */
                also_cross_postings, also_full_digest, also_subgroups,
                append_sig_mail, append_sig_post, auto_junk_seen, auto_select_rw,
                auto_select_subject, auto_preview_mode, body_search_header,
                case_fold_search, check_group_access, compress_mode, conf_append,
                conf_auto_quit, conf_create, conf_dont_sleep, conf_group_entry,
                conf_junk_seen, consolidated_manual, consolidated_menu,
                counter_padding, decode_keep, delay_redraw, dflt_kill_select,
                do_kill_handling, dont_sort_articles, dont_sort_folders,
                dont_split_digests, echo_prefix_key, edit_patch_command,
                edit_print_command, edit_unshar_command, empty_answer_check,
                enter_last_read_mode, flow_control, flush_typeahead, fmt_rptsubj,
                folder_format_check, folder_rewrite_trace, guard_double_slash,
                ignore_formfeed, ignore_xon_xoff, ignore_re, include_art_id,
                include_full_header, include_mark_blanks, inews_pipe_input,
                keep_backup_folder, keep_rc_backup, keep_unsubscribed, keep_unsub_long,
                long_menu, macro_debug, mailer_pipe_input, mark_overlap,
                mark_overlap_shading, match_parts_equal, match_parts_begin,
                monitor_mode, new_read_prompt, novice, old_packname, prev_also_read,
                preview_mark_read, query_signature, quick_save, quick_unread_count,
                read_ret_next_page, repeat_group_query, report_cost_on_exit, retain_seen_status,
                save_report, scroll_clear_page, select_leave_next, select_on_sender,
                seq_cross_filtering, shell_restrictions, show_article_date,
                show_current_time, show_motd_on_entry, silent, slow_mode,
                suggest_save_file, tidy_newsrc, use_ed_line, use_mail_folders,
                use_mmdf_folders, use_path_in_from, use_selections, use_visible_bell;

extern int			/* integer variables */
                also_read_articles, article_limit, auto_read_limit, auto_select_closed,
                check_db_update, conf_entry_limit, collapse_subject, Columns,
                cross_post_limit, data_bits, Debug, decode_skip_prefix,
                entry_message_limit, expired_msg_delay, first_page_lines,
                fmt_linenum, kill_debug, kill_ref_count, Lines, match_skip_prefix,
                mark_next_group, mark_read_return, mark_read_skip, menu_spacing,
                merge_report_rate, message_history, min_pv_window, mouse_usage,
                multi_key_guard_time, new_group_action, newsrc_update_freq,
                orig_to_include_mask, overlap, preview_continuation, preview_window,
                print_header_type, re_layout, re_layout_more, response_check_pause,
                retry_on_error, save_closed_mode, save_counter_offset,
                scroll_last_lines, show_purpose_mode, slow_speed, strict_from_parse,
                sort_mode, subject_match_limit, wrap_headers;

#ifdef NNTP
extern char    *nn_directory;
extern char    *nntp_cache_dir, *nntp_server;
extern char    *nntp_user, *nntp_password;
extern int      nntp_cache_size, nntp_debug;
#endif

extern          key_type	/* key strokes */
                comp1_key, comp2_key, help_key, erase_key, delword_key,
                kill_key;

#undef STR
#undef BOOL
#undef INT
#undef KEY
#undef SPEC
#undef SAFE
#undef INIT
#undef CODES


#define	V_STRING	0x1000
#define V_BOOLEAN	0x2000
#define	V_INTEGER	0x3000
#define V_KEY		0x4000
#define	V_SPECIAL	0x5000
#define V_CODES		0x6000

#define V_SAFE		0x0100
#define V_INIT		0x0200

#define V_LOCKED	0x0800
#define V_MODIFIED	0x8000

#define	STR		V_STRING |
#define BOOL		V_BOOLEAN |
#define	INT		V_INTEGER |
#define KEY		V_KEY |
#define	SPEC		V_SPECIAL |
#define	CODES		V_CODES |

#define SAFE		V_SAFE |
#define INIT		V_INIT |

static char    *code_strings[16] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static int      var_on_stack();

static struct variable_defs {
    char           *var_name;
    int             var_flags;
    char          **var_addr;
}               variables[] = {

    "also-full-digest", BOOL 0, (char **) &also_full_digest,
    "also-subgroups", BOOL INIT 0, (char **) &also_subgroups,
    "append-signature-mail", BOOL 0, (char **) &append_sig_mail,
    "append-signature-post", BOOL 0, (char **) &append_sig_post,
    "attributes", STR 1, (char **) attributes,
    "auto-junk-seen", BOOL 0, (char **) &auto_junk_seen,
    "auto-preview-mode", BOOL 0, (char **) &auto_preview_mode,
    "auto-read-mode-limit", INT 0, (char **) &auto_read_limit,
    "auto-select-closed", INT 0, (char **) &auto_select_closed,
    "auto-select-rw", BOOL 0, (char **) &auto_select_rw,
    "auto-select-subject", BOOL 0, (char **) &auto_select_subject,
    "backup", BOOL 0, (char **) &keep_rc_backup,
    "backup-folder-path", STR 4, (char **) &backup_folder_path,
    "backup-suffix", STR 0, (char **) &bak_suffix,
    "body-search-header", BOOL 0, (char **) &body_search_header,
    "bug-report-address", STR 0, (char **) &bug_address,
    "case-fold-search", BOOL 0, (char **) &case_fold_search,
    "charset", SPEC 3, (char **) NULL,
    "check-db-update-time", INT 0, (char **) &check_db_update,
    "check-group-access", BOOL 0, (char **) &check_group_access,
    "collapse-subject", INT 3, (char **) &collapse_subject,
    "columns", INT 1, (char **) &Columns,
    "comp1-key", KEY 0, (char **) &comp1_key,
    "comp2-key", KEY 0, (char **) &comp2_key,
    "compress", BOOL 0, (char **) &compress_mode,
    "confirm-append", BOOL 0, (char **) &conf_append,
    "confirm-auto-quit", BOOL 0, (char **) &conf_auto_quit,
    "confirm-create", BOOL 0, (char **) &conf_create,
    "confirm-entry", BOOL 0, (char **) &conf_group_entry,
    "confirm-entry-limit", INT 0, (char **) &conf_entry_limit,
    "confirm-junk-seen", BOOL 0, (char **) &conf_junk_seen,
    "confirm-messages", BOOL 0, (char **) &conf_dont_sleep,
    "consolidated-manual", BOOL 0, (char **) &consolidated_manual,
    "consolidated-menu", BOOL 1, (char **) &consolidated_menu,
    "counter-delim-left", STR 5, (char **) &counter_delim_left,
    "counter-delim-right", STR 5, (char **) &counter_delim_right,
    "counter-padding", INT 1, (char **) &counter_padding,
    "cross-filter-seq", BOOL 0, (char **) &seq_cross_filtering,
    "cross-post", BOOL 0, (char **) &also_cross_postings,
    "cross-post-limit", INT 0, (char **) &cross_post_limit,
    "data-bits", INT 0, (char **) &data_bits,
    "date", BOOL 0, (char **) &show_article_date,
    "debug", INT 0, (char **) &Debug,
    "decode-header-file", STR 0, (char **) &decode_header_file,
    "decode-keep", INT 0, (char **) &decode_keep,
    "decode-skip-prefix", INT 0, (char **) &decode_skip_prefix,
    "default-distribution", STR 0, (char **) &default_distribution,
    "default-kill-select", INT 0, (char **) &dflt_kill_select,
    "default-save-file", STR 3, (char **) &default_save_file,
    "delay-redraw", BOOL 0, (char **) &delay_redraw,
    "echo-prefix-key", BOOL 0, (char **) &echo_prefix_key,
    "edit-patch-command", BOOL 0, (char **) &edit_patch_command,
    "edit-print-command", BOOL 0, (char **) &edit_print_command,
    "edit-response-check", BOOL 0, (char **) &empty_answer_check,
    "edit-unshar-command", BOOL 0, (char **) &edit_unshar_command,
    "editor", STR 0, (char **) &editor_program,
    "embedded-header-escape", STR 0, (char **) &saved_header_escape,
    "enter-last-read-mode", INT 0, (char **) &enter_last_read_mode,
    "entry-report-limit", INT 0, (char **) &entry_message_limit,
    "erase-key", KEY 0, (char **) &erase_key,
    "expert", BOOL 4, (char **) &novice,
    "expired-message-delay", INT 0, (char **) &expired_msg_delay,
    "flow-control", BOOL 0, (char **) &flow_control,
    "flush-typeahead", BOOL 0, (char **) &flush_typeahead,
    "folder", STR 2, (char **) &folder_directory,
    "folder-format-check", BOOL 0, (char **) &folder_format_check,
    "folder-save-file", STR 3, (char **) &folder_save_file,
    "follow-distribution", STR 0, (char **) &distribution_follow,
    "from-line-parsing", INT 0, (char **) &strict_from_parse,
    "fsort", BOOL 2, (char **) &dont_sort_folders,
    "guard-double-slash", BOOL 0, (char **) &guard_double_slash,
    "header-lines", STR 0, (char **) &header_lines,
    "help-key", KEY 0, (char **) &help_key,
    "ignore-formfeed", BOOL 0, (char **) &ignore_formfeed,
    "ignore-re", BOOL 0, (char **) &ignore_re,
    "ignore-xon-xoff", BOOL 0, (char **) &ignore_xon_xoff,
    "include-art-id", BOOL 0, (char **) &include_art_id,
    "include-full-header", BOOL 0, (char **) &include_full_header,
    "include-mark-blank-lines", BOOL 0, (char **) &include_mark_blanks,
    "included-mark", STR 1, (char **) included_mark,
    "inews", STR 0, (char **) &inews_program,
    "inews-pipe-input", BOOL 0, (char **) &inews_pipe_input,
    "initial-newsrc-file", STR 0, (char **) &initial_newsrc_path,
    "keep-backup-folder", BOOL 0, (char **) &keep_backup_folder,
    "keep-unsubscribed", BOOL 0, (char **) &keep_unsubscribed,
    "kill", BOOL 0, (char **) &do_kill_handling,
    "kill-debug", BOOL 0, (char **) &kill_debug,
    "kill-key", KEY 0, (char **) &kill_key,
    "kill-reference-count", INT 0, (char **) &kill_ref_count,
    "layout", INT 1, (char **) &fmt_linenum,
    "limit", INT 2, (char **) &article_limit,
    "lines", INT 1, (char **) &Lines,
    "long-menu", BOOL 1, (char **) &long_menu,
    "macro-debug", BOOL 0, (char **) &macro_debug,
    "mail", STR 2, (char **) &mail_box,
    "mail-alias-expander", STR 0, (char **) &mail_alias_expander,
    "mail-format", BOOL 0, (char **) &use_mail_folders,
    "mail-header", STR 0, (char **) &extra_mail_headers,
    "mail-record", STR 2, (char **) &mail_record,
    "mail-script", STR SAFE 2, (char **) &mail_script,
    "mailer", STR 0, (char **) &mailer_program,
    "mailer-pipe-input", BOOL 0, (char **) &mailer_pipe_input,
    "mark-overlap", BOOL 0, (char **) &mark_overlap,
    "mark-overlap-shading", BOOL 0, (char **) &mark_overlap_shading,
    "marked-by-next-group", INT 0, (char **) &mark_next_group,
    "marked-by-read-return", INT 0, (char **) &mark_read_return,
    "marked-by-read-skip", INT 0, (char **) &mark_read_skip,
    "menu-spacing", INT 1, (char **) &menu_spacing,
    "merge-report-rate", INT 0, (char **) &merge_report_rate,
    "message-history", INT 0, (char **) &message_history,
    "min-window", INT 1, (char **) &min_pv_window,
    "mmdf-format", BOOL 0, (char **) &use_mmdf_folders,
    "monitor", BOOL 0, (char **) &monitor_mode,
    "motd", BOOL 0, (char **) &show_motd_on_entry,
    "mouse-usage", INT 0, (char **) &mouse_usage,
    "multi-key-guard-time", INT 0, (char **) &multi_key_guard_time,
    "new-group-action", INT 0, (char **) &new_group_action,
    "new-style-read-prompt", BOOL 0, (char **) &new_read_prompt,
    "news-header", STR 0, (char **) &extra_news_headers,
    "news-record", STR 2, (char **) &news_record,
    "news-script", STR SAFE 2, (char **) &news_script,
    "newsrc", STR 2, (char **) &newsrc_file,
    "nn-directory", STR 2, (char **) &nn_directory,

#ifdef NNTP
    "nntp-cache-dir", STR INIT 0, (char **) &nntp_cache_dir,
    "nntp-cache-size", INT INIT 0, (char **) &nntp_cache_size,
    "nntp-debug", BOOL 0, (char **) &nntp_debug,
    "nntp-password", STR 3, (char **) &nntp_password,
    "nntp-server", STR 3, (char **) &nntp_server,
    "nntp-user", STR 3, (char **) &nntp_user,
#endif

/*  "no....."  -- cannot have variable names starting with "no" */
    "old", SPEC 2, (char **) NULL,
    "old-packname", BOOL 0, (char **) &old_packname,
    "orig-to-include-mask", INT 0, (char **) &orig_to_include_mask,
    "overlap", INT 0, (char **) &overlap,
    "pager", STR SAFE 3, (char **) &pager,
    "patch-command", STR SAFE 1, (char **) patch_command,
    "post-distribution", STR 0, (char **) &distribution_post,
    "preview-continuation", INT 0, (char **) &preview_continuation,
    "preview-mark-read", BOOL 0, (char **) &preview_mark_read,
    "previous-also-read", BOOL 0, (char **) &prev_also_read,
    "print-header-lines", STR 0, (char **) &print_header_lines,
    "print-header-type", INT 0, (char **) &print_header_type,
    "printer", STR SAFE 1, (char **) printer,
    "query-signature", BOOL 0, (char **) &query_signature,
    "quick-count", BOOL 0, (char **) &quick_unread_count,
    "quick-save", BOOL 0, (char **) &quick_save,
    "re-layout", INT 0, (char **) &re_layout,
    "re-layout-read", INT 0, (char **) &re_layout_more,
    "read-return-next-page", BOOL 0, (char **) &read_ret_next_page,
    "record", SPEC 1, (char **) NULL,
    "repeat", BOOL 0, (char **) &fmt_rptsubj,
    "repeat-group-query", BOOL 0, (char **) &repeat_group_query,
    "report-cost", BOOL 0, (char **) &report_cost_on_exit,
    "response-check-pause", INT 0, (char **) &response_check_pause,
    "response-default-answer", STR 0, (char **) &response_dflt_answer,
    "retain-seen-status", BOOL 0, (char **) &retain_seen_status,
    "retry-on-error", INT 0, (char **) &retry_on_error,
    "save-closed-mode", INT 0, (char **) &save_closed_mode,
    "save-counter", STR 3, (char **) &save_counter_format,
    "save-counter-offset", INT 0, (char **) &save_counter_offset,
    "save-header-lines", STR 0, (char **) &save_header_lines,
    "save-report", BOOL 0, (char **) &save_report,
    "scroll-clear-page", BOOL 0, (char **) &scroll_clear_page,
    "scroll-last-lines", INT 0, (char **) &scroll_last_lines,
    "select-leave-next", BOOL 0, (char **) &select_leave_next,
    "select-on-sender", BOOL 0, (char **) &select_on_sender,
    "shading-off", CODES 0, (char **) &shade_off_attr,
    "shading-on", CODES 1, (char **) &shade_on_attr,
    "shell", STR SAFE 0, (char **) &user_shell,
    "shell-restrictions", BOOL INIT 0, (char **) &shell_restrictions,
    "show-purpose-mode", INT 0, (char **) &show_purpose_mode,
    "sign-type", STR 0, (char **) &sign_type,
    "silent", BOOL 0, (char **) &silent,
    "slow-mode", BOOL 0, (char **) &slow_mode,
    "slow-speed", INT 0, (char **) &slow_speed,
    "sort", BOOL 2, (char **) &dont_sort_articles,
    "sort-mode", INT 0, (char **) &sort_mode,
    "spell-checker", STR 0, (char **) &spell_checker,
    "split", BOOL 4, (char **) &dont_split_digests,
    "stop", INT 0, (char **) &first_page_lines,
    "subject-match-limit", INT 0, (char **) &subject_match_limit,
    "subject-match-minimum", INT 0, (char **) &match_parts_begin,
    "subject-match-offset", INT 0, (char **) &match_skip_prefix,
    "subject-match-parts", BOOL 0, (char **) &match_parts_equal,
    "suggest-default-save", BOOL 0, (char **) &suggest_save_file,
    "tidy-newsrc", BOOL 0, (char **) &tidy_newsrc,
    "time", BOOL 0, (char **) &show_current_time,
    "trace-folder-packing", BOOL 0, (char **) &folder_rewrite_trace,
    "trusted-escape-codes", STR 0, (char **) &trusted_escapes,
    "unshar-command", STR SAFE 1, (char **) unshar_command,
    "unshar-header-file", STR 0, (char **) &unshar_header_file,
    "unsubscribe-mark-read", BOOL 4, (char **) &keep_unsub_long,
    "update-frequency", INT 0, (char **) &newsrc_update_freq,
    "use-editor-line", BOOL 0, (char **) &use_ed_line,
    "use-path-in-from", BOOL 0, (char **) &use_path_in_from,
    "use-selections", BOOL 0, (char **) &use_selections,
    "visible-bell", BOOL 0, (char **) &use_visible_bell,
    "window", INT 1, (char **) &preview_window,
    "word-key", KEY 0, (char **) &delword_key,
    "wrap-header-margin", INT 2, (char **) &wrap_headers
};

#define TABLE_SIZE	(sizeof(variables)/sizeof(struct variable_defs))

#define INT_VAR		(*((int *)(var->var_addr)))
#define BOOL_VAR	(*((int *)(var->var_addr)))
#define STR_VAR		(*(var->var_addr))
#define CBUF_VAR	((char *)(var->var_addr))
#define KEY_VAR		(*((key_type *)(var->var_addr)))

#define VAR_TYPE	(var->var_flags & 0x7000)
#define VAR_OP		(var->var_flags & 0x000f)

static struct variable_defs *
lookup_variable(char *variable)
{
    register struct variable_defs *var;
    register int    i, j, k, t;

    i = 0;
    j = TABLE_SIZE - 1;

    while (i <= j) {
	k = (i + j) / 2;
	var = &variables[k];

	if ((t = strcmp(variable, var->var_name)) > 0)
	    i = k + 1;
	else if (t < 0)
	    j = k - 1;
	else
	    return var;
    }

    init_message("unknown variable: %s", variable);
    return NULL;
}


static char
escaped_char(char c)
{
    switch (c) {
	    case 'a':
	    return 007;
	case 'b':
	    return BS;
	case 'e':
	    return 033;
	case 'f':
	    return '\f';
	case 'n':
	    return NL;
	case 'r':
	    return CR;
	case 't':
	    return TAB;
    }
    return c;
}

static void
adjust(register char *str)
{
    register char  *s, *t;

    if ((s = t = str) == NULL)
	return;
    while (*s && *s != '#') {
	if (*s == '\\' && s[1] != NUL) {
	    s++;
	    *str++ = escaped_char(*s++);
	} else if (str == s) {
	    str++;
	    if (isspace(*s++))
		continue;
	} else if (isspace(*str++ = *s++))
	    continue;
	t = str;
    }
    *t = NUL;
}

int
set_variable(char *variable, int on, char *val_string)
{
    int             value;
    register struct variable_defs *var;

    if (strncmp(variable, "no", 2) == 0) {
	on = !on;
	variable += 2;
	if (variable[0] == '-')
	    variable++;
    }
    if ((var = lookup_variable(variable)) == NULL)
	return 0;

    if (!in_init && (var->var_flags & (V_INIT | V_SAFE))) {
	if (var->var_flags & V_INIT) {
	    msg("'%s' can only be set in the init file", variable);
	    return 0;
	}
	if (shell_restrictions) {
	    msg("Restricted operation - cannot change");
	    return 0;
	}
    }
    if (var->var_flags & V_LOCKED) {
	msg("Variable '%s' is locked", variable);
	return 0;
    }
    if (!on || val_string == NULL)
	value = 0;
    else
	value = atoi(val_string);

    var->var_flags |= V_MODIFIED;

    switch (VAR_TYPE) {

	case V_STRING:

	    if (on)
		adjust(val_string);

	    switch (VAR_OP) {
		case 0:
		    STR_VAR = (on && val_string) ? copy_str(val_string) : (char *) NULL;
		    break;

		case 1:
		    strcpy(CBUF_VAR, (on && val_string) ? val_string : "");
		    break;

		case 2:
		    if (on) {
			char            exp_buf[FILENAME];

			if (val_string) {
			    if (expand_file_name(exp_buf, val_string, 1))
				STR_VAR = home_relative(exp_buf);
			}
		    } else
			STR_VAR = (char *) NULL;
		    break;

		case 3:
		case 4:
		    if (!on || val_string == NULL) {
			msg("Cannot unset string `%s'", variable);
			break;
		    }
		    if (VAR_OP == 4) {
			char            exp_buf[FILENAME];
			if (expand_file_name(exp_buf, val_string, 1)) {
			    STR_VAR = copy_str(exp_buf);
			    break;
			}
		    }
		    STR_VAR = copy_str(val_string);
		    break;
		case 5:
		    STR_VAR = (on && val_string) ? copy_str(val_string) : "";
		    break;

	    }
	    break;

	case V_BOOLEAN:

	    adjust(val_string);
	    if (val_string && *val_string != NUL) {
		if (val_string[0] == 'o')
		    on = val_string[1] == 'n';	/* on */
		else
		    on = val_string[0] == 't';	/* true */
	    }
	    switch (VAR_OP) {
		case 0:
		    BOOL_VAR = on;
		    break;

		case 1:
		    BOOL_VAR = on;
		    return 1;

		case 2:
		    if (BOOL_VAR) {	/* don't change if already ok */
			if (!on)
			    break;
		    } else if (on)
			break;

		    BOOL_VAR = !on;
		    if (!in_init) {
			sort_articles(BOOL_VAR ? 0 : -1);
			return 1;
		    }
		    break;

		case 4:
		    BOOL_VAR = !on;
		    break;
	    }
	    break;

	case V_INTEGER:

	    switch (VAR_OP) {
		case 0:
		case 1:
		    INT_VAR = value;
		    break;

		case 2:
		case 3:
		    if (!on)
			value = -1;
		    INT_VAR = value;
		    break;
	    }
	    return (VAR_OP & 1);

	case V_KEY:
	    switch (VAR_OP) {
		case 0:
		    if (val_string) {
			if (*val_string)
			    adjust(val_string + 1);	/* #N is valid */
			KEY_VAR = parse_key(val_string);
		    }
		    break;
	    }
	    break;

	case V_SPECIAL:

	    switch (VAR_OP) {
		case 1:
		    if (val_string) {
			adjust(val_string);
			news_record = home_relative(val_string);
			mail_record = news_record;
			var->var_flags &= ~V_MODIFIED;
			lookup_variable("mail-record")->var_flags |= V_MODIFIED;
			lookup_variable("news-record")->var_flags |= V_MODIFIED;
		    }
		    break;

		case 2:
		    also_read_articles = on;
		    article_limit = (on && value > 0) ? value : -1;
		    break;

		case 3:
		    {
			struct chset   *csp;
			struct variable_defs *dbvar;

			dbvar = lookup_variable("data-bits");

			if (on && val_string) {
			    if ((csp = getchset(val_string)) == NULL)
				msg("Illegal value for `%s' variable", variable);
			    else {
				curchset = csp;
				data_bits = csp->cs_width ? csp->cs_width : 7;
				dbvar->var_flags &= ~V_MODIFIED;
			    }
			} else
			    msg("Cannot unset special `%s' variable", variable);
		    }
		    break;
	    }
	    break;

	case V_CODES:
	    {
		char            codes[80], code[16], *sp, *cp, *vs;

		if (val_string == NULL)
		    on = 0;
		if (on) {
		    adjust(val_string);
		    if (val_string[0] == NUL)
			on = 0;
		}
		if (on) {
		    sp = codes;
		    vs = val_string;
		    while (*vs) {
			while (*vs && (!isascii(*vs) || isspace(*vs)))
			    vs++;
			if (*vs == NUL)
			    break;
			cp = code;
			while (*vs && isascii(*vs) && !isspace(*vs))
			    *cp++ = *vs++;
			*cp = NUL;
			*sp++ = parse_key(code);
		    }
		    *sp = NUL;
		    if (codes[0] == NUL)
			on = 0;
		}
		freeobj(code_strings[VAR_OP]);
		code_strings[VAR_OP] = on ? copy_str(val_string) : NULL;
		STR_VAR = on ? copy_str(codes) : (char *) NULL;
		break;
	    }
    }
    return 0;
}

void
toggle_variable(char *variable)
{
    register struct variable_defs *var;

    if ((var = lookup_variable(variable)) == NULL)
	return;
    if (VAR_TYPE != V_BOOLEAN) {
	init_message("variable %s is not boolean", variable);
	return;
    }
    BOOL_VAR = !BOOL_VAR;
}

void
lock_variable(char *variable)
{
    register struct variable_defs *var;

    if ((var = lookup_variable(variable)) != NULL)
	var->var_flags |= V_LOCKED;
}


static char    *
var_value(register struct variable_defs * var, char *tag)
{
    static char     ival[16];
    register char  *str = NULL;
    register int    b;

    if (tag != NULL)
	*tag = var_on_stack(var) ? '>' :
	    (var->var_flags & V_LOCKED) ? '!' :
	    (var->var_flags & V_MODIFIED) ? '*' : ' ';

    switch (VAR_TYPE) {
	case V_STRING:
	    str = (VAR_OP == 1) ? CBUF_VAR : STR_VAR;
	    break;

	case V_BOOLEAN:
	    b = BOOL_VAR;
	    if (VAR_OP == 2 || VAR_OP == 4)
		b = !b;
	    str = b ? "on" : "off";
	    break;

	case V_INTEGER:
	    sprintf(ival, "%d", INT_VAR);
	    str = ival;
	    break;

	case V_KEY:
	    str = key_name(KEY_VAR);
	    break;

	case V_SPECIAL:
	    str = "UNDEF";
	    switch (VAR_OP) {
		case 2:
		    if (!also_read_articles)
			break;
		    sprintf(ival, "%d", article_limit);
		    str = ival;
		    break;
		case 3:
		    str = curchset->cs_name;
		    break;
	    }
	    break;

	case V_CODES:
	    str = code_strings[VAR_OP];
	    break;
    }
    if (str == NULL)
	str = "NULL";
    return str;
}

int
test_variable(char *expr)
{
    char           *variable;
    register struct variable_defs *var;
    int             res = -1;

    variable = expr;
    if ((expr = strchr(variable, '=')) == NULL)
	goto err;

    *expr++ = NUL;

    if ((var = lookup_variable(variable)) == NULL) {
	msg("testing unknown variable %s=%s", variable, expr);
	goto out;
    }
    switch (VAR_TYPE) {

	case V_BOOLEAN:
	    res = BOOL_VAR;

	    if (strcmp(expr, "on") == 0 || strcmp(expr, "true") == 0)
		break;
	    if (strcmp(expr, "off") == 0 || strcmp(expr, "false") == 0) {
		res = !res;
		break;
	    }
	    msg("boolean variables must be tested =on or =off");
	    break;

	case V_INTEGER:
	    res = (INT_VAR == atoi(expr)) ? 1 : 0;
	    break;

	default:
	    msg("%s: cannot only test boolean and integer variables", variable);
	    break;
    }
out:
    *--expr = '=';
err:
    return res;
}

static int      vc_column;

void
var_compl_opts(int col)
{
    vc_column = col;
}

int
var_completion(char *path, int index)
{
    static char    *head, *tail = NULL;
    static int      len;
    static struct variable_defs *var, *help_var;

    if (index < 0) {
	clrmsg(-(index + 1));
	return 1;
    }
    if (path) {
	head = path;
	tail = path + index;
	while (*head && isspace(*head))
	    head++;
	if (strncmp(head, "no", 2) == 0) {
	    head += 2;
	    if (*head == '-')
		head++;
	}
	help_var = var = variables;
	len = tail - head;

	return 1;
    }
    if (index) {
	list_completion((char *) NULL);

	for (;; help_var++) {
	    if (help_var >= &variables[TABLE_SIZE]) {
		help_var = variables;
		break;
	    }
	    index = strncmp(help_var->var_name, head, len);
	    if (index < 0)
		continue;
	    if (index > 0) {
		help_var = variables;
		break;
	    }
	    if (list_completion(help_var->var_name) == 0)
		break;
	}
	fl;
	return 1;
    }
    for (; var < &variables[TABLE_SIZE]; var++) {
	if (len == 0)
	    index = 0;
	else
	    index = strncmp(var->var_name, head, len);
	if (index < 0)
	    continue;
	if (index > 0)
	    break;
	sprintf(tail, "%s ", var->var_name + len);
	msg("%.70s", var_value(var, (char *) NULL));
	gotoxy(prompt_length + vc_column, prompt_line);
	var++;
	return 1;
    }
    clrmsg(vc_column);
    return 0;
}

static struct var_stack {
    struct var_stack *next;
    struct variable_defs *v;
    int             mod_flag;
    union {
	int             ivar;
	int             bool;
	char            key;
	char           *str;
    }               value;
}              *var_stack = NULL, *vs_pool = NULL;

void
mark_var_stack(void)
{
    register struct var_stack *vs;

    if (vs_pool) {
	vs = vs_pool;
	vs_pool = vs->next;
    } else
	vs = newobj(struct var_stack, 1);

    vs->next = var_stack;
    var_stack = vs;
    vs->v = NULL;
}

int
push_variable(char *variable)
{
    register struct variable_defs *var;
    register struct var_stack *vs;

    if (strncmp(variable, "no", 2) == 0) {
	variable += 2;
	if (variable[0] == '-')
	    variable++;
    }
    if ((var = lookup_variable(variable)) == NULL) {
	msg("pushing unknown variable %s", variable);
	return 0;
    }
    mark_var_stack();
    vs = var_stack;
    vs->v = var;
    vs->mod_flag = var->var_flags & V_MODIFIED;

    switch (VAR_TYPE) {

	case V_STRING:

	    switch (VAR_OP) {
		case 0:	/* if we update one of these variables,	 */
		case 2:	/* new storage will be allocated for it */
		case 3:	/* so it is ok just to save the pointer */
		case 4:
		    vs->value.str = STR_VAR;
		    break;

		case 1:	/* we free this memory when restored	 */
		    vs->value.str = copy_str(CBUF_VAR);
		    break;
	    }
	    break;

	case V_BOOLEAN:
	    vs->value.bool = BOOL_VAR;
	    break;

	case V_INTEGER:
	    vs->value.ivar = INT_VAR;
	    break;

	case V_KEY:
	    vs->value.key = KEY_VAR;
	    break;

	case V_SPECIAL:
	    msg("Cannot push pseudo variable %s", var->var_name);
	    break;

	case V_CODES:
	    msg("Cannot push code string variable %s", var->var_name);
	    break;
    }

    return 1;
}

void
restore_variables(void)
{
    register struct variable_defs *var;
    register struct var_stack *vs, *vs1;

    vs = var_stack;

    while (vs != NULL) {
	if ((var = vs->v) == NULL) {
	    var_stack = vs->next;
	    vs->next = vs_pool;
	    vs_pool = vs;
	    return;
	}
	var->var_flags &= ~V_MODIFIED;
	var->var_flags |= vs->mod_flag;

	switch (VAR_TYPE) {

	    case V_STRING:
		switch (VAR_OP) {
		    case 0:	/* only restore the string if changed; then
				 * we	 */
		    case 2:	/* can also free the memory occupied by the	 */
		    case 3:	/* 'new' value (if not NULL)			 */
		    case 4:
			if (STR_VAR != vs->value.str) {
			    if (STR_VAR != NULL)
				freeobj(STR_VAR);
			    STR_VAR = vs->value.str;
			}
			break;

		    case 1:	/* it fitted before, so it will fit againg */
			strcpy(CBUF_VAR, vs->value.str);
			freeobj(vs->value.str);
			break;
		}
		break;

	    case V_BOOLEAN:
		BOOL_VAR = vs->value.bool;
		break;

	    case V_INTEGER:
		INT_VAR = vs->value.ivar;
		break;

	    case V_KEY:
		KEY_VAR = vs->value.key;
		break;

	    case V_SPECIAL:	/* these are not saved, so... */
		break;

	    case V_CODES:
		break;
	}

	vs1 = vs->next;
	vs->next = vs_pool;
	vs_pool = vs;
	vs = vs1;
    }
    var_stack = NULL;
}

static int
var_on_stack(register struct variable_defs * var)
{
    register struct var_stack *vs;

    for (vs = var_stack; vs; vs = vs->next)
	if (vs->v == var)
	    return 1;
    return 0;
}

void
disp_variables(int all, char *rexp)
{
    char           *str, pushed;
    register struct variable_defs *var;

    if (in_init)
	return;

    clrdisp();
    if (novice && !all && rexp == NULL) {
	msg("Use `:set all' to see all variable settings");
	home();
    }
    so_printxy(0, 0, "Variable settings");
    pg_init(1, 1);
    if (rexp) {
	pg_regexp = regcomp(rexp + 1);
	all = 1;
    }
    for (var = variables; var < &variables[TABLE_SIZE]; var++) {
	if (pg_regexp != NULL && regexec(pg_regexp, var->var_name) == 0)
	    continue;
	str = var_value(var, &pushed);
	if (!all && pushed == ' ')
	    continue;
	if (pg_next() < 0)
	    break;
	if (pg_new_regexp) {
	    pg_new_regexp = 0;
	    var = variables;
	    var--;
	    continue;
	}
	if (VAR_TYPE == V_STRING)
	    tprintf("%c %-25.25s = \"%s\"\n", pushed, var->var_name, str);
	else
	    tprintf("%c %-25.25s = %s\n", pushed, var->var_name, str);
    }

    pg_end();
}

void
print_variable_config(FILE * f, int all)
{
    register struct variable_defs *var;
    char           *str, tag[2];

    tag[1] = NUL;
    for (var = variables; var < &variables[TABLE_SIZE]; var++) {
	if (!all && (var->var_flags & V_MODIFIED) == 0)
	    continue;
	str = var_value(var, tag);
	if (var->var_name == "nntp-password" || var->var_name == "nntp-user")
	    str = "XXXXXXXX";
	fprintf(f, "%s%s='%s'\n", all ? tag : "", var->var_name, str);
    }
}

/*
 *	var_options(string_var *, options, result *)
 *
 *	test whether "string_var" contains any of the options
 *	listed in "options" (NUL-separated list of names ended with
 *	double NUL).  On return, string_var is advanced over all
 *	recognized options and the result will have the bits corresponding
 *	to the recognized options set.
 */

void
var_options(char **str, register char *options, flag_type * res)
{
    char            word[128];
    char           *optab[32];
    register char **op, *wp, c;
    register int    n;

    for (op = optab; *options != NUL; op++, options++) {
	*op = options;
	while (*options)
	    options++;
    }
    *op = NULL;

    *res = 0;

    options = *str;
    while (*options) {
	for (wp = word; (c = *options) != NUL; *wp++ = c, options++)
	    if (isascii(c) && isspace(c))
		break;
	while (((c = *options)) && isascii(c) && isspace(c))
	    options++;
	*wp = NUL;

	for (op = optab, n = 1; *op != NULL; op++, n++) {
	    if (strcmp(word, *op))
		continue;
	    *res |= FLAG(n);
	    *str = options;
	    break;
	}
	if (*op == NULL)
	    break;
    }
}
