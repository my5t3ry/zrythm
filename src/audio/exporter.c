/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Audio file exporter.
 *
 * Currently used by the export dialog to export
 * the mixdown.
 */

#include <stdio.h>

#include "config.h"

#include "audio/channel.h"
#include "audio/engine.h"
#ifdef HAVE_JACK
#include "audio/engine_jack.h"
#endif
#include "audio/exporter.h"
#include "audio/marker_track.h"
#include "audio/master_track.h"
#include "audio/mixer.h"
#include "audio/position.h"
#include "audio/routing.h"
#include "audio/transport.h"
#include "gui/widgets/main_window.h"
#include "project.h"
#include "utils/io.h"
#include "utils/ui.h"
#include "midilib/src/midifile.h"

#include <sndfile.h>

#define	AMPLITUDE	(1.0 * 0x7F000000)

/**
 * Returns the audio format as string.
 *
 * Must be g_free()'d by caller.
 */
char *
exporter_stringize_audio_format (
  AudioFormat format)
{
  switch (format)
    {
    case AUDIO_FORMAT_FLAC:
      return g_strdup ("FLAC");
      break;
    case AUDIO_FORMAT_OGG:
      return g_strdup ("ogg");
      break;
    case AUDIO_FORMAT_WAV:
      return g_strdup ("wav");
      break;
    case AUDIO_FORMAT_MP3:
      return g_strdup ("mp3");
      break;
    case AUDIO_FORMAT_MIDI:
      return g_strdup ("mid");
      break;
    case NUM_AUDIO_FORMATS:
      break;
    }

  g_return_val_if_reached (NULL);
}

static void
export_audio (
  ExportSettings * info)
{
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));

#define EXPORT_CHANNELS 2

  if (info->format == AUDIO_FORMAT_FLAC)
    {
      sfinfo.format = SF_FORMAT_FLAC;
    }
  else if (info->format == AUDIO_FORMAT_WAV)
    {
      g_message ("FORMAT WAV");
      sfinfo.format = SF_FORMAT_WAV;
    }
  else if (info->format == AUDIO_FORMAT_OGG)
    {
      sfinfo.format = SF_FORMAT_OGG;
    }
  else
    {
      char * format =
        exporter_stringize_audio_format (
          info->format);
      char str[600];
      sprintf (
        str, "Format %s not supported yet",
        format);
      /* FIXME this is not the GTK thread */
      ui_show_error_message (
        MAIN_WINDOW, str);
      g_free (format);

      return;
    }

  if (info->format == AUDIO_FORMAT_OGG)
    {
      sfinfo.format =
        sfinfo.format | SF_FORMAT_VORBIS;
    }
  else if (info->depth == BIT_DEPTH_16)
    {
      sfinfo.format =
        sfinfo.format | SF_FORMAT_PCM_16;
      g_message ("PCM 16");
    }
  else if (info->depth == BIT_DEPTH_24)
    {
      sfinfo.format =
        sfinfo.format | SF_FORMAT_PCM_24;
      g_message ("PCM 24");
    }
  else if (info->depth == BIT_DEPTH_32)
    {
      sfinfo.format =
        sfinfo.format | SF_FORMAT_PCM_32;
      g_message ("PCM 32");
    }

  if (info->time_range ==
        TIME_RANGE_SONG)
    {
      ArrangerObject * start =
        (ArrangerObject *)
        marker_track_get_start_marker (
          P_MARKER_TRACK);
      ArrangerObject * end =
        (ArrangerObject *)
        marker_track_get_end_marker (
          P_MARKER_TRACK);
      sfinfo.frames =
        position_to_frames (
          &end->pos) -
        position_to_frames (
          &start->pos);
    }
  else if (info->time_range ==
             TIME_RANGE_LOOP)
    {
      sfinfo.frames =
        position_to_frames (
          &TRANSPORT->loop_end_pos) -
          position_to_frames (
            &TRANSPORT->loop_start_pos);
    }

  sfinfo.samplerate =
    (int) AUDIO_ENGINE->sample_rate;
  sfinfo.channels = EXPORT_CHANNELS;

  if (!sf_format_check (&sfinfo))
    {
      ui_show_error_message (
        MAIN_WINDOW,
        "SF INFO invalid");
      return;
    }

  char * dir = io_get_dir (info->file_uri);
  io_mkdir (dir);
  g_free (dir);
  SNDFILE * sndfile =
    sf_open (info->file_uri, SFM_WRITE, &sfinfo);

  if (!sndfile)
    {
      int error = sf_error (NULL);
      char error_str[600];
      switch (error)
        {
        case 1:
          strcpy (error_str, "Unrecognized format");
          break;
        case 2:
          strcpy (error_str, "System error");
          break;
        case 3:
          strcpy (error_str, "Malformed file");
          break;
        case 4:
          strcpy (error_str, "Unsupported encoding");
          break;
        default:
          g_warn_if_reached ();
          return;
        }

      g_warning (
        "Couldn't open SNDFILE %s:\n%s",
        info->file_uri, error_str);

      return;
    }

  sf_set_string (
    sndfile,
    SF_STR_TITLE,
    PROJECT->title);
  sf_set_string (
    sndfile,
    SF_STR_SOFTWARE,
    "Zrythm");
  sf_set_string (
    sndfile,
    SF_STR_ARTIST,
    info->artist);
  sf_set_string (
    sndfile,
    SF_STR_GENRE,
    info->genre);

  Position prev_playhead_pos;
  /* position to start at */
  POSITION_INIT_ON_STACK (start_pos);
  /* position to stop at */
  POSITION_INIT_ON_STACK (stop_pos);
  position_set_to_pos (
    &prev_playhead_pos,
    &TRANSPORT->playhead_pos);
  if (info->time_range ==
        TIME_RANGE_SONG)
    {
      ArrangerObject * start =
        (ArrangerObject *)
        marker_track_get_start_marker (
          P_MARKER_TRACK);
      ArrangerObject * end =
        (ArrangerObject *)
        marker_track_get_end_marker (
          P_MARKER_TRACK);
      position_set_to_pos (
        &TRANSPORT->playhead_pos,
        &start->pos);
      position_set_to_pos (
        &start_pos,
        &start->pos);
      position_set_to_pos (
        &stop_pos,
        &end->pos);
    }
  else if (info->time_range ==
             TIME_RANGE_LOOP)
    {
      position_set_to_pos (
        &TRANSPORT->playhead_pos,
        &TRANSPORT->loop_start_pos);
      position_set_to_pos (
        &start_pos,
        &TRANSPORT->loop_start_pos);
      position_set_to_pos (
        &stop_pos,
        &TRANSPORT->loop_end_pos);
    }
  Play_State prev_play_state =
    TRANSPORT->play_state;
  TRANSPORT->play_state =
    PLAYSTATE_ROLLING;

  /* set jack freewheeling mode */
