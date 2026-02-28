#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <libavformat/avformat.h>

#include "linter.h"
#include "media_linter.h"
#include "report.h"

// https://en.wikipedia.org/wiki/Binary_prefix#Definitions
static const int MEBIBIT = 1048576;

// The minimum bit rate for a video that we're willing to tolerate.
static const int MIN_BIT_RATE = 2 * 1048576;

// The minimum frame size for a video that we're willing to tolerate.
static const int MIN_PIXEL_COUNT = 1280 * 720;

// Accumulated cross-stream facts collected during probe_streams().
typedef struct
{
    // Audio
    int audio_count;
    unsigned int first_audio_idx;
    const char *first_audio_lang; // NULL = no audio; points into AVStream metadata

    // Subtitles
    int subtitle_count;
    bool has_english_subtitle;
} MediaSummary;

static bool is_english(const char *lang)
{
    return lang && (strcmp(lang, "eng") == 0 || strcmp(lang, "en") == 0);
}

// Single pass over all streams. Video lints fire immediately (no cross-stream
// dependencies). Audio and subtitle facts are accumulated for post-loop lints.
static void probe_streams(AVFormatContext *ctx, MediaSummary *summary, const char *path, LinterState *state)
{
    for (unsigned int i = 0; i < ctx->nb_streams; i++)
    {
        AVStream *stream = ctx->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;

        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && stream->disposition != AV_DISPOSITION_ATTACHED_PIC)
        {
            // Check video bit rate, ignoring streams where it could not be determined.
            if (codecpar->bit_rate != 0 && codecpar->bit_rate <= MIN_BIT_RATE)
            {
                char msg[128] = {0};
                snprintf(msg, 128, "%.2f Mibps [track %d].", codecpar->bit_rate / (float)MEBIBIT, i);
                reporting_context_add(state->reporting_context, path, CLASS_VIDEO_BITRATE, msg);
            }

            // Check video resolution.
            int pixel_count = codecpar->width * codecpar->height;
            if (pixel_count <= MIN_PIXEL_COUNT)
            {
                char msg[128] = {0};
                snprintf(msg, 128, "%dx%d [track %d].", codecpar->width, codecpar->height, i);
                reporting_context_add(state->reporting_context, path, CLASS_VIDEO_RESOLUTION, msg);
            }

            // Check video codec.
            switch (codecpar->codec_id)
            {
            case AV_CODEC_ID_H264:
            case AV_CODEC_ID_HEVC:
            case AV_CODEC_ID_VP9:
            case AV_CODEC_ID_AV1:
                // Do nothing, we like these codecs.
                break;

            default: {
                char msg[128] = {0};
                snprintf(msg, 128, "%s [track %d].", avcodec_get_name(codecpar->codec_id), i);
                reporting_context_add(state->reporting_context, path, CLASS_VIDEO_CODEC, msg);
            }
            }
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (summary->audio_count == 0)
            {
                summary->first_audio_idx = i;
                AVDictionaryEntry *lang = av_dict_get(stream->metadata, "language", NULL, 0);
                summary->first_audio_lang = lang ? lang->value : NULL;
            }
            summary->audio_count++;
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            summary->subtitle_count++;

            // Track whether any subtitle track is English.
            AVDictionaryEntry *lang = av_dict_get(stream->metadata, "language", NULL, 0);
            if (is_english(lang ? lang->value : NULL))
                summary->has_english_subtitle = true;

            // Check that default/forced subtitles are in English.
            if (stream->disposition & (AV_DISPOSITION_DEFAULT | AV_DISPOSITION_FORCED))
            {
                const char *lang_str = lang ? lang->value : "unknown";
                if (!is_english(lang_str))
                {
                    char msg[128] = {0};
                    snprintf(msg, 128, "%s [track %d, %s].",
                             (stream->disposition & AV_DISPOSITION_FORCED) ? "forced" : "default", i, lang_str);
                    reporting_context_add(state->reporting_context, path, CLASS_SUBTITLES_LANGUAGE, msg);
                }
            }
        }
    }
}

// A single non-English audio track is acceptable when English subtitles are present
// (i.e. a foreign film with English subs). For multi-track files, the first track must be
// English because Plex ignores the default flag and always picks track 0.
static void lint_audio_language(const MediaSummary *summary, const char *path, LinterState *state)
{
    const char *lang = summary->first_audio_lang;
    bool is_known = lang && strcmp(lang, "und") != 0;
    if (!is_known || is_english(lang))
        return;

    bool exempt = summary->audio_count == 1 && summary->has_english_subtitle;
    if (!exempt)
    {
        char msg[128] = {0};
        snprintf(msg, 128, "first audio track is not English [track %d, %s].", summary->first_audio_idx, lang);
        reporting_context_add(state->reporting_context, path, CLASS_AUDIO_LANGUAGE, msg);
    }
}

static void lint_subtitle_presence(const MediaSummary *summary, const char *path, LinterState *state)
{
    if (summary->subtitle_count < 1)
    {
        reporting_context_add(state->reporting_context, path, CLASS_SUBTITLES_PRESENCE, "No subtitles found.");
    }
}

static void lint_path(const char *path, LinterState *state)
{
    // Check for forbidden characters in the file path.
    char **parts = g_strsplit(path, G_DIR_SEPARATOR_S, 0);
    for (int i = 0; parts[i] != NULL; i++)
    {
        if (g_regex_match(state->forbidden_chars_regex, parts[i], 0, NULL))
        {
            reporting_context_add(state->reporting_context, path, CLASS_NAMING_FORBIDDEN,
                                  "Forbidden characters in file path.");
            break;
        }
    }
    g_strfreev(parts);

    // Movie files should have a year in the file name.
    if (g_str_match_string("movies", path, TRUE))
    {
        if (!g_regex_match(state->movie_year_regex, path, 0, NULL))
        {
            reporting_context_add(state->reporting_context, path, CLASS_NAMING_MOVIE,
                                  "Movie year does not match (0000).");
        }
    }
    // TV files should have a season and episode in the file name.
    else if (g_str_match_string("tv", path, TRUE))
    {
        if (!g_regex_match(state->tv_naming_regex, path, 0, NULL))
        {
            reporting_context_add(state->reporting_context, path, CLASS_NAMING_TV, "TV episode does not match S00E00.");
        }
    }
}

void lint_media_file(char *file_path, LinterState *state)
{
    printf(".");
    fflush(stdout);

    lint_path(file_path, state);

    AVFormatContext *ifmt_ctx = avformat_alloc_context();

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
        reporting_context_add(state->reporting_context, file_path, CLASS_FORMAT_UNSUPPORTED, errbuf);

        g_free(file_path);
        avformat_free_context(ifmt_ctx);
        return;
    }

    g_atomic_int_inc(state->files_scanned);

    MediaSummary summary = {0};
    probe_streams(ifmt_ctx, &summary, file_path, state);

    lint_audio_language(&summary, file_path, state);
    lint_subtitle_presence(&summary, file_path, state);

    g_free(file_path);
    avformat_close_input(&ifmt_ctx);
}
