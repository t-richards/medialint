#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include <gio/gio.h>
#include <glib.h>
#include <libavformat/avformat.h>

#include "media_linter.h"
#include "report.h"

int total_files = 0;
gint files_scanned = 0;

// Thread pool for parallel processing.
GThreadPool *pool = NULL;

// Linter state passed to each worker thread via the thread pool user_data slot.
LinterState state = {0};

// Recursively walk a directory tree, pushing regular file paths onto the thread
// pool. Symlinks are not followed (G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS).
static void walk_directory(GFile *dir)
{
    GError *error = NULL;
    GFileEnumerator *enumerator =
        g_file_enumerate_children(dir, G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);

    if (enumerator == NULL)
    {
        gchar *dir_path = g_file_get_path(dir);
        fprintf(stderr, "Failed to read directory %s: %s\n", dir_path, error->message);
        g_free(dir_path);
        g_error_free(error);
        return;
    }

    GFileInfo *info;
    while ((info = g_file_enumerator_next_file(enumerator, NULL, &error)) != NULL)
    {
        GFileType type = g_file_info_get_file_type(info);
        GFile *child = g_file_get_child(dir, g_file_info_get_name(info));

        if (type == G_FILE_TYPE_REGULAR)
        {
            total_files++;
            // g_file_get_path allocates with g_malloc; freed by lint_media_file via g_free.
            g_thread_pool_push(pool, g_file_get_path(child), NULL);
        }
        else if (type == G_FILE_TYPE_DIRECTORY)
        {
            walk_directory(child);
        }

        g_object_unref(child);
        g_object_unref(info);
    }

    if (error != NULL)
    {
        gchar *dir_path = g_file_get_path(dir);
        fprintf(stderr, "Error reading directory %s: %s\n", dir_path, error->message);
        g_free(dir_path);
        g_error_free(error);
    }

    g_object_unref(enumerator);
}

void init()
{
    // Set ffmpeg to have silent output.
    av_log_set_level(AV_LOG_FATAL);

    // Create reporting context.
    state.reporting_context = reporting_context_new();
    state.files_scanned = &files_scanned;

    // Create regular expressions.
    GRegexCompileFlags flags = G_REGEX_CASELESS | G_REGEX_OPTIMIZE;
    state.forbidden_chars_regex = g_regex_new("[<>:\"|?*]", flags, 0, NULL);
    state.movie_year_regex = g_regex_new("\\(\\d{4}\\)", flags, 0, NULL);
    state.tv_naming_regex = g_regex_new("S\\d{2}E\\d{2}", flags, 0, NULL);

    // Compile regular expressions.
    // GLib invokes the PCRE2 JIT compiler during the first use of a regular expression.
    // This operation is not thread-safe and it can leak allocations unless we do it ahead of time.
    g_regex_match(state.forbidden_chars_regex, "", 0, NULL);
    g_regex_match(state.movie_year_regex, "", 0, NULL);
    g_regex_match(state.tv_naming_regex, "", 0, NULL);

    // Start thread pool.
    pool = g_thread_pool_new((GFunc)lint_media_file, &state, g_get_num_processors(), TRUE, NULL);
}

void cleanup()
{
    // Free regular expressions.
    g_regex_unref(state.forbidden_chars_regex);
    g_regex_unref(state.movie_year_regex);
    g_regex_unref(state.tv_naming_regex);

    // Free reporting context.
    reporting_context_free(state.reporting_context);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Initialize important globals.
    init();

    GTimer *timer = g_timer_new();

    // g_file_new_for_commandline_arg resolves relative paths against CWD.
    GFile *root = g_file_new_for_commandline_arg(argv[1]);
    walk_directory(root);
    g_object_unref(root);

    // Wait for all threads to finish.
    g_thread_pool_free(pool, FALSE, TRUE);

    g_timer_stop(timer);
    double elapsed = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    puts("");
    int report_file_count = reporting_context_print(state.reporting_context);

    printf("Time:      %.2f seconds\n", elapsed);
    printf("Total:     %d\n", total_files);
    printf("Processed: %d\n", files_scanned);
    printf("Errors:    %d\n", report_file_count);

    cleanup();
}
