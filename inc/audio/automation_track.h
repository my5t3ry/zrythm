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

#ifndef __AUDIO_AUTOMATION_TRACK_H__
#define __AUDIO_AUTOMATION_TRACK_H__

#include "audio/automatable.h"
#include "audio/automation_point.h"
#include "audio/position.h"
#include "audio/region.h"

#define MAX_AUTOMATION_POINTS 1200

typedef struct Port Port;
typedef struct _AutomationTrackWidget
  AutomationTrackWidget;
typedef struct Track Track;
typedef struct Automatable Automatable;
typedef struct AutomationTracklist
  AutomationTracklist;
typedef struct CustomButtonWidget
  CustomButtonWidget;

typedef struct AutomationTrack
{
  /** Index in parent AutomationTracklist. */
  int               index;

  /**
   * Details about the automatable this automation
   * track is for.
   */
  Automatable *     automatable;

  /**
   * Owner track.
   *
   * For convenience only.
   */
  Track *           track;

  /** Whether it has been created by the user
   * yet or not. */
  int               created;

  /** The automation Region's. */
  ZRegion **         regions;
  int               num_regions;
  size_t            regions_size;

  /**
   * Whether visible or not.
   *
   * Being created is a precondition for this.
   */
  int               visible;

  /** Y local to track. */
  int               y;

  /** Position of multipane handle. */
  int               height;

  /** Buttons used by the track widget */
  CustomButtonWidget * top_right_buttons[8];
  int                  num_top_right_buttons;
  CustomButtonWidget * top_left_buttons[8];
  int                  num_top_left_buttons;
  CustomButtonWidget * bot_right_buttons[8];
  int                  num_bot_right_buttons;
  CustomButtonWidget * bot_left_buttons[8];
  int                  num_bot_left_buttons;

  /** The widget. */
  //AutomationTrackWidget * widget;
} AutomationTrack;

static const cyaml_schema_field_t
  automation_track_fields_schema[] =
{
	CYAML_FIELD_INT (
    "index", CYAML_FLAG_DEFAULT,
    AutomationTrack, index),
  CYAML_FIELD_MAPPING_PTR (
    "automatable",
    CYAML_FLAG_DEFAULT,
    AutomationTrack, automatable,
    automatable_fields_schema),
  CYAML_FIELD_SEQUENCE_COUNT (
    "regions", CYAML_FLAG_POINTER,
    AutomationTrack, regions, num_regions,
    &region_schema, 0, CYAML_UNLIMITED),
	CYAML_FIELD_INT (
    "created", CYAML_FLAG_DEFAULT,
    AutomationTrack, created),
	CYAML_FIELD_INT (
    "visible", CYAML_FLAG_DEFAULT,
    AutomationTrack, visible),
	CYAML_FIELD_INT (
    "height", CYAML_FLAG_DEFAULT,
    AutomationTrack, height),

	CYAML_FIELD_END
};

static const cyaml_schema_value_t
  automation_track_schema =
{
	CYAML_VALUE_MAPPING (
    CYAML_FLAG_POINTER,
    AutomationTrack,
    automation_track_fields_schema),
};

void
automation_track_init_loaded (
  AutomationTrack * self);

/**
 * Creates an automation track for the given
 * automatable.
 */
AutomationTrack *
automation_track_new (Automatable *  automatable);

AutomationTracklist *
automation_track_get_automation_tracklist (
  AutomationTrack * self);

/**
 * Adds an automation ZRegion to the AutomationTrack.
 */
void
automation_track_add_region (
  AutomationTrack * self,
  ZRegion *          region);

/**
 * Returns the visible y offset from the top of
 * the track widget.
 */
//int
//automation_track_get_visible_y_offset (
  //AutomationTrack * self);

/**
 * Updates the frames of each position in each child
 * of the automation track recursively.
 */
void
automation_track_update_frames (
  AutomationTrack * self);

/**
 * Sets the index of the AutomationTrack in the
 * AutomationTracklist.
 */
void
automation_track_set_index (
  AutomationTrack * self,
  int               index);

/**
 * Sets the automatable to the automation track and
 * updates the GUI
 *
 * FIXME no definition implemented.
 */
void
automation_track_set_automatable (
  AutomationTrack * automation_track,
  Automatable *     a);

/**
 * Clones the AutomationTrack.
 */
AutomationTrack *
automation_track_clone (
  AutomationTrack * src);

/**
 * Frees the automation track.
 */
void
automation_track_free (AutomationTrack * at);

/**
 * Returns the automation point before the pos.
 */
AutomationPoint *
automation_track_get_ap_before_pos (
  const AutomationTrack * self,
  const Position *        pos);

/**
 * Returns the last ZRegion before the given
 * Position.
 */
ZRegion *
automation_track_get_region_before_pos (
  const AutomationTrack * self,
  const Position *        pos);

/**
 * Returns the actual parameter value at the given
 * position.
 *
 * If there is no automation point/curve during
 * the position, it returns the current value
 * of the parameter it is automating.
 */
float
automation_track_get_normalized_val_at_pos (
  AutomationTrack * at,
  Position *        pos);

/**
 * Returns the y pixels from the value based on the
 * allocation of the automation track.
 */
int
automation_track_get_y_px_from_normalized_val (
  AutomationTrack * self,
  float             normalized_val);

/**
 * Updates automation track & its GUI
 */
//void
//automation_track_update (AutomationTrack * at);

/**
 * Gets the last ZRegion in the AutomationTrack.
 */
ZRegion *
automation_track_get_last_region (
  AutomationTrack * self);

#endif // __AUDIO_AUTOMATION_TRACK_H__