#ifdef HAVE_JACK
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    {
      jack_set_freewheel (
        AUDIO_ENGINE->client, 1);
    }
#endif

  zix_sem_wait (
    &AUDIO_ENGINE->port_operation_lock);

  nframes_t nframes;
  g_return_if_fail (
    stop_pos.frames >= 1 ||
    start_pos.frames >= 0);
  const unsigned long total_frames =
    (unsigned long)
    ((stop_pos.frames - 1) -
     start_pos.frames);
  sf_count_t covered = 0;
  float out_ptr[
    AUDIO_ENGINE->nframes * EXPORT_CHANNELS];
  do
    {
      /* calculate number of frames to process
       * this time */
      nframes =
        (nframes_t)
        MIN (
          (stop_pos.frames - 1) -
            TRANSPORT->playhead_pos.frames,
          (long) AUDIO_ENGINE->nframes);

      /* run process code */
      engine_process_prepare (
        AUDIO_ENGINE,
        nframes);
      router_start_cycle (
        &MIXER->router, nframes,
        0, PLAYHEAD);
      engine_post_process (
        AUDIO_ENGINE, nframes);

      /* by this time, the Master channel should
       * have its Stereo Out ports filled.
       * pass its buffers to the output */
      for (nframes_t i = 0; i < nframes; i++)
        {
          out_ptr[i * 2] =
            P_MASTER_TRACK->channel->
              stereo_out->l->buf[i];
          out_ptr[i * 2 + 1] =
            P_MASTER_TRACK->channel->
              stereo_out->r->buf[i];
        }

      /* seek to the write position in the file */
      if (covered != 0)
        {
          sf_count_t seek_cnt =
            sf_seek (
              sndfile, covered,
              SEEK_SET | SFM_WRITE);

          /* wav is weird for some reason */
          if ((sfinfo.format & SF_FORMAT_WAV) == 0)
            {
              if (seek_cnt < 0)
                {
                  char err[256];
                  sf_error_str (
                    0, err, sizeof (err) - 1);
                  g_message (
                    "Error seeking file: %s", err);
                }
              g_warn_if_fail (seek_cnt == covered);
            }
        }

      /* write the frames for the current
       * cycle */
      sf_count_t written_frames =
        sf_writef_float (
          sndfile, out_ptr, nframes);
      g_warn_if_fail (written_frames == nframes);

      covered += nframes;
      g_warn_if_fail (
        covered ==
          TRANSPORT->playhead_pos.frames);

      info->progress =
        (double)
        (TRANSPORT->playhead_pos.frames -
          start_pos.frames) /
        (double) total_frames;
    } while (
      TRANSPORT->playhead_pos.frames <
      stop_pos.frames - 1);

  g_warn_if_fail (
    covered == (sf_count_t) total_frames);

  info->progress = 1.0;

  /* set jack freewheeling mode */
#ifdef HAVE_JACK
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    {
      jack_set_freewheel (
        AUDIO_ENGINE->client, 0);
    }
#endif

  zix_sem_post (
    &AUDIO_ENGINE->port_operation_lock);

  TRANSPORT->play_state = prev_play_state;
  position_set_to_pos (
    &TRANSPORT->playhead_pos,
    &prev_playhead_pos);

  sf_close (sndfile);
}

static void
export_midi (
  ExportSettings * info)
{
  MIDI_FILE *mf;

  int i;
	if ((mf = midiFileCreate (info->file_uri, TRUE)))
		{
      /* Write tempo information out to track 1 */
      midiSongAddTempo (
        mf, 1, (int) TRANSPORT->bpm);

      midiFileSetPPQN (mf, TICKS_PER_QUARTER_NOTE);

      midiFileSetVersion (mf, 0);

      /* common time: 4 crochet beats, per bar */
      midiSongAddSimpleTimeSig (
        mf, 1, TRANSPORT->beats_per_bar,
        TRANSPORT->ticks_per_beat);

      Track * track;
      for (i = 0; i < TRACKLIST->num_tracks; i++)
        {
          track = TRACKLIST->tracks[i];

          if (track_has_piano_roll (track))
            {
              /* write track to midi file */
              track_write_to_midi_file (
                track, mf);
            }
          info->progress =
            (double) i /
            (double) TRACKLIST->num_tracks;
        }

      midiFileClose(mf);
    }
  info->progress = 1.0;
}

/**
 * Exports an audio file based on the given
 * settings.
 *
 * TODO move some things into functions.
 */
void
exporter_export (ExportSettings * info)
{
  if (info->format == AUDIO_FORMAT_MIDI)
    {
      export_midi (info);
    }
  else
    {
      export_audio (info);
    }
}

