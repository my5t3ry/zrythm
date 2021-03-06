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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/audio_region.h"
#include "audio/automation_point.h"
#include "audio/automation_region.h"
#include "audio/chord_region.h"
#include "audio/chord_track.h"
#include "audio/marker_track.h"
#include "audio/midi_region.h"
#include "gui/backend/arranger_object.h"
#include "gui/backend/automation_selections.h"
#include "gui/backend/chord_selections.h"
#include "gui/backend/timeline_selections.h"
#include "gui/widgets/audio_region.h"
#include "gui/widgets/automation_arranger.h"
#include "gui/widgets/automation_editor_space.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/automation_region.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/chord_arranger.h"
#include "gui/widgets/chord_editor_space.h"
#include "gui/widgets/chord_object.h"
#include "gui/widgets/chord_region.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/marker.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_editor_space.h"
#include "gui/widgets/midi_modifier_arranger.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/region.h"
#include "gui/widgets/scale_object.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_panel.h"
#include "gui/widgets/track.h"
#include "gui/widgets/velocity.h"
#include "project.h"
#include "utils/cairo.h"
#include "utils/flags.h"
#include "utils/math.h"

#define TYPE(x) \
  ARRANGER_OBJECT_TYPE_##x

#define POSITION_TYPE(x) \
  ARRANGER_OBJECT_POSITION_TYPE_##x

#define FOREACH_TYPE(func) \
  func (REGION, ZRegion, region) \
  func (SCALE_OBJECT, ScaleObject, scale_object) \
  func (MARKER, Marker, marker) \
  func (MIDI_NOTE, MidiNote, midi_note) \
  func (VELOCITY, Velocity, velocity) \
  func (CHORD_OBJECT, ChordObject, chord_object) \
  func (AUTOMATION_POINT, AutomationPoint, \
        automation_point)

/**
 * Returns the ArrangerSelections corresponding
 * to the given object type.
 */
ArrangerSelections *
arranger_object_get_selections_for_type (
  ArrangerObjectType type)
{
  switch (type)
    {
    case TYPE (REGION):
    case TYPE (SCALE_OBJECT):
    case TYPE (MARKER):
      return (ArrangerSelections *) TL_SELECTIONS;
    case TYPE (MIDI_NOTE):
    case TYPE (VELOCITY):
      return (ArrangerSelections *) MA_SELECTIONS;
    case TYPE (CHORD_OBJECT):
      return (ArrangerSelections *) CHORD_SELECTIONS;
    case TYPE (AUTOMATION_POINT):
      return
        (ArrangerSelections *) AUTOMATION_SELECTIONS;
    default:
      return NULL;
    }
}

/**
 * Selects the object by adding it to its
 * corresponding selections or making it the
 * only selection.
 *
 * @param select 1 to select, 0 to deselect.
 * @param append 1 to append, 0 to make it the only
 *   selection.
 */
void
arranger_object_select (
  ArrangerObject * self,
  const int        select,
  const int        append)
{
  ArrangerSelections * selections =
    arranger_object_get_selections_for_type (
      self->type);

  if (select)
    {
      if (!append)
        {
          arranger_selections_clear (
            selections);
        }
      arranger_selections_add_object (
        selections, self);
    }
  else
    {
      arranger_selections_remove_object (
        selections, self);
    }
}

/**
 * Returns the number of loops in the ArrangerObject,
 * optionally including incomplete ones.
 */
int
arranger_object_get_num_loops (
  ArrangerObject * self,
  const int        count_incomplete)
{
  int i = 0;
  long loop_size =
    arranger_object_get_loop_length_in_frames (
      self);
  g_return_val_if_fail (loop_size > 0, 0);
  long full_size =
    arranger_object_get_length_in_frames (self);
  long loop_start =
    self->loop_start_pos.frames -
    self->clip_start_pos.frames;
  long curr_ticks = loop_start;

  while (curr_ticks < full_size)
    {
      i++;
      curr_ticks += loop_size;
    }

  if (!count_incomplete)
    i--;

  return i;
}

static void
set_to_region_object (
  ZRegion * src,
  ZRegion * dest)
{
  g_return_if_fail (src && dest);
}

static void
set_to_midi_note_object (
  MidiNote * src,
  MidiNote * dest)
{
  g_return_if_fail (
    dest && dest->vel && src && src->vel);
  dest->vel->vel = src->vel->vel;
  dest->val = src->val;
}

/**
 * Sets the dest object's values to the main
 * src object's values.
 */
void
arranger_object_set_to_object (
  ArrangerObject * dest,
  ArrangerObject * src)
{
  g_return_if_fail (src && dest);

  /* reset positions */
  dest->pos = src->pos;
  if (src->has_length)
    {
      dest->end_pos = src->end_pos;
    }
  if (src->can_loop)
    {
      dest->clip_start_pos = src->clip_start_pos;
      dest->loop_start_pos = src->loop_start_pos;
      dest->loop_end_pos = src->loop_end_pos;
    }
  if (src->can_fade)
    {
      dest->fade_in_pos = src->fade_in_pos;
      dest->fade_out_pos = src->fade_out_pos;
    }

  /* reset other members */
  switch (src->type)
    {
    case TYPE (REGION):
      set_to_region_object (
        (ZRegion *) src, (ZRegion *) dest);
      break;
    case TYPE (MIDI_NOTE):
      set_to_midi_note_object (
        (MidiNote *) src, (MidiNote *) dest);
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * dest_co =
          (ChordObject *) dest;
        ChordObject * src_co =
          (ChordObject *) src;
        dest_co->index = src_co->index;
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * dest_ap =
          (AutomationPoint *) dest;
        AutomationPoint * src_ap =
          (AutomationPoint *) src;
        dest_ap->fvalue = src_ap->fvalue;
      }
      break;
    default:
      break;
    }
}

/**
 * Returns if the object is in the selections.
 */
