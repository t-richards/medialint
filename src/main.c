#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <errno.h>
#include <ftw.h>
#include <time.h>

#include <glib.h>
#include <libavformat/avformat.h>

#include "linter.h"
#include "report.h"

int total_files = 0;
gint files_scanned = 0;

// Thread pool for parallel processing.
GThreadPool *pool = NULL;

// Reporting context for storing linter messages.
ReportingContext *reporting_context = NULL;

// Regular expressions for file path checks.
GRegex *forbidden_chars_regex = NULL;
GRegex *movie_year_regex = NULL;
GRegex *tv_naming_regex = NULL;

// https://en.wikipedia.org/wiki/Binary_prefix#Definitions
const int MEBIBIT = 1048576;

// The minimum bit rate for a video that we're willing to tolerate.
const int MIN_BIT_RATE = 2 * MEBIBIT;

// The minimum frame size for a video that we're willing to tolerate.
const int MIN_PIXEL_COUNT = 1280 * 720;

// Runs all linters on a single media file.
void lint_media_file(char *file_path, const void *_unused)
{
    // Processing one file now.
    printf(".");
    fflush(stdout);

    // Check for forbidden characters in the file path.
    char **parts = g_strsplit(file_path, "/", 0);
    for (int i = 0; parts[i] != NULL; i++)
    {
        gboolean forbidden_matched = g_regex_match(forbidden_chars_regex, parts[i], 0, NULL);
        if (forbidden_matched)
        {
            reporting_context_add(reporting_context, file_path, CLASS_NAMING_FORBIDDEN, "Forbidden characters in file path.");
            break;
        }
    }

    // Movie files should have a year in the file name.
    if (g_str_match_string("movies", file_path, TRUE))
    {
        gboolean matched = g_regex_match(movie_year_regex, file_path, 0, NULL);
        if (!matched)
        {
            reporting_context_add(reporting_context, file_path, CLASS_NAMING_MOVIE, "Movie year does not match (0000).");
        }
    }
    // TV files should have a season and episode in the file name.
    else if (g_str_match_string("tv", file_path, TRUE))
    {
        gboolean matched = g_regex_match(tv_naming_regex, file_path, 0, NULL);
        if (!matched)
        {
            reporting_context_add(reporting_context, file_path, CLASS_NAMING_TV, "TV episode does not match S00E00.");
        }
    }

    // Done with path parts.
    g_strfreev(parts);

    AVFormatContext *ifmt_ctx = avformat_alloc_context();

    // Attempt to speed up reading / decoding.
    ifmt_ctx->flags |= AVFMT_FLAG_NOBUFFER;
    ifmt_ctx->flags |= AVFMT_FLAG_NOFILLIN;
    ifmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;
    ifmt_ctx->flags |= AVFMT_FLAG_NOPARSE;

    // Require strict input format compliance.
    ifmt_ctx->strict_std_compliance = FF_COMPLIANCE_VERY_STRICT;

    // Blow up on decoding errors.
    ifmt_ctx->error_recognition |= AV_EF_CRCCHECK;
    ifmt_ctx->error_recognition |= AV_EF_BITSTREAM;
    ifmt_ctx->error_recognition |= AV_EF_BUFFER;
    ifmt_ctx->error_recognition |= AV_EF_EXPLODE;
    ifmt_ctx->error_recognition |= AV_EF_CAREFUL;
    ifmt_ctx->error_recognition |= AV_EF_COMPLIANT;
    ifmt_ctx->error_recognition |= AV_EF_AGGRESSIVE;

    int ret = avformat_open_input(&ifmt_ctx, file_path, NULL, NULL);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        reporting_context_add(reporting_context, file_path, CLASS_FORMAT_UNSUPPORTED, errbuf);

        free(file_path);
        avformat_free_context(ifmt_ctx);
        return;
    }

    g_atomic_int_inc(&files_scanned);

    int subtitle_count = 0;
    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;

        // Check video streams, ignoring attached pictures.
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && stream->disposition != AV_DISPOSITION_ATTACHED_PIC)
        {
            // Check video bit rate, ignoring streams where it could not be determined.
            if (codecpar->bit_rate != 0 && codecpar->bit_rate <= MIN_BIT_RATE)
            {
                char report_message[128] = {0};
                snprintf(report_message, 128, "%.2f Mibps [track %d].", codecpar->bit_rate / (float)MEBIBIT, i);
                reporting_context_add(reporting_context, file_path, CLASS_VIDEO_BITRATE, report_message);
            }

            // Check video resolution.
            int pixel_count = codecpar->width * codecpar->height;
            if (pixel_count <= MIN_PIXEL_COUNT)
            {
                char report_message[128] = {0};
                snprintf(report_message, 128, "%dx%d [track %d].", codecpar->width, codecpar->height, i);
                reporting_context_add(reporting_context, file_path, CLASS_VIDEO_RESOLUTION, report_message);
            }

            // Check video codec.
            const char *codec_name = NULL;
            switch (codecpar->codec_id)
            {
            case AV_CODEC_ID_H264:
            case AV_CODEC_ID_HEVC:
            case AV_CODEC_ID_VP9:
            case AV_CODEC_ID_AV1:
                // Do nothing, we like these codecs.
                break;

            default:
                codec_name = avcodec_get_name(codecpar->codec_id);
                char report_message[128] = {0};
                snprintf(report_message, 128, "%s [track %d].", codec_name, i);
                reporting_context_add(reporting_context, file_path, CLASS_VIDEO_CODEC, report_message);
            }
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            // Count subtitles.
            subtitle_count++;
        }
    }

    if (subtitle_count < 1)
    {
        reporting_context_add(reporting_context, file_path, CLASS_SUBTITLES_PRESENCE, "No subtitles found.");
    }

    free(file_path);
    avformat_close_input(&ifmt_ctx);
}

