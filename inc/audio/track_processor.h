/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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
 * Track processor.
 */

#ifndef __AUDIO_TRACK_PROCESSOR_H__
#define __AUDIO_TRACK_PROCESSOR_H__

#include "audio/port.h"
#include "utils/types.h"
#include "utils/yaml.h"

typedef struct StereoPorts StereoPorts;
typedef struct Port Port;
typedef struct Track Track;

/**
 * @addtogroup audio
 *
 * @{
 */

/**
 * A TrackProcessor is a processor that is used as
 * the first entry point when processing a track.
 */
typedef struct TrackProcessor
{
  /**
   * L & R audio input ports, if audio.
   */
  StereoPorts *    stereo_in;

  /**
   * L & R audio output ports, if audio.
   */
  StereoPorts *    stereo_out;

  /**
   * MIDI in Port.
   *
   * This port is for receiving MIDI signals from
   * an external MIDI source.
   *
   * This is also where piano roll, midi in and midi
   * manual press will be routed to and this will
   * be the port used to pass midi to the plugins.
   */
  Port *           midi_in;

  /**
   * MIDI out port, if MIDI.
   */
  Port *           midi_out;

  /**
   * MIDI input for receiving MIDI signals from
   * the piano roll (i.e., MIDI notes inside
   * regions) or other sources.
   *
   * This will not be a separately exposed port
   * during processing. It will be processed by
   * the TrackProcessor internally.
   */
  Port *           piano_roll;

  /**
   * Current dBFS after procesing each output port.
   *
   * Transient variables only used by the GUI.
   */
  float            l_port_db;
  float            r_port_db;

  /** Pointer back to owner Track. */
  Track *          track;
} TrackProcessor;

static const cyaml_schema_field_t
track_processor_fields_schema[] =
{
	CYAML_FIELD_MAPPING_PTR (
    "midi_in",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    TrackProcessor, midi_in,
    port_fields_schema),
	CYAML_FIELD_MAPPING_PTR (
    "midi_out",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    TrackProcessor, midi_out,
    port_fields_schema),
	CYAML_FIELD_MAPPING_PTR (
    "piano_roll",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    TrackProcessor, piano_roll,
    port_fields_schema),
	CYAML_FIELD_MAPPING_PTR (
    "stereo_in",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    TrackProcessor, stereo_in,
    stereo_ports_fields_schema),
	CYAML_FIELD_MAPPING_PTR (
    "stereo_out",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    TrackProcessor, stereo_out,
    stereo_ports_fields_schema),

	CYAML_FIELD_END
};

static const cyaml_schema_value_t
track_processor_schema =
{
	CYAML_VALUE_MAPPING (
    CYAML_FLAG_POINTER,
	  TrackProcessor, track_processor_fields_schema),
};

/**
 * Inits a TrackProcessor after a project is loaded.
 */
void
track_processor_init_loaded (
  TrackProcessor * self);

/**
 * Inits the TrackProcessor to default values.
 *
 * @param self The TrackProcessor to init.
 * @param track The owner Track.
 */
void
track_processor_init (
  TrackProcessor * self,
  Track *          track);

/**
 * Clears all buffers.
 */
void
track_processor_clear_buffers (
  TrackProcessor * self);

/**
 * Disconnects all ports connected to the
 * TrackProcessor.
 */
void
track_processor_disconnect_all (
  TrackProcessor * self);

/**
 * Process the TrackProcessor.
 *
 * @param g_start_frames The global start frames.
 * @param local_offset The local start frames.
 * @param nframes The number of frames to process.
 */
void
track_processor_process (
  TrackProcessor * self,
  const long       g_start_frames,
  const nframes_t  local_offset,
  const nframes_t  nframes);

/**
 * Disconnect the TrackProcessor's stereo out ports
 * from the prefader.
 *
 * Used when there is no plugin in the channel.
 */
void
track_processor_disconnect_from_prefader (
  TrackProcessor * self);

/**
 * Connects the TrackProcessor's stereo out ports to
 * the Channel's prefader in ports.
 *
 * Used when deleting the only plugin left.
 */
void
track_processor_connect_to_prefader (
  TrackProcessor * self);

/**
 * Disconnect the TrackProcessor's out ports
 * from the Plugin's input ports.
 */
void
track_processor_disconnect_from_plugin (
  TrackProcessor * self,
  Plugin         * pl);

/**
 * Connect the TrackProcessor's out ports to the
 * Plugin's input ports.
 */
void
track_processor_connect_to_plugin (
  TrackProcessor * self,
  Plugin         * pl);

/**
 * Frees the members of the TrackProcessor.
 */
void
track_processor_free_members (
  TrackProcessor * self);

/**
 * @}
 */

#endif