int
arranger_object_is_selected (
  ArrangerObject * self)
{
  ArrangerSelections * selections =
    arranger_object_get_selections_for_type (
      self->type);

  return
    arranger_selections_contains_object (
      selections, self);
}

/**
 * If the object is part of a ZRegion, returns it,
 * otherwise returns NULL.
 */
ZRegion *
arranger_object_get_region (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        return ap->region;
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * mn =
          (MidiNote *) self;
        return mn->region;
      }
      break;
    case ARRANGER_OBJECT_TYPE_VELOCITY:
      {
        Velocity * vel =
          (Velocity *) self;
        return vel->midi_note->region;
      }
      break;
    case ARRANGER_OBJECT_TYPE_CHORD_OBJECT:
      {
        ChordObject * co =
          (ChordObject *) self;
        return co->region;
      }
      break;
    default:
      break;
    }
  return NULL;
}

/**
 * Gets a pointer to the Position in the
 * ArrangerObject matching the given arguments.
 *
 * @param cache 1 to return the cached Position
 *   instead of the main one.
 */
static Position *
get_position_ptr (
  ArrangerObject *           self,
  ArrangerObjectPositionType pos_type,
  int                        cached)
{
  switch (pos_type)
    {
    case ARRANGER_OBJECT_POSITION_TYPE_START:
      if (cached)
        return &self->cache_pos;
      else
        return &self->pos;
    case ARRANGER_OBJECT_POSITION_TYPE_END:
      if (cached)
        return &self->cache_end_pos;
      else
        return &self->end_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_CLIP_START:
      if (cached)
        return &self->cache_clip_start_pos;
      else
        return &self->clip_start_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_START:
      if (cached)
        return &self->cache_loop_start_pos;
      else
        return &self->loop_start_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_END:
      if (cached)
        return &self->cache_loop_end_pos;
      else
        return &self->loop_end_pos;
    }
  g_return_val_if_reached (NULL);
}

/**
 * Returns if the given Position is valid.
 *
 * @param pos The position to set to.
 * @param pos_type The type of Position to set in the
 *   ArrangerObject.
 * @param cached Set to 1 to validate based on the
 *   cached positions instead of the main ones.
 * @param validate Validate the Position before
 *   setting it.
 */
int
arranger_object_is_position_valid (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType pos_type,
  const int                  cached)
{
  int is_valid = 0;
  switch (pos_type)
    {
    case ARRANGER_OBJECT_POSITION_TYPE_START:
      if (self->has_length)
        {
          Position * end_pos =
            cached ?
              &self->cache_end_pos :
              &self->end_pos;
          is_valid =
            position_is_before (
              pos, end_pos) &&
            position_is_after_or_equal (
              pos, &POSITION_START);
        }
      else if (self->is_pos_global)
        {
          is_valid =
            position_is_after_or_equal (
              pos, &POSITION_START);
        }
      else
        {
          is_valid = 1;
        }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_START:
      {
        Position * loop_end_pos =
          cached ?
            &self->cache_loop_end_pos :
            &self->loop_end_pos;
        is_valid =
          position_is_before (
            pos, loop_end_pos) &&
          position_is_after_or_equal (
            pos, &POSITION_START);
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_END:
      {
        /* TODO */
        is_valid = 1;
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_CLIP_START:
      {
        Position * loop_end_pos =
          cached ?
            &self->cache_loop_end_pos :
            &self->loop_end_pos;
        is_valid =
          position_is_before (
            pos, loop_end_pos) &&
          position_is_after_or_equal (
            pos, &POSITION_START);
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_END:
      {
        /* TODO */
        is_valid = 1;
      }
      break;
    default:
      break;
    }

  return is_valid;
}

/**
 * Sets the Position  all of the object's linked
 * objects (see ArrangerObjectInfo)
 *
 * @param pos The position to set to.
 * @param pos_type The type of Position to set in the
 *   ArrangerObject.
 * @param cached Set to 1 to set the cached positions
 *   instead of the main ones.
 * @param validate Validate the Position before
 *   setting it.
 */
void
arranger_object_set_position (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType pos_type,
  const int                  cached,
  const int                  validate)
{
  g_return_if_fail (self && pos);

  /* return if validate is on and position is
   * invalid */
  if (validate &&
      !arranger_object_is_position_valid (
        self, pos, pos_type, cached))
    return;

  Position * pos_ptr;
  pos_ptr =
    get_position_ptr (
      self, pos_type, cached);
  g_return_if_fail (pos_ptr);
  position_set_to_pos (pos_ptr, pos);
}

/**
 * Returns the type as a string.
 */
const char *
arranger_object_stringize_type (
  ArrangerObjectType type)
{
  return arranger_object_type_strings[type].str;
}

/**
 * Prints debug information about the given
 * object.
 */
void
arranger_object_print (
  ArrangerObject * self)
{
  g_return_if_fail (self);

  const char * type =
    arranger_object_stringize_type (self->type);

  char positions[500];
  if (self->has_length)
    {
      sprintf (
        positions, "(%d.%d.%d.%d ~ %d.%d.%d.%d)",
        self->pos.bars, self->pos.beats,
        self->pos.sixteenths, self->pos.ticks,
        self->end_pos.bars, self->end_pos.beats,
        self->end_pos.sixteenths,
        self->end_pos.ticks);
    }
  else
    {
      sprintf (
        positions, "(%d.%d.%d.%d)",
        self->pos.bars, self->pos.beats,
        self->pos.sixteenths, self->pos.ticks);
    }
  g_message (
    "%s %s",
    type, positions);
}

/**
 * Moves the object by the given amount of
 * ticks.
 *
 * @param use_cached_pos Add the ticks to the cached
 *   Position instead of its current Position.
 */
void
arranger_object_move (
  ArrangerObject *         self,
  const long               ticks,
  const int                use_cached_pos)
{
  if (arranger_object_type_has_length (self->type))
    {
      /* start pos */
      Position tmp;
      if (use_cached_pos)
        position_set_to_pos (
          &tmp, &self->cache_pos);
      else
        position_set_to_pos (
          &tmp, &self->pos);
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_START,
        F_NO_CACHED, F_NO_VALIDATE);

      /* end pos */
      if (use_cached_pos)
        position_set_to_pos (
          &tmp, &self->cache_end_pos);
      else
        position_set_to_pos (
          &tmp, &self->end_pos);
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_END,
        F_NO_CACHED, F_NO_VALIDATE);
    }
  else
    {
      Position tmp;
      if (use_cached_pos)
        position_set_to_pos (
          &tmp, &self->cache_pos);
      else
        position_set_to_pos (
          &tmp, &self->pos);
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_START,
        F_NO_CACHED, F_NO_VALIDATE);
    }
}