int nftw_callback(const char *fpath, const struct stat *sb,
                  int tflag, struct FTW *ftwbuf)
{
    switch (tflag)
    {
    case FTW_F:
        total_files++;
        char *fpath_copy = strdup(fpath); // This will be freed by the thread.
        g_thread_pool_push(pool, (gpointer)fpath_copy, NULL);
        break;
    }

    return 0;
}

void init()
{
    // Set ffmpeg to have silent output.
    av_log_set_level(AV_LOG_FATAL);

    // Create reporting context.
    reporting_context = reporting_context_new();

    // Start thread pool.
    pool = g_thread_pool_new((GFunc)lint_media_file, NULL, g_get_num_processors(), TRUE, NULL);

    // Create regular expressions.
    GRegexCompileFlags flags = G_REGEX_CASELESS | G_REGEX_OPTIMIZE;
    forbidden_chars_regex = g_regex_new("[<>:\"/\\|?*]", flags, 0, NULL);
    movie_year_regex = g_regex_new("\\(\\d{4}\\)", flags, 0, NULL);
    tv_naming_regex = g_regex_new("S\\d{2}E\\d{2}", flags, 0, NULL);

    // Compile regular expressions.
    // GLib invokes the PCRE2 JIT compiler during the first use of a regular expression.
    // This operation is not thread-safe and it can leak allocations unless we do it ahead of time.
    g_regex_match(forbidden_chars_regex, "", 0, NULL);
    g_regex_match(movie_year_regex, "", 0, NULL);
    g_regex_match(tv_naming_regex, "", 0, NULL);
}

void cleanup()
{
    // Free regular expressions.
    g_regex_unref(forbidden_chars_regex);
    g_regex_unref(movie_year_regex);
    g_regex_unref(tv_naming_regex);

    // Free reporting context.
    reporting_context_free(reporting_context);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *path = realpath(argv[1], NULL);
    if (path == NULL)
    {
        int errno_sv = errno;
        fprintf(stderr, "Failed to resolve path %s: %s\n", argv[1],
                strerror(errno_sv));

        return EXIT_FAILURE;
    }

    // Initialize important globals.
    init();

    GTimer *timer = g_timer_new();

    const int nopenfd = 32; // max number of open file descriptors by nftw.
    int flags = FTW_PHYS | FTW_MOUNT | FTW_CHDIR;
    if (nftw(path, nftw_callback, nopenfd, flags) == -1)
    {
        int errno_sv = errno;
        fprintf(stderr, "Failed to walk file tree: %s\n", strerror(errno_sv));
    }

    // Wait for all threads to finish.
    g_thread_pool_free(pool, FALSE, TRUE);

    g_timer_stop(timer);
    double elapsed = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    puts("");
    int report_file_count = reporting_context_print(reporting_context);

    printf("Time:      %.2f seconds\n", elapsed);
    printf("Total:     %d\n", total_files);
    printf("Processed: %d\n", files_scanned);
    printf("Errors:    %d\n", report_file_count);

    // Be nice and clean up.
    free(path);
    cleanup();
}
