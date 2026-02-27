#pragma once

#include <glib.h>

#include "report.h"

// State passed to lint_media_file via the GLib thread pool user_data slot.
// Owns no memory — all pointers must outlive the thread pool.
typedef struct LinterState
{
    ReportingContext *reporting_context;
    GRegex *forbidden_chars_regex;
    GRegex *movie_year_regex;
    GRegex *tv_naming_regex;
    gint *files_scanned;
} LinterState;

/**
 * Run all linters on a single media file.
 * Signature matches GFunc: called by the GLib thread pool.
 */
void lint_media_file(char *file_path, LinterState *state);