/**
 * Getter.
 */
void
arranger_object_get_pos (
  const ArrangerObject * self,
  Position *             pos)
{
  position_set_to_pos (
    pos, &self->pos);
}

/**
 * Getter.
 */
void
arranger_object_get_end_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->end_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_clip_start_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->clip_start_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_loop_start_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->loop_start_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_loop_end_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->loop_end_pos);
}

static void
init_loaded_midi_note (
  MidiNote * self)
{
  midi_note_set_region (self, self->region);
  self->vel->midi_note = self;
  arranger_object_init_loaded (
    (ArrangerObject *) self->vel);
}

static void
init_loaded_region (
  ZRegion * self)
{
  self->linked_region =
    region_find_by_name (
      self->linked_region_name);

  int i;
  switch (self->type)
    {
    case REGION_TYPE_AUDIO:
      {
      }
      break;
    case REGION_TYPE_MIDI:
      {
        MidiNote * mn;
        for (i = 0; i < self->num_midi_notes; i++)
          {
            mn = self->midi_notes[i];
            mn->region = self;
            arranger_object_init_loaded (
              (ArrangerObject *) mn);
          }
        self->midi_notes_size =
          (size_t) self->num_midi_notes;
      }
      break;
    case REGION_TYPE_CHORD:
      {
        ChordObject * chord;
        for (i = 0; i < self->num_chord_objects;
             i++)
          {
            chord = self->chord_objects[i];
            chord->region = self;
            arranger_object_init_loaded (
              (ArrangerObject *) chord);
          }
        self->chord_objects_size =
          (size_t) self->num_chord_objects;
        }
      break;
    case REGION_TYPE_AUTOMATION:
      {
        AutomationPoint * ap;
        for (i = 0; i < self->num_aps; i++)
          {
            ap = self->aps[i];
            ap->region = self;
            arranger_object_init_loaded (
              (ArrangerObject *) ap);
          }
        self->aps_size =
          (size_t) self->num_aps;
      }
      break;
    }

  if (region_type_has_lane (self->type))
    {
      region_set_lane (self, self->lane);
    }
  else if (self->type == REGION_TYPE_AUTOMATION)
    {
      region_set_automation_track (
        self, self->at);
    }
}

/**
 * Initializes the object after loading a Project.
 */
void
arranger_object_init_loaded (
  ArrangerObject * self)
{
  g_return_if_fail (self->type > TYPE (NONE));

  /* init positions */
  self->cache_pos = self->pos;
  self->cache_end_pos = self->end_pos;

  switch (self->type)
    {
    case TYPE (REGION):
      init_loaded_region (
        (ZRegion *) self);
      break;
    case TYPE (MIDI_NOTE):
      init_loaded_midi_note (
        (MidiNote *) self);
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        automation_point_set_region_and_index (
          ap, ap->region, ap->index);
      }
      break;
    case TYPE (VELOCITY):
      {
        Velocity * vel =
          (Velocity *) self;
        velocity_set_midi_note (
          vel, vel->midi_note);
      }
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * co = (ChordObject *) self;
        co->region =
          region_find_by_name (
            co->region_name);
      }
      break;
    case TYPE (SCALE_OBJECT):
      break;
    default:
      break;
    }
}

/**
 * Returns the length of the ArrangerObject (if
 * it has length) in ticks.
 *
 * (End Position - start Position).
 */
long
arranger_object_get_length_in_ticks (
  ArrangerObject * self)
{
  g_return_val_if_fail (self->has_length, 0);
  return
    self->end_pos.total_ticks -
    self->pos.total_ticks;
}

/**
 * Returns the length of the ArrangerObject (if
 * it has length) in frames.
 *
 * (End Position - start Position).
 */
long
arranger_object_get_length_in_frames (
  ArrangerObject * self)
{
  g_return_val_if_fail (self->has_length, 0);
  return
    self->end_pos.frames -
    self->pos.frames;
}

/**
 * Returns the length of the loop in ticks.
 */
long
arranger_object_get_loop_length_in_ticks (
  ArrangerObject * self)
{
  g_return_val_if_fail (self->has_length, 0);
  return
    self->loop_end_pos.total_ticks -
    self->loop_start_pos.total_ticks;
}

/**
 * Returns the length of the loop in ticks.
 */
long
arranger_object_get_loop_length_in_frames (
  ArrangerObject * self)
{
  g_return_val_if_fail (self->has_length, 0);
  return
    self->loop_end_pos.frames -
    self->loop_start_pos.frames;
}

/**
 * Updates the frames of each position in each child
 * recursively.
 */
void
arranger_object_update_frames (
  ArrangerObject * self)
{
  position_update_ticks_and_frames (
    &self->pos);
  if (self->has_length)
    {
      position_update_ticks_and_frames (
        &self->end_pos);
    }
  if (self->can_loop)
    {
      position_update_ticks_and_frames (
        &self->clip_start_pos);
      position_update_ticks_and_frames (
        &self->loop_start_pos);
      position_update_ticks_and_frames (
        &self->loop_end_pos);
    }
  if (self->can_fade)
    {
      position_update_ticks_and_frames (
        &self->fade_in_pos);
      position_update_ticks_and_frames (
        &self->fade_out_pos);
    }

  int i;
  ZRegion * r;
  switch (self->type)
    {
    case TYPE (REGION):
      r = (ZRegion *) self;
      for (i = 0; i < r->num_midi_notes; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->midi_notes[i]);
        }
      for (i = 0; i < r->num_unended_notes; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->unended_notes[i]);
        }

      for (i = 0; i < r->num_aps; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->aps[i]);
        }

      for (i = 0; i < r->num_chord_objects; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->chord_objects[i]);
        }
      break;
    default:
      break;
    }
}

