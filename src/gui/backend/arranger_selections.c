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

#include <stdlib.h>

#include "audio/automation_region.h"
#include "audio/chord_object.h"
#include "audio/engine.h"
#include "audio/marker.h"
#include "audio/scale_object.h"
#include "audio/transport.h"
#include "gui/backend/arranger_selections.h"
#include "gui/backend/automation_selections.h"
#include "gui/backend/chord_selections.h"
#include "gui/backend/midi_arranger_selections.h"
#include "gui/backend/timeline_selections.h"
#include "gui/widgets/arranger_object.h"
#include "project.h"
#include "utils/arrays.h"

#define TYPE(x) \
  (ARRANGER_SELECTIONS_TYPE_##x)

/**
 * Inits the selections after loading a project.
 */
void
arranger_selections_init_loaded (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

#define SET_OBJ(sel,cc,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    { \
      ArrangerObject * obj = \
        (ArrangerObject *) sel->sc##s[i]; \
      arranger_object_update_frames (obj); \
      sel->sc##s[i] = \
        (cc *) \
        arranger_object_find (obj); \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      SET_OBJ (ts, ZRegion, region);
      SET_OBJ (ts, ScaleObject, scale_object);
      SET_OBJ (ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      for (i = 0; i < mas->num_midi_notes; i++)
        {
          MidiNote * mn = mas->midi_notes[i];
          ArrangerObject * mn_obj =
            (ArrangerObject *) mn;
          arranger_object_update_frames (
            mn_obj);
          mn->region =
            region_find_by_name (mn->region_name);
          g_warn_if_fail (mn->region);
          mas->midi_notes[i] =
            (MidiNote *)
            arranger_object_find (mn_obj);
          g_warn_if_fail (mas->midi_notes[i]);
          arranger_object_free (mn_obj);
        }
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      SET_OBJ (as, AutomationPoint, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      SET_OBJ (cs, ChordObject, chord_object);
      break;
    default:
      g_return_if_reached ();
    }

#undef SET_OBJ
}

/**
 * Initializes the selections.
 */
void
arranger_selections_init (
  ArrangerSelections *   self,
  ArrangerSelectionsType type)
{
  self->type = type;

  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

#define SET_OBJ(sel,cc,sc) \
  sel->sc##s_size = 1; \
  sel->sc##s = \
    calloc (1, sizeof (cc *))

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      SET_OBJ (ts, ZRegion, region);
      SET_OBJ (ts, ScaleObject, scale_object);
      SET_OBJ (ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      SET_OBJ (mas, MidiNote, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      SET_OBJ (
        as, AutomationPoint, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      SET_OBJ (cs, ChordObject, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef SET_OBJ
}

/**
 * Appends the given object to the selections.
 */
void
arranger_selections_add_object (
  ArrangerSelections * self,
  ArrangerObject *     obj)
{
  /* check if object is allowed */
  switch (self->type)
    {
    case TYPE (CHORD):
      g_return_if_fail (
        obj->type ==
          ARRANGER_OBJECT_TYPE_CHORD_OBJECT);
      break;
    case TYPE (TIMELINE):
      g_return_if_fail (
        obj->type == ARRANGER_OBJECT_TYPE_REGION ||
        obj->type == ARRANGER_OBJECT_TYPE_SCALE_OBJECT ||
        obj->type == ARRANGER_OBJECT_TYPE_MARKER);
      break;
    case TYPE (MIDI):
      g_return_if_fail (
        obj->type == ARRANGER_OBJECT_TYPE_MIDI_NOTE ||
        obj->type == ARRANGER_OBJECT_TYPE_VELOCITY);
      break;
    case TYPE (AUTOMATION):
      g_return_if_fail (
        obj->type == ARRANGER_OBJECT_TYPE_AUTOMATION_POINT);
      break;
		default:
			g_return_if_reached();
    }

#define ADD_OBJ(sel,caps,cc,sc) \
  if (obj->type == ARRANGER_OBJECT_TYPE_##caps) \
    { \
      cc * sc = (cc *) obj; \
      if (!array_contains ( \
             sel->sc##s, sel->num_##sc##s, \
             sc)) \
        { \
          array_double_size_if_full ( \
            sel->sc##s, sel->num_##sc##s, \
            sel->sc##s_size, cc *); \
          array_append ( \
            sel->sc##s, \
            sel->num_##sc##s, \
            sc); \
        } \
    }

  /* add the object to the child selections */
  switch (self->type)
    {
    case TYPE (CHORD):
      {
        ChordSelections * sel =
          (ChordSelections *) self;
        ADD_OBJ (
          sel, CHORD_OBJECT,
          ChordObject, chord_object);
        }
      break;
    case TYPE (TIMELINE):
      {
        TimelineSelections * sel =
          (TimelineSelections *) self;
        ADD_OBJ (
          sel, REGION,
          ZRegion, region);
        ADD_OBJ (
          sel, SCALE_OBJECT,
          ScaleObject, scale_object);
        ADD_OBJ (
          sel, MARKER,
          Marker, marker);
        }
      break;
    case TYPE (MIDI):
      {
        MidiArrangerSelections * sel =
          (MidiArrangerSelections *) self;
        if (obj->type ==
              ARRANGER_OBJECT_TYPE_VELOCITY)
          {
            Velocity * vel = (Velocity *) obj;
            obj = (ArrangerObject *) vel->midi_note;
          }
        ADD_OBJ (
          sel, MIDI_NOTE,
          MidiNote, midi_note);
        }
      break;
    case TYPE (AUTOMATION):
      {
        AutomationSelections * sel =
          (AutomationSelections *) self;
        ADD_OBJ (
          sel, AUTOMATION_POINT,
          AutomationPoint, automation_point);
        }
      break;
		default:
			g_return_if_reached ();
    }
#undef ADD_OBJ
}

/**
 * Sets the values of each object in the dest selections
 * to the values in the src selections.
 */
void
arranger_selections_set_from_selections (
  ArrangerSelections * dest,
  ArrangerSelections * src)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  /* TODO */
#define RESET_COUNTERPART(sel,cc,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    break;

  switch (dest->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) dest;
      RESET_COUNTERPART (ts, ZRegion, region);
      RESET_COUNTERPART (
        ts, ScaleObject, scale_object);
      RESET_COUNTERPART (ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) dest;
      RESET_COUNTERPART (mas, MidiNote, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) dest;
      RESET_COUNTERPART (
        as, AutomationPoint, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) dest;
      RESET_COUNTERPART (
        cs, ChordObject, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef RESET_COUNTERPART
}

/**
 * Clone the struct for copying, undoing, etc.
 */
ArrangerSelections *
arranger_selections_clone (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * src_ts, * new_ts;
  ChordSelections * src_cs, * new_cs;
  MidiArrangerSelections * src_mas, * new_mas;
  AutomationSelections * src_as, * new_as;

#define CLONE_OBJS(src_sel,new_sel,cc,sc) \
  cc * sc, * new_##sc; \
    for (i = 0; i < src_sel->num_##sc##s; i++) \
    { \
      sc = src_sel->sc##s[i]; \
      ArrangerObject * sc_obj = \
        (ArrangerObject *) sc; \
      new_##sc = \
        (cc *) \
        arranger_object_clone ( \
          (ArrangerObject *) sc, \
          ARRANGER_OBJECT_CLONE_COPY); \
      ArrangerObject * new_sc_obj = \
        (ArrangerObject *) new_##sc; \
      sc_obj->transient = new_sc_obj; \
      arranger_selections_add_object ( \
        (ArrangerSelections *) new_sel, \
        new_sc_obj); \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      src_ts = (TimelineSelections *) self;
      new_ts =
        calloc (1, sizeof (TimelineSelections));
      new_ts->base = src_ts->base;
      arranger_selections_init (
        (ArrangerSelections *) new_ts,
        ARRANGER_SELECTIONS_TYPE_TIMELINE);
      CLONE_OBJS (
        src_ts, new_ts, ZRegion, region);
      CLONE_OBJS (
        src_ts, new_ts, ScaleObject, scale_object);
      CLONE_OBJS (
        src_ts, new_ts, Marker, marker);
      return ((ArrangerSelections *) new_ts);
    case TYPE (MIDI):
      src_mas = (MidiArrangerSelections *) self;
      new_mas =
        calloc (1, sizeof (MidiArrangerSelections));
      arranger_selections_init (
        (ArrangerSelections *) new_mas,
        ARRANGER_SELECTIONS_TYPE_MIDI);
      new_mas->base = src_mas->base;
      CLONE_OBJS (
        src_mas, new_mas, MidiNote, midi_note);
      return ((ArrangerSelections *) new_mas);
    case TYPE (AUTOMATION):
      src_as = (AutomationSelections *) self;
      new_as =
        calloc (1, sizeof (AutomationSelections));
      arranger_selections_init (
        (ArrangerSelections *) new_as,
        ARRANGER_SELECTIONS_TYPE_AUTOMATION);
      new_as->base = src_as->base;
      CLONE_OBJS (
        src_as, new_as, AutomationPoint,
        automation_point);
      return ((ArrangerSelections *) new_as);
    case TYPE (CHORD):
      src_cs = (ChordSelections *) self;
      new_cs =
        calloc (1, sizeof (ChordSelections));
      arranger_selections_init (
        (ArrangerSelections *) new_cs,
        ARRANGER_SELECTIONS_TYPE_CHORD);
      new_cs->base = src_cs->base;
      CLONE_OBJS (
        src_cs, new_cs, ChordObject, chord_object);
      return ((ArrangerSelections *) new_cs);
		default:
			g_return_val_if_reached (NULL);
    }

  g_return_val_if_reached (NULL);

#undef CLONE_OBJS
}

/**
 * Returns if there are any selections.
 */
int
arranger_selections_has_any (
  ArrangerSelections * self)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      return
        ts->num_regions > 0 ||
        ts->num_scale_objects > 0 ||
        ts->num_markers > 0;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      return
        mas->num_midi_notes > 0;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      return
        as->num_automation_points > 0;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      return
        cs->num_chord_objects > 0;
		default:
			g_return_val_if_reached (-1);
    }

  g_return_val_if_reached (-1);
}

static void
add_ticks_if_global (
  ArrangerSelections * self,
  const int            global,
  Position *           pos)
{
  ArrangerObject * obj =
    arranger_selections_get_first_object (self);
  ArrangerObject * region =
    (ArrangerObject *)
    arranger_object_get_region (obj);
  g_return_if_fail (region);
  position_add_ticks (
    pos, region->pos.total_ticks);

}

/**
 * Returns the position of the leftmost object.
 *
 * @param pos The return value will be stored here.
 * @param global Return global (timeline) Position,
 *   otherwise returns the local (from the start
 *   of the ZRegion) Position.
 */
void
arranger_selections_get_start_pos (
  ArrangerSelections * self,
  Position *           pos,
  int                  global)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  position_set_to_bar (
    pos, TRANSPORT->total_bars);

#define GET_START_POS(sel,cc,sc) \
  for (i = 0; i < (sel)->num_##sc##s; i++) \
    { \
      cc * sc = (sel)->sc##s[i]; \
      ArrangerObject * obj = (ArrangerObject *) sc; \
      g_warn_if_fail (obj); \
      if (position_is_before ( \
            &obj->pos, pos)) \
        { \
          position_set_to_pos ( \
            pos, &obj->pos); \
        } \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      GET_START_POS (
        ts, ZRegion, region);
      GET_START_POS (
        ts, ScaleObject, scale_object);
      GET_START_POS (
        ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      GET_START_POS (
        mas, MidiNote, midi_note);
      add_ticks_if_global (
        self, global, pos);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      GET_START_POS (
        as, AutomationPoint, automation_point);
      add_ticks_if_global (
        self, global, pos);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      GET_START_POS (
        cs, ChordObject, chord_object);
      add_ticks_if_global (
        self, global, pos);
      break;
		default:
			g_return_if_reached ();
    }

#undef GET_START_POS
}

/**
 * Returns the end position of the rightmost object.
 *
 * @param pos The return value will be stored here.
 * @param global Return global (timeline) Position,
 *   otherwise returns the local (from the start
 *   of the ZRegion) Position.
 */
void
arranger_selections_get_end_pos (
  ArrangerSelections * self,
  Position *           pos,
  int                  global)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  position_init (pos);

#define GET_END_POS(sel,cc,sc) \
  for (i = 0; i < (sel)->num_##sc##s; i++) \
    { \
      cc * sc = (sel)->sc##s[i]; \
      ArrangerObject * obj = (ArrangerObject *) sc; \
      g_warn_if_fail (obj); \
      if (obj->has_length) \
        { \
          if (position_is_after ( \
                &obj->end_pos, pos)) \
            { \
              position_set_to_pos ( \
                pos, &obj->end_pos); \
            } \
        } \
      else \
        { \
          if (position_is_after ( \
                &obj->pos, pos)) \
            { \
              position_set_to_pos ( \
                pos, &obj->pos); \
            } \
        } \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      GET_END_POS (
        ts, ZRegion, region);
      GET_END_POS (
        ts, ScaleObject, scale_object);
      GET_END_POS (
        ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      GET_END_POS (
        mas, MidiNote, midi_note);
      add_ticks_if_global (
        self, global, pos);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      GET_END_POS (
        as, AutomationPoint, automation_point);
      add_ticks_if_global (
        self, global, pos);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      GET_END_POS (
        cs, ChordObject, chord_object);
      add_ticks_if_global (
        self, global, pos);
      break;
		default:
			g_return_if_reached ();
    }

#undef GET_END_POS
}

/**
 * Gets first object.
 */
ArrangerObject *
arranger_selections_get_first_object (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  Position pos;
  position_set_to_bar (
    &pos, TRANSPORT->total_bars);
  ArrangerObject * ret_obj = NULL;

#define GET_FIRST_OBJ(sel,cc,sc) \
  for (i = 0; i < (sel)->num_##sc##s; i++) \
    { \
      cc * sc = (sel)->sc##s[i]; \
      ArrangerObject * obj = (ArrangerObject *) sc; \
      g_warn_if_fail (obj); \
      if (position_is_before ( \
            &obj->pos, &pos)) \
        { \
          position_set_to_pos ( \
            &pos, &obj->pos); \
          ret_obj = obj; \
        } \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      GET_FIRST_OBJ (
        ts, ZRegion, region);
      GET_FIRST_OBJ (
        ts, ScaleObject, scale_object);
      GET_FIRST_OBJ (
        ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      GET_FIRST_OBJ (
        mas, MidiNote, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      GET_FIRST_OBJ (
        as, AutomationPoint, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      GET_FIRST_OBJ (
        cs, ChordObject, chord_object);
      break;
		default:
			g_return_val_if_reached (NULL);
    }

  return ret_obj;

#undef GET_FIRST_OBJ
}

/**
 * Gets last object.
 */
ArrangerObject *
arranger_selections_get_last_object (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  POSITION_INIT_ON_STACK (pos);
  ArrangerObject * ret_obj = NULL;

#define GET_LAST_OBJ(sel,cc,sc) \
  for (i = 0; i < (sel)->num_##sc##s; i++) \
    { \
      cc * sc = (sel)->sc##s[i]; \
      ArrangerObject * obj = (ArrangerObject *) sc; \
      g_warn_if_fail (obj); \
      if (obj->has_length) \
        { \
          if (position_is_after ( \
                &obj->end_pos, &pos)) \
            { \
              position_set_to_pos ( \
                &pos, &obj->end_pos); \
              ret_obj = obj; \
            } \
        } \
      else \
        { \
          if (position_is_after ( \
                &obj->pos, &pos)) \
            { \
              position_set_to_pos ( \
                &pos, &obj->pos); \
              ret_obj = obj; \
            } \
        } \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      GET_LAST_OBJ (
        ts, ZRegion, region);
      GET_LAST_OBJ (
        ts, ScaleObject, scale_object);
      GET_LAST_OBJ (
        ts, Marker, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      GET_LAST_OBJ (
        mas, MidiNote, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      GET_LAST_OBJ (
        as, AutomationPoint, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      GET_LAST_OBJ (
        cs, ChordObject, chord_object);
      break;
		default:
			g_return_val_if_reached (NULL);
    }

  return ret_obj;

#undef GET_LAST_OBJ
}

/**
 * Sets the cache Position's for each object in
 * the selection.
 *
 * Used by the ArrangerWidget's.
 */
void
arranger_selections_set_cache_poses (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

#define SET_CACHE_POS(sel,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    { \
      ArrangerObject * sc = \
        (ArrangerObject *) sel->sc##s[i]; \
      sc->cache_pos = sc->pos; \
      if (sc->has_length) \
        { \
          sc->cache_end_pos = sc->end_pos; \
        } \
      if (sc->can_loop) \
        { \
          sc->cache_clip_start_pos =  \
            sc->clip_start_pos; \
          sc->cache_loop_start_pos =  \
            sc->loop_start_pos; \
          sc->cache_loop_end_pos =  \
            sc->loop_end_pos; \
        } \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      SET_CACHE_POS (
        ts, region);
      SET_CACHE_POS (
        ts, scale_object);
      SET_CACHE_POS (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      SET_CACHE_POS (
        mas, midi_note);
      for (i = 0; i < mas->num_midi_notes; i++)
        {
          MidiNote * mn = mas->midi_notes[i];
          midi_note_set_cache_val (
            mn, mn->val);
          velocity_set_cache_vel (
            mn->vel, mn->vel->vel);
        }
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      SET_CACHE_POS (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      SET_CACHE_POS (
        cs, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef SET_CACHE_POS
}

/**
 * Moves the selections by the given
 * amount of ticks.
 *
 * @param use_cached_pos Add the ticks to the cached
 *   Position's instead of the current Position's.
 * @param ticks Ticks to add.
 */
void
arranger_selections_add_ticks (
  ArrangerSelections *     self,
  const long               ticks,
  const int                use_cached_pos)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

#define ADD_TICKS(sel,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    { \
      ArrangerObject * sc = \
        (ArrangerObject *) sel->sc##s[i]; \
      arranger_object_move ( \
        sc, ticks, use_cached_pos); \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      ADD_TICKS (
        ts, region);
      ADD_TICKS (
        ts, scale_object);
      ADD_TICKS (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      ADD_TICKS (
        mas, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      ADD_TICKS (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      ADD_TICKS (
        cs, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef ADD_TICKS
}

/**
 * Clears selections.
 */
void
arranger_selections_clear (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

/* use caches because ts->* will be operated on. */
#define REMOVE_OBJS(sel,sc) \
  { \
    int num_##sc##s = sel->num_##sc##s; \
    ArrangerObject * sc##s[num_##sc##s]; \
    for (i = 0; i < num_##sc##s; i++) \
      { \
        sc##s[i] = \
          (ArrangerObject *) sel->sc##s[i]; \
      } \
    for (i = 0; i < num_##sc##s; i++) \
      { \
        ArrangerObject * sc = sc##s[i]; \
        arranger_selections_remove_object ( \
          self, sc); \
        EVENTS_PUSH ( \
          ET_ARRANGER_SELECTIONS_CHANGED, \
          self); \
      } \
  }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      REMOVE_OBJS (
        ts, region);
      REMOVE_OBJS (
        ts, scale_object);
      REMOVE_OBJS (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      REMOVE_OBJS (
        mas, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      REMOVE_OBJS (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      REMOVE_OBJS (
        cs, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef REMOVE_OBJS
}

/**
 * Returns the number of selected objects.
 */
int
arranger_selections_get_num_objects (
  ArrangerSelections * self)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  int size = 0;

#define ADD_OBJ(sel,sc) \
  for (int i = 0; i < sel->num_##sc##s; i++) \
    { \
      size++; \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      ADD_OBJ (
        ts, region);
      ADD_OBJ (
        ts, scale_object);
      ADD_OBJ (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      ADD_OBJ (
        mas, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      ADD_OBJ (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      ADD_OBJ (
        cs, chord_object);
      break;
		default:
			g_return_val_if_reached (-1);
    }
#undef ADD_OBJ

  return size;
}

/**
 * Code to run after deserializing.
 */
void
arranger_selections_post_deserialize (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

/* use caches because ts->* will be operated on. */
#define POST_DESERIALIZE(sel,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    { \
      arranger_object_post_deserialize ( \
        (ArrangerObject *) \
        sel->sc##s[i]); \
    } \

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      POST_DESERIALIZE (
        ts, region);
      POST_DESERIALIZE (
        ts, scale_object);
      POST_DESERIALIZE (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      POST_DESERIALIZE (
        mas, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      POST_DESERIALIZE (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      POST_DESERIALIZE (
        cs, chord_object);
      break;
		default:
			g_return_if_reached ();
    }

#undef POST_DESERIALIZE
}

/**
 * Frees the selections but not the objects.
 */
void
arranger_selections_free (
  ArrangerSelections * self)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      free (ts->regions);
      free (ts->scale_objects);
      free (ts->markers);
      free (ts);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      free (mas->midi_notes);
      free (mas);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      free (as->automation_points);
      free (as);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      free (cs->chord_objects);
      free (cs);
      break;
		default:
			g_return_if_reached ();
    }
}

/**
 * Frees all the objects as well.
 *
 * To be used in actions where the selections are
 * all clones.
 */
void
arranger_selections_free_full (
  ArrangerSelections * self)
{
  int i;
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

/* use caches because ts->* will be operated on. */
#define FREE_OBJS(sel,sc) \
  for (i = 0; i < sel->num_##sc##s; i++) \
    { \
      ArrangerObject * obj = \
        (ArrangerObject *) sel->sc##s[i]; \
      arranger_object_free (obj); \
    } \
    free (sel->sc##s)

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      FREE_OBJS (
        ts, region);
      FREE_OBJS (
        ts, scale_object);
      FREE_OBJS (
        ts, marker);
      free (ts);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      FREE_OBJS (
        mas, midi_note);
      free (mas);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      FREE_OBJS (
        as, automation_point);
      free (as);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      FREE_OBJS (
        cs, chord_object);
      free (cs);
      break;
    default:
      g_return_if_reached ();
    }

#undef FREE_OBJS
}

/**
 * Returns if the arranger object is in the
 * selections or  not.
 *
 * The object must be the main object (see
 * ArrangerObjectInfo).
 */
int
arranger_selections_contains_object (
  ArrangerSelections * self,
  ArrangerObject *     obj)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  g_return_val_if_fail (self && obj, 0);

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      switch (obj->type)
        {
        case ARRANGER_OBJECT_TYPE_REGION:
          return
            array_contains (
              ts->regions, ts->num_regions, obj);
          break;
        case ARRANGER_OBJECT_TYPE_SCALE_OBJECT:
          return
            array_contains (
              ts->scale_objects,
              ts->num_scale_objects, obj);
          break;
        case ARRANGER_OBJECT_TYPE_MARKER:
          return
            array_contains (
              ts->markers,
              ts->num_markers, obj);
          break;
        default:
          break;
        }
      break;
    case TYPE (MIDI):
      {
        mas = (MidiArrangerSelections *) self;
        switch (obj->type)
          {
          case ARRANGER_OBJECT_TYPE_VELOCITY:
            {
              Velocity * vel =
                (Velocity *) obj;
              return
                array_contains (
                  mas->midi_notes,
                  mas->num_midi_notes,
                  vel->midi_note);
            }
            break;
          case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
            {
              return
                array_contains (
                  mas->midi_notes,
                  mas->num_midi_notes, obj);
            }
            break;
          default:
            g_return_val_if_reached (-1);
          }
      }
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      switch (obj->type)
        {
        case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
          {
            return
              array_contains (
                as->automation_points,
                as->num_automation_points, obj);
          }
          break;
        default:
          g_return_val_if_reached (-1);
        }
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      if (obj->type ==
            ARRANGER_OBJECT_TYPE_CHORD_OBJECT)
        {
          return
            array_contains (
              cs->chord_objects,
              cs->num_chord_objects, obj);
        }
      break;
		default:
			g_return_val_if_reached (0);
    }

  g_return_val_if_reached (0);
}

/**
 * Removes the arranger object from the selections.
 */
void
arranger_selections_remove_object (
  ArrangerSelections * self,
  ArrangerObject *     obj)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

#define REMOVE_OBJ(sel,caps,sc) \
  if (obj->type == ARRANGER_OBJECT_TYPE_##caps && \
      array_contains ( \
        sel->sc##s, sel->num_##sc##s, obj)) \
    { \
      array_delete ( \
        sel->sc##s, sel->num_##sc##s, obj); \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      REMOVE_OBJ (
        ts, REGION, region);
      REMOVE_OBJ (
        ts, SCALE_OBJECT, scale_object);
      REMOVE_OBJ (
        ts, MARKER, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      if (obj->type ==
            ARRANGER_OBJECT_TYPE_VELOCITY)
        {
          Velocity * vel = (Velocity *) obj;
          obj = (ArrangerObject *) vel->midi_note;
        }
      REMOVE_OBJ (
        mas, MIDI_NOTE, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      REMOVE_OBJ (
        as, AUTOMATION_POINT, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      REMOVE_OBJ (
        cs, CHORD_OBJECT, chord_object);
      break;
		default:
			g_return_if_reached ();
    }
#undef REMOVE_OBJ
}

/**
 * Returns all objects in the selections in a
 * newly allocated array that should be free'd.
 *
 * @param size A pointer to save the size into.
 */
ArrangerObject **
arranger_selections_get_all_objects (
  ArrangerSelections * self,
  int *                size)
{
  TimelineSelections * ts;
  ChordSelections * cs;
  MidiArrangerSelections * mas;
  AutomationSelections * as;

  ArrangerObject ** objs =
    calloc (1, sizeof (ArrangerObject *));
  *size = 0;

#define ADD_OBJ(sel,sc) \
  for (int i = 0; i < sel->num_##sc##s; i++) \
    { \
      ArrangerObject ** new_objs = \
        (ArrangerObject **) \
        realloc ( \
          objs, \
          (size_t) (*size + 1) * \
            sizeof (ArrangerObject *)); \
      g_return_val_if_fail (new_objs, NULL); \
      objs = new_objs; \
      objs[*size] = \
        (ArrangerObject *) sel->sc##s[i]; \
      (*size)++; \
    }

  switch (self->type)
    {
    case TYPE (TIMELINE):
      ts = (TimelineSelections *) self;
      ADD_OBJ (
        ts, region);
      ADD_OBJ (
        ts, scale_object);
      ADD_OBJ (
        ts, marker);
      break;
    case TYPE (MIDI):
      mas = (MidiArrangerSelections *) self;
      ADD_OBJ (
        mas, midi_note);
      break;
    case TYPE (AUTOMATION):
      as = (AutomationSelections *) self;
      ADD_OBJ (
        as, automation_point);
      break;
    case TYPE (CHORD):
      cs = (ChordSelections *) self;
      ADD_OBJ (
        cs, chord_object);
      break;
		default:
			g_return_val_if_reached (NULL);
    }
#undef ADD_OBJ

  return objs;
}

ArrangerSelections *
arranger_selections_get_for_type (
  ArrangerSelectionsType type)
{
  switch (type)
    {
    case ARRANGER_SELECTIONS_TYPE_TIMELINE:
      return (ArrangerSelections *) TL_SELECTIONS;
    case ARRANGER_SELECTIONS_TYPE_MIDI:
      return (ArrangerSelections *) MA_SELECTIONS;
    case ARRANGER_SELECTIONS_TYPE_CHORD:
      return (ArrangerSelections *) CHORD_SELECTIONS;
    case ARRANGER_SELECTIONS_TYPE_AUTOMATION:
      return (ArrangerSelections *) AUTOMATION_SELECTIONS;
    default:
      g_return_val_if_reached (NULL);
    }
  g_return_val_if_reached (NULL);
}

/**
 * Redraws each object in the arranger selections.
 */
void
arranger_selections_redraw (
  ArrangerSelections * self)
{
  int size;
  ArrangerObject ** objs =
    arranger_selections_get_all_objects (
      self, &size);

  for (int i = 0; i < size; i++)
    {
      ArrangerObject * obj = objs[i];
      arranger_object_queue_redraw (obj);
    }

  free (objs);
}