static void
add_ticks_to_region_children (
  ZRegion *   self,
  const long ticks)
{
  switch (self->type)
    {
    case REGION_TYPE_MIDI:
      for (int i = 0; i < self->num_midi_notes; i++)
        {
          arranger_object_move (
            (ArrangerObject *) self->midi_notes[i],
            ticks, F_NO_CACHED);
        }
      break;
    case REGION_TYPE_AUDIO:
      break;
    case REGION_TYPE_AUTOMATION:
      for (int i = 0; i < self->num_aps; i++)
        {
          arranger_object_move (
            (ArrangerObject *) self->aps[i],
            ticks, F_NO_CACHED);
        }
      break;
    case REGION_TYPE_CHORD:
      for (int i = 0; i < self->num_chord_objects;
           i++)
        {
          arranger_object_move (
            (ArrangerObject *)
              self->chord_objects[i],
            ticks, F_NO_CACHED);
        }
      break;
    }
}

/**
 * Adds the given ticks to each included object.
 */
void
arranger_object_add_ticks_to_children (
  ArrangerObject * self,
  const long       ticks)
{
  if (self->type == TYPE (REGION))
    {
      add_ticks_to_region_children (
        (ZRegion *) self, ticks);
    }
}

/**
 * Resizes the object on the left side or right
 * side by given amount of ticks, for objects that
 * do not have loops (currently none? keep it as
 * reference).
 *
 * @param left 1 to resize left side, 0 to resize
 *   right side.
 * @param ticks Number of ticks to resize.
 * @param loop Whether this is a loop-resize (1) or
 *   a normal resize (0). Only used if the object
 *   can have loops.
 */
void
arranger_object_resize (
  ArrangerObject * self,
  const int        left,
  const int        loop,
  const long       ticks)
{
  Position tmp;
  if (left)
    {
      tmp = self->pos;
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_START,
        F_NO_CACHED, F_NO_VALIDATE);

      if (self->can_loop)
        {
          tmp = self->loop_end_pos;
          position_add_ticks (
            &tmp, - ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
            F_NO_CACHED, F_NO_VALIDATE);
        }

      /* move containing items */
      arranger_object_add_ticks_to_children (
        self, - ticks);
      if (loop && self->can_loop)
        {
          tmp = self->loop_start_pos;
          position_add_ticks (
            &tmp, - ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_LOOP_START,
            F_NO_CACHED, F_NO_VALIDATE);
        }
    }
  else
    {
      tmp = self->end_pos;
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_END,
        F_NO_CACHED, F_NO_VALIDATE);

      if (!loop && self->can_loop)
        {
          tmp = self->loop_end_pos;
          position_add_ticks (
            &tmp, ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
            F_NO_CACHED, F_NO_VALIDATE);
        }
    }
}

static void
post_deserialize_children (
  ArrangerObject * self)
{
  if (self->type == TYPE (REGION))
    {
      ZRegion * r = (ZRegion *) self;
      MidiNote * mn;
      for (int i = 0; i < r->num_midi_notes; i++)
        {
          mn = r->midi_notes[i];
          mn->region = r;
          arranger_object_post_deserialize (
            (ArrangerObject *) mn);
        }
    }
}

void
arranger_object_post_deserialize (
  ArrangerObject * self)
{
  g_return_if_fail (self);

  post_deserialize_children (self);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_START,
    F_NO_CACHED, F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_end_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_END,
    F_NO_CACHED, F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_clip_start_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_CLIP_START,
    F_NO_CACHED, F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_loop_start_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_LOOP_START,
    F_NO_CACHED, F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_loop_end_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
    F_NO_CACHED, F_VALIDATE);
}

/**
 * Validates the given Position.
 *
 * @return 1 if valid, 0 otherwise.
 */
int
arranger_object_validate_pos (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType type)
{
  switch (self->type)
    {
    case TYPE (REGION):
      switch (type)
        {
        case POSITION_TYPE (START):
          return
            position_is_before (
              pos, &self->end_pos) &&
            position_is_after_or_equal (
              pos, &POSITION_START);
          break;
        default:
          break;
        }
      break;
    default:
      break;
    }

  return 1;
}

/**
 * Returns the Track this ArrangerObject is in.
 */
Track *
arranger_object_get_track (
  ArrangerObject * self)
{
  g_return_val_if_fail (self, NULL);

  Track * track = NULL;

  switch (self->type)
    {
    case TYPE (REGION):
      {
        ZRegion * r = (ZRegion *) self;
        track =
          TRACKLIST->tracks[r->track_pos];
      }
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * chord = (ChordObject *) self;
        ZRegion * r = chord->region;
        g_return_val_if_fail (r, NULL);
        track =
          TRACKLIST->tracks[r->track_pos];
      }
      break;
    case TYPE (SCALE_OBJECT):
      {
        ScaleObject * scale = (ScaleObject *) self;
        track =
          TRACKLIST->tracks[scale->track_pos];
      }
      break;
    case TYPE (MARKER):
      {
        Marker * marker = (Marker *) self;
        track =
          TRACKLIST->tracks[marker->track_pos];
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        AutomationTrack * at =
          automation_point_get_automation_track (
            ap);
        g_return_val_if_fail (
          at && at->track, NULL);
        track = at->track;
      }
      break;
    case TYPE (MIDI_NOTE):
      {
        MidiNote * mn = (MidiNote *) self;
        g_return_val_if_fail (
          mn->region, NULL);
        track =
          TRACKLIST->tracks[mn->region->track_pos];
      }
      break;
    case TYPE (VELOCITY):
      {
        Velocity * vel = (Velocity *) self;
        g_return_val_if_fail (
          vel->midi_note && vel->midi_note->region,
          NULL);
        track =
          TRACKLIST->tracks[
            vel->midi_note->region->track_pos];
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  g_return_val_if_fail (track, NULL);

  return track;
}

/**
 * Gets the arranger for this arranger object.
 */
ArrangerWidget *
arranger_object_get_arranger (
  ArrangerObject * self)
{
  g_return_val_if_fail (self, NULL);

  Track * track =
    arranger_object_get_track (self);
  g_return_val_if_fail (track, NULL);

  ArrangerWidget * arranger = NULL;
  switch (self->type)
    {
    case TYPE (REGION):
      {
        if (track->pinned)
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (CHORD_OBJECT):
      arranger =
        (ArrangerWidget *) (MW_CHORD_ARRANGER);
      break;
    case TYPE (SCALE_OBJECT):
      {
        if (track->pinned)
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (MARKER):
      {
        if (track->pinned)
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (AUTOMATION_POINT):
      arranger =
        (ArrangerWidget *) (MW_AUTOMATION_ARRANGER);
      break;
    case TYPE (MIDI_NOTE):
      arranger =
        (ArrangerWidget *) (MW_MIDI_ARRANGER);
      break;
    case TYPE (VELOCITY):
      arranger =
        (ArrangerWidget *) (MW_MIDI_MODIFIER_ARRANGER);
      break;
    default:
      g_return_val_if_reached (NULL);
    }

  g_warn_if_fail (arranger);
  return arranger;
}

/**
 * Returns if the lane counterpart should be visible.
 */
int
arranger_object_should_lane_be_visible (
  ArrangerObject * self)
{
  if (self->type == TYPE (REGION))
    {
      return
        arranger_object_get_track (self)->
          lanes_visible;
    }
  else
    {
      return 0;
    }
}

/**
 * Returns if the cached object should be visible,
 * ie, while copy- moving (ctrl+drag) we want to
 * show both the object at its original position
 * and the current object.
 *
 * This refers to the object at its original
 * position.
 */
int
arranger_object_should_orig_be_visible (
  ArrangerObject * self)
{
  g_return_val_if_fail (self, 0);

  ArrangerWidget * arranger =
    arranger_object_get_arranger (self);

  /* check trans/non-trans visiblity */
  if (ARRANGER_WIDGET_GET_ACTION (
        arranger, MOVING) ||
      ARRANGER_WIDGET_GET_ACTION (
        arranger, CREATING_MOVING))
    {
      return 0;
    }
  else if (
    ARRANGER_WIDGET_GET_ACTION (
      arranger, MOVING_COPY) ||
    ARRANGER_WIDGET_GET_ACTION (
      arranger, MOVING_LINK))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

static ArrangerObject *
find_region (
  ZRegion * self)
{
  ArrangerObject * obj =
    (ArrangerObject *)
    region_find_by_name (self->name);
  ArrangerObject * self_obj =
    (ArrangerObject *) self;
  g_warn_if_fail (
    position_is_equal (
      &self_obj->pos, &obj->pos) &&
    position_is_equal (
      &self_obj->end_pos, &obj->end_pos));

  return obj;
}

static ArrangerObject *
find_chord_object (
  ChordObject * clone)
{
  /* get actual region - clone's region might be
   * an unused clone */
  ZRegion * r =
    region_find_by_name (clone->region_name);
  g_return_val_if_fail (r, NULL);

  ChordObject * chord;
  for (int i = 0; i < r->num_chord_objects; i++)
    {
      chord = r->chord_objects[i];
      if (chord_object_is_equal (
            chord, clone))
        return (ArrangerObject *) chord;
    }
  return NULL;
}

static ArrangerObject *
find_scale_object (
  ScaleObject * clone)
{
  for (int i = 0;
       i < P_CHORD_TRACK->num_scales; i++)
    {
      if (scale_object_is_equal (
            P_CHORD_TRACK->scales[i],
            clone))
        {
          return
            (ArrangerObject *)
            P_CHORD_TRACK->scales[i];
        }
    }
  return NULL;
}

static ArrangerObject *
find_marker (
  Marker * clone)
{
  for (int i = 0;
       i < P_MARKER_TRACK->num_markers; i++)
    {
      if (marker_is_equal (
            P_MARKER_TRACK->markers[i],
            clone))
        return
          (ArrangerObject *)
          P_MARKER_TRACK->markers[i];
    }
  return NULL;
}

static ArrangerObject *
find_automation_point (
  AutomationPoint * src)
{
  ZRegion * region = src->region;
  if (!region)
    {
      region =
        region_find_by_name (
          src->region_name);
    }
  g_return_val_if_fail (region, NULL);

  /* the src region might be an unused clone, find
   * the actual region. */
  region =
    (ZRegion *)
    arranger_object_find (
      (ArrangerObject *) region);

  int i;
  AutomationPoint * ap;
  for (i = 0; i < region->num_aps; i++)
    {
      ap = region->aps[i];
      if (automation_point_is_equal (src, ap))
        return (ArrangerObject *) ap;
    }

  return NULL;
}

static ArrangerObject *
find_midi_note (
  MidiNote * src)
{
  ZRegion * r =
    region_find_by_name (src->region_name);
  g_return_val_if_fail (r, NULL);

  for (int i = 0; i < r->num_midi_notes; i++)
    {
      if (midi_note_is_equal (
            r->midi_notes[i], src))
        return (ArrangerObject *) r->midi_notes[i];
    }
  return NULL;
}

/**
 * Returns the ArrangerObject matching the
 * given one.
 *
 * This should be called when we have a copy or a
 * clone, to get the actual region in the project.
 */
ArrangerObject *
arranger_object_find (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case TYPE (REGION):
      return
        find_region ((ZRegion *) self);
    case TYPE (CHORD_OBJECT):
      return
        find_chord_object ((ChordObject *) self);
    case TYPE (SCALE_OBJECT):
      return
        find_scale_object ((ScaleObject *) self);
    case TYPE (MARKER):
      return
        find_marker ((Marker *) self);
    case TYPE (AUTOMATION_POINT):
      return
        find_automation_point (
          (AutomationPoint *) self);
    case TYPE (MIDI_NOTE):
      return
        find_midi_note ((MidiNote *) self);
    case TYPE (VELOCITY):
      {
        Velocity * clone =
          (Velocity *) self;
        MidiNote * mn =
          (MidiNote *)
          find_midi_note (clone->midi_note);
        g_return_val_if_fail (mn && mn->vel, NULL);
        return (ArrangerObject *) mn->vel;
      }
    default:
      g_return_val_if_reached (NULL);
    }
  g_return_val_if_reached (NULL);
}

static ArrangerObject *
clone_region (
  ZRegion *                region,
  ArrangerObjectCloneFlag flag)
{
  int i, j;

  ArrangerObject * r_obj =
    (ArrangerObject *) region;
  ZRegion * new_region = NULL;
  switch (region->type)
    {
    case REGION_TYPE_MIDI:
      {
        ZRegion * mr =
          midi_region_new (
            &r_obj->pos, &r_obj->end_pos);
        ZRegion * mr_orig = region;
        if (flag == ARRANGER_OBJECT_CLONE_COPY ||
            flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
          {
            for (i = 0;
                 i < mr_orig->num_midi_notes; i++)
              {
                mr_orig->midi_notes[i]->region =
                  mr_orig;
                MidiNote * mn =
                  (MidiNote *)
                  arranger_object_clone (
                    (ArrangerObject *)
                    mr_orig->midi_notes[i],
                    ARRANGER_OBJECT_CLONE_COPY_MAIN);

                midi_region_add_midi_note (
                  mr, mn, 0);
              }
          }

        new_region = (ZRegion *) mr;
      }
    break;
    case REGION_TYPE_AUDIO:
      {
        ZRegion * ar =
          audio_region_new (
            region->pool_id, NULL, NULL, -1,
            0, &r_obj->pos);

        new_region = ar;
        new_region->pool_id = region->pool_id;
      }
    break;
    case REGION_TYPE_AUTOMATION:
      {
        ZRegion * ar  =
          automation_region_new (
            &r_obj->pos, &r_obj->end_pos);
        ZRegion * ar_orig = region;

        AutomationPoint * src_ap, * dest_ap;

        /* add automation points */
        for (j = 0; j < ar_orig->num_aps; j++)
          {
            src_ap = ar_orig->aps[j];
            ArrangerObject * src_ap_obj =
              (ArrangerObject *) src_ap;
            dest_ap =
              automation_point_new_float (
                src_ap->fvalue,
                src_ap->normalized_val,
                &src_ap_obj->pos, F_MAIN);
            automation_region_add_ap (
              ar, dest_ap);
          }

        new_region = ar;
      }
      break;
    case REGION_TYPE_CHORD:
      {
        ZRegion * cr =
          chord_region_new (
            &r_obj->pos, &r_obj->end_pos);
        ZRegion * cr_orig = region;
        if (flag == ARRANGER_OBJECT_CLONE_COPY ||
            flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
          {
            ChordObject * co;
            for (i = 0;
                 i < cr_orig->num_chord_objects;
                 i++)
              {
                co =
                  (ChordObject *)
                  arranger_object_clone (
                    (ArrangerObject *)
                    cr_orig->chord_objects[i],
                    ARRANGER_OBJECT_CLONE_COPY_MAIN);

                chord_region_add_chord_object (
                  cr, co);
              }
          }

        new_region = cr;
      }
      break;
    }

  g_return_val_if_fail (new_region, NULL);

  /* clone name */
  new_region->name = g_strdup (region->name);

  /* set track to NULL and remember track pos */
  new_region->lane = NULL;
  new_region->track_pos = -1;
  new_region->lane_pos = region->lane_pos;
  if (region->lane)
    {
      new_region->track_pos =
        region->lane->track_pos;
    }
  else
    {
      new_region->track_pos = region->track_pos;
    }
  new_region->at_index = region->at_index;

  return (ArrangerObject *) new_region;
}

/**
 * Returns a pointer to the name of the object,
 * if the object can have names.
 */
const char *
arranger_object_get_name (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r = (ZRegion *) self;
        return r->name;
      }
      break;
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * m = (Marker *) self;
        return m->name;
      }
      break;
    default:
      break;
    }
  return NULL;
}

static ArrangerObject *
clone_midi_note (
  MidiNote *              src,
  ArrangerObjectCloneFlag flag)
{
  g_return_val_if_fail (src->region, NULL);

  int is_main =
    flag == ARRANGER_OBJECT_CLONE_COPY_MAIN;

  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  MidiNote * mn =
    midi_note_new (
      src->region, &src_obj->pos,
      &src_obj->end_pos,
      src->val, src->vel->vel, is_main);
  mn->currently_listened = src->currently_listened;
  mn->last_listened_val = src->last_listened_val;

  return (ArrangerObject *) mn;
}

static ArrangerObject *
clone_chord_object (
  ChordObject *           src,
  ArrangerObjectCloneFlag flag)
{
  int is_main = 0;
  if (flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
    is_main = 1;

  ChordObject * chord =
    chord_object_new (
      src->region, src->index, is_main);

  return (ArrangerObject *) chord;
}

static ArrangerObject *
clone_scale_object (
  ScaleObject *           src,
  ArrangerObjectCloneFlag flag)
{
  int is_main = 0;
  if (flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
    is_main = 1;

  MusicalScale * musical_scale =
    musical_scale_clone (src->scale);
  ScaleObject * scale =
    scale_object_new (musical_scale, is_main);

  return (ArrangerObject *) scale;
}

static ArrangerObject *
clone_marker (
  Marker *                src,
  ArrangerObjectCloneFlag flag)
{
  int is_main = 0;
  if (flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
    is_main = 1;

  Marker * marker =
    marker_new (src->name, is_main);

  return (ArrangerObject *) marker;
}

static ArrangerObject *
clone_automation_point (
  AutomationPoint *       src,
  ArrangerObjectCloneFlag flag)
{
  int is_main = 0;
  if (flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
    is_main = 1;

  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  AutomationPoint * ap =
    automation_point_new_float (
      src->fvalue, src->normalized_val,
			&src_obj->pos, is_main);

  return (ArrangerObject *) ap;
}

/**
 * Clone the ArrangerObject.
 *
 * Creates a new object and either links to the
 * original or copies every field.
 */
ArrangerObject *
arranger_object_clone (
  ArrangerObject *        self,
  ArrangerObjectCloneFlag flag)
{
  g_return_val_if_fail (self, NULL);

  ArrangerObject * new_obj = NULL;
  switch (self->type)
    {
    case TYPE (REGION):
      new_obj =
        clone_region ((ZRegion *) self, flag);
      break;
    case TYPE (MIDI_NOTE):
      new_obj =
        clone_midi_note ((MidiNote *) self, flag);
      break;
    case TYPE (CHORD_OBJECT):
      new_obj =
        clone_chord_object (
          (ChordObject *) self, flag);
      break;
    case TYPE (SCALE_OBJECT):
      new_obj =
        clone_scale_object (
          (ScaleObject *) self, flag);
      break;
    case TYPE (AUTOMATION_POINT):
      new_obj =
        clone_automation_point (
          (AutomationPoint *) self, flag);
      break;
    case TYPE (MARKER):
      new_obj =
        clone_marker (
          (Marker *) self, flag);
      break;
    case TYPE (VELOCITY):
      {
        Velocity * src = (Velocity *) self;
        int is_main =
          flag == ARRANGER_OBJECT_CLONE_COPY_MAIN;
        new_obj =
          (ArrangerObject *)
          velocity_new (
            src->midi_note, src->vel, is_main);
      }
      break;
    default:
      g_return_val_if_reached (NULL);
    }
  g_return_val_if_fail (new_obj, NULL);

  /* set positions */
  new_obj->pos = self->pos;
  new_obj->cache_pos = self->pos;
  if (self->has_length)
    {
      new_obj->end_pos = self->end_pos;
      new_obj->cache_end_pos = self->end_pos;
    }
  if (self->can_loop)
    {
      new_obj->clip_start_pos = self->clip_start_pos;
      new_obj->cache_clip_start_pos =
        self->clip_start_pos;
      new_obj->loop_start_pos = self->loop_start_pos;
      new_obj->cache_loop_start_pos =
        self->cache_loop_start_pos;
      new_obj->loop_end_pos = self->loop_end_pos;
      new_obj->cache_loop_end_pos =
        self->cache_loop_end_pos;
    }
  if (self->can_fade)
    {
      new_obj->fade_in_pos = self->fade_in_pos;
      new_obj->cache_fade_in_pos =
        self->cache_fade_in_pos;
      new_obj->fade_out_pos = self->fade_out_pos;
      new_obj->cache_fade_out_pos =
        self->cache_fade_out_pos;
    }

  return new_obj;
}

/**
 * Splits the given object at the given Position,
 * deletes the original object and adds 2 new
 * objects in the same parent (Track or
 * AutomationTrack or Region).
 *
 * The given object must be the main object, as this
 * will create 2 new main objects.
 *
 * @param region The ArrangerObject to split. This
 *   ArrangerObject will be deleted.
 * @param pos The Position to split at.
 * @param pos_is_local If the position is local (1)
 *   or global (0).
 * @param r1 Address to hold the pointer to the
 *   newly created ArrangerObject 1.
 * @param r2 Address to hold the pointer to the
 *   newly created ArrangerObject 2.
 */
void
arranger_object_split (
  ArrangerObject *  self,
  const Position *  pos,
  const int         pos_is_local,
  ArrangerObject ** r1,
  ArrangerObject ** r2)
{
  /* create the new objects */
  *r1 =
    arranger_object_clone (
      self, ARRANGER_OBJECT_CLONE_COPY_MAIN);
  *r2 =
    arranger_object_clone (
      self, ARRANGER_OBJECT_CLONE_COPY_MAIN);

  /* get global/local positions (the local pos
   * is after traversing the loops) */
  Position globalp, localp;
  if (pos_is_local)
    {
      position_set_to_pos (&globalp, pos);
      position_add_ticks (
        &globalp, self->pos.total_ticks);
      position_set_to_pos (&localp, pos);
    }
  else
    {
      position_set_to_pos (&globalp, pos);
      if (self->type == ARRANGER_OBJECT_TYPE_REGION)
        {
          long localp_frames =
            region_timeline_frames_to_local (
              (ZRegion *) self, globalp.frames, 1);
          position_from_frames (
            &localp, localp_frames);
        }
      else
        {
          position_set_to_pos (&localp, &globalp);
        }
    }

  /* for first region just set the end pos */
  arranger_object_end_pos_setter (
    *r1, &globalp);

  /* for the second set the clip start and start
   * pos. */
  arranger_object_clip_start_pos_setter (
    *r2, &localp);
  arranger_object_pos_setter (
    *r2, &globalp);

  /* add them to the parent */
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        Track * track =
          arranger_object_get_track (self);
        ZRegion * src_region =
          (ZRegion *) self;
        ZRegion * region1 =
          (ZRegion *) *r1;
        ZRegion * region2 =
          (ZRegion *) *r2;
        track_add_region (
          track, region1, src_region->at,
          src_region->lane_pos,
          F_GEN_NAME, F_PUBLISH_EVENTS);
        track_add_region (
          track, region2, src_region->at,
          src_region->lane_pos,
          F_GEN_NAME, F_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * src_midi_note =
          (MidiNote *) self;
        ZRegion * parent_region =
          src_midi_note->region;
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *r1, 1);
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *r2, 1);
      }
      break;
    default:
      break;
    }

  /* generate widgets so update visibility in the
   * arranger can work */
  /*arranger_object_gen_widget (*r1);*/
  /*arranger_object_gen_widget (*r2);*/

  /* select them */
  ArrangerSelections * sel =
    arranger_object_get_selections_for_type (
      self->type);
  arranger_selections_remove_object (
    sel, self);
  arranger_selections_add_object (
    sel, *r1);
  arranger_selections_add_object (
    sel, *r2);

  /* change to r1 if the original region was the
   * clip editor region */
  if (CLIP_EDITOR->region == (ZRegion *) self)
    {
      clip_editor_set_region (
        CLIP_EDITOR, (ZRegion *) *r1);
    }

  /* remove and free the original object */
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        track_remove_region (
          arranger_object_get_track (self),
          (ZRegion *) self, F_PUBLISH_EVENTS,
          F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        ZRegion * parent_region =
          ((MidiNote *) self)->region;
        midi_region_remove_midi_note (
          parent_region, (MidiNote *) self,
          F_FREE, F_PUBLISH_EVENTS);
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  EVENTS_PUSH (ET_ARRANGER_OBJECT_CREATED, *r1);
  EVENTS_PUSH (ET_ARRANGER_OBJECT_CREATED, *r2);
}

/**
 * Undoes what arranger_object_split() did.
 */
void
arranger_object_unsplit (
  ArrangerObject *         r1,
  ArrangerObject *         r2,
  ArrangerObject **        obj)
{
  /* create the new object */
  *obj =
    arranger_object_clone (
      r1, ARRANGER_OBJECT_CLONE_COPY_MAIN);

  /* set the end pos to the end pos of r2 */
  arranger_object_end_pos_setter (
    *obj, &r2->end_pos);

  /* add it to the parent */
  switch (r1->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        track_add_region (
          arranger_object_get_track (r1),
          (ZRegion *) *obj, ((ZRegion *) r1)->at,
          ((ZRegion *) r1)->lane_pos,
          F_GEN_NAME, F_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        ZRegion * parent_region =
          ((MidiNote *) r1)->region;
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *obj, 1);
      }
      break;
    default:
      break;
    }

  /* generate widgets so update visibility in the
   * arranger can work */
  /*arranger_object_gen_widget (*obj);*/

  /* select it */
  ArrangerSelections * sel =
    arranger_object_get_selections_for_type (
      (*obj)->type);
  arranger_selections_remove_object (
    sel, r1);
  arranger_selections_remove_object (
    sel, r2);
  arranger_selections_add_object (
    sel, *obj);

  /* change to r1 if the original region was the
   * clip editor region */
  if (CLIP_EDITOR->region == (ZRegion *) r1)
    {
      clip_editor_set_region (
        CLIP_EDITOR, (ZRegion *) *obj);
    }

  /* remove and free the original regions */
  switch (r1->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        track_remove_region (
          arranger_object_get_track (r1),
          (ZRegion *) r1, F_PUBLISH_EVENTS, F_FREE);
        track_remove_region (
          arranger_object_get_track (r2),
          (ZRegion *) r2, F_PUBLISH_EVENTS, F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        midi_region_remove_midi_note (
          ((MidiNote *) r1)->region,
          (MidiNote *) r1, F_PUBLISH_EVENTS, F_FREE);
        midi_region_remove_midi_note (
          ((MidiNote *) r2)->region,
          (MidiNote *) r2, F_PUBLISH_EVENTS, F_FREE);
      }
      break;
    default:
      break;
    }

  EVENTS_PUSH (ET_ARRANGER_OBJECT_CREATED, *obj);
}

/**
 * Sets the name of the object, if the object can
 * have a name.
 */
void
arranger_object_set_name (
  ArrangerObject * self,
  const char *     name,
  int              fire_events)
{
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        arranger_object_set_string (
          Marker, self, name, name);
      }
      break;
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        arranger_object_set_string (
          ZRegion, self, name, name);
      }
      break;
    default:
      break;
    }
  if (fire_events)
    {
      EVENTS_PUSH (
        ET_ARRANGER_OBJECT_CHANGED, self);
    }
}

static void
free_region (
  ZRegion * self)
{
  if (self->name)
    g_free (self->name);
  if (G_IS_OBJECT (self->layout))
    g_object_unref (self->layout);

#define FREE_R(type,sc) \
  case REGION_TYPE_##type: \
    sc##_region_free_members (self); \
  break

  switch (self->type)
    {
      FREE_R (MIDI, midi);
      FREE_R (AUDIO, audio);
      FREE_R (CHORD, chord);
      FREE_R (AUTOMATION, automation);
    }

#undef FREE_R

  free (self);
}

static void
free_midi_note (
  MidiNote * self)
{
  g_return_if_fail (self->vel);
  arranger_object_free (
    (ArrangerObject *) self->vel);

  if (self->region_name)
    g_free (self->region_name);

  if (G_IS_OBJECT (self->layout))
    g_object_unref (self->layout);

  free (self);
}

/**
 * Frees only this object.
 */
void
arranger_object_free (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case TYPE (REGION):
      free_region ((ZRegion *) self);
      return;
    case TYPE (MIDI_NOTE):
      free_midi_note ((MidiNote *) self);
      return;
    case TYPE (MARKER):
      {
        Marker * marker = (Marker *) self;
        g_free (marker->name);
        free (marker);
      }
      return;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * co = (ChordObject *) self;
        if (co->region_name)
          g_free (co->region_name);
        free (co);
      }
      return;
    case TYPE (SCALE_OBJECT):
      {
        ScaleObject * scale = (ScaleObject *) self;
        musical_scale_free (scale->scale);
        free (scale);
      }
      return;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        if (ap->region_name)
          g_free (ap->region_name);
        free (ap);
      }
      return;
    case TYPE (VELOCITY):
      {
        Velocity * vel =
          (Velocity *) self;
        free (vel);
      }
      return;
    default:
      g_return_if_reached ();
    }
  g_return_if_reached ();
}
