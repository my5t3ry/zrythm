/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/track.h"
#include "gui/backend/arranger_object_info.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/timeline_arranger.h"

#include <gtk/gtk.h>

/**
 * Returns whether the object is transient or not.
 *
 * Transient objects are objects that are used
 * during moving operations.
 */
int
arranger_object_info_is_transient (
  ArrangerObjectInfo * self)
{
  return (
    self->counterpart ==
      AOI_COUNTERPART_MAIN_TRANSIENT ||
    self->counterpart ==
      AOI_COUNTERPART_LANE_TRANSIENT);
}

/**
 * Returns whether the object is a lane object or not
 * (only applies to TimelineArrangerWidget objects.
 */
int
arranger_object_info_is_lane (
  ArrangerObjectInfo * self)
{
  return (
    self->counterpart ==
      AOI_COUNTERPART_LANE ||
    self->counterpart ==
      AOI_COUNTERPART_LANE_TRANSIENT);
}

/**
 * Returns if the object represented by the
 * ArrrangerObjectInfo should be visible.
 *
 * This is counterpart-specific, ie. it checks
 * if the transient should be visible or lane
 * counterpart should be visible, etc., based on
 * what is given.
 */
int
arranger_object_info_should_be_visible (
  ArrangerObjectInfo * self)
{
  ArrangerWidget * arranger = NULL;
  switch (self->type)
    {
      /* FIXME fixed for now */
    case AOI_TYPE_REGION:
      arranger =
        Z_ARRANGER_WIDGET (MW_TIMELINE);
      break;
    case AOI_TYPE_CHORD_OBJECT:
      arranger =
        Z_ARRANGER_WIDGET (MW_PINNED_TIMELINE);
      break;
    case AOI_TYPE_SCALE_OBJECT:
      arranger =
        Z_ARRANGER_WIDGET (MW_PINNED_TIMELINE);
      break;
    case AOI_TYPE_MARKER:
      arranger =
        Z_ARRANGER_WIDGET (MW_PINNED_TIMELINE);
      break;
    case AOI_TYPE_AUTOMATION_POINT:
      arranger =
        Z_ARRANGER_WIDGET (MW_TIMELINE);
      break;
    case AOI_TYPE_MIDI_NOTE:
      arranger =
        Z_ARRANGER_WIDGET (MIDI_ARRANGER);
      break;
    }

  ARRANGER_WIDGET_GET_PRIVATE (arranger);

  int trans_visible = 0;
  int non_trans_visible = 0;
  int lane_visible;

  /* check trans/non-trans visiblity */
  if (ar_prv->action ==
        UI_OVERLAY_ACTION_MOVING ||
      ar_prv->action ==
        UI_OVERLAY_ACTION_CREATING_MOVING)
    {
      trans_visible = 1;
      non_trans_visible = 0;
    }
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_COPY ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_LINK)
    {
      trans_visible = 1;
      non_trans_visible = 1;
    }
  else if (ar_prv->action ==
            UI_OVERLAY_ACTION_RESIZING_L ||
          ar_prv->action ==
            UI_OVERLAY_ACTION_RESIZING_R ||
          ar_prv->action ==
            UI_OVERLAY_ACTION_CREATING_RESIZING_R)
    {
      trans_visible = 1;
      non_trans_visible = 0;
    }
  else
    {
      trans_visible = 0;
      non_trans_visible = 1;
    }

  /* check visibility at all */
  if (self->type == AOI_TYPE_AUTOMATION_POINT)
    {
      AutomationPoint * ap =
        (AutomationPoint *)
        arranger_object_info_get_object (self);
      if (!(automation_point_get_track (ap)->
              bot_paned_visible))
        {
          non_trans_visible = 0;
          trans_visible = 0;
        }
    }

  /* check lane visibility */
  if (self->type == AOI_TYPE_REGION)
    {
      Region * region =
        (Region * )
        arranger_object_info_get_object (self);
      lane_visible =
        region_get_track (region)->lanes_visible;
    }
  else
    {
      lane_visible = 0;
    }


  /* return visibility */
  switch (self->counterpart)
    {
    case AOI_COUNTERPART_MAIN:
      return non_trans_visible;
    case AOI_COUNTERPART_MAIN_TRANSIENT:
      return trans_visible;
    case AOI_COUNTERPART_LANE:
      return lane_visible && non_trans_visible;
    case AOI_COUNTERPART_LANE_TRANSIENT:
      return lane_visible && trans_visible;
    }

  g_return_val_if_reached (-1);
}

/**
 * Sets the widget visibility to this counterparty
 * only, or to all counterparties if all is 1.
 */
void
arranger_object_info_set_widget_visibility (
  ArrangerObjectInfo * self,
  int                  all)
{
  Region * r = NULL;
  ChordObject * c = NULL;
  ScaleObject * s = NULL;
  Marker * m = NULL;
  MidiNote * mn = NULL;
  AutomationPoint * ap = NULL;

  /* get the objects */
  switch (self->type)
    {
    case AOI_TYPE_REGION:
      r =
        (Region *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_CHORD_OBJECT:
      c =
        (ChordObject *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_SCALE_OBJECT:
      s =
        (ScaleObject *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_MARKER:
      m =
        (Marker *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_MIDI_NOTE:
      mn =
        (MidiNote *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_AUTOMATION_POINT:
      ap =
        (AutomationPoint *)
        arranger_object_info_get_object (self);
      break;
    }

/**
 * Sets a counterpart of the given object visible
 * or not.
 *
 * param cc CamelCase.
 * param lc snake_case.
 */
#define SET_COUNTERPART_VISIBLE(cc,lc,counterpart) \
  lc = \
    (cc *) self->counterpart; \
  /*g_message ("setting " #counterpart " visible %d", \
    arranger_object_info_should_be_visible ( \
      &lc->obj_info));*/ \
  gtk_widget_set_visible ( \
    GTK_WIDGET (lc->widget), \
    arranger_object_info_should_be_visible ( \
      &lc->obj_info))

/** Sets this object counterpart visible or not. */
#define SET_THIS_VISIBLE(lc) \
  gtk_widget_set_visible ( \
    GTK_WIDGET (lc->widget), \
    arranger_object_info_should_be_visible ( \
      self));

/** Sets all object counterparts visible or not. */
#define SET_ALL_VISIBLE_WITH_LANE(cc,lc) \
  SET_COUNTERPART_VISIBLE (cc, lc, main); \
  SET_COUNTERPART_VISIBLE (cc, lc, main_trans); \
  SET_COUNTERPART_VISIBLE (cc, lc, lane); \
  SET_COUNTERPART_VISIBLE (cc, lc, lane_trans);

/** Sets all object counterparts visible or not. */
#define SET_ALL_VISIBLE_WITHOUT_LANE(cc,lc) \
  SET_COUNTERPART_VISIBLE (cc, lc, main); \
  SET_COUNTERPART_VISIBLE (cc, lc, main_trans); \

  if (all)
    {
      if (r)
        {
          SET_ALL_VISIBLE_WITH_LANE (Region, r);
        }
      if (c)
        {
          SET_ALL_VISIBLE_WITHOUT_LANE (
            ChordObject, c);
        }
      if (s)
        {
          SET_ALL_VISIBLE_WITHOUT_LANE (
            ScaleObject, s);
        }
      if (m)
        {
          SET_ALL_VISIBLE_WITHOUT_LANE (
            Marker, m);
        }
      if (mn)
        {
          SET_ALL_VISIBLE_WITHOUT_LANE (
            MidiNote, mn);
        }
      if (ap)
        {
          SET_ALL_VISIBLE_WITHOUT_LANE (
            AutomationPoint, ap);
        }
    }
  else
    {
      if (r)
        {
          SET_THIS_VISIBLE (r);
        }
      if (c)
        {
          SET_THIS_VISIBLE (c);
        }
      if (s)
        {
          SET_THIS_VISIBLE (s);
        }
      if (m)
        {
          SET_THIS_VISIBLE (m);
        }
      if (mn)
        {
          SET_THIS_VISIBLE (mn);
        }
      if (ap)
        {
          SET_THIS_VISIBLE (ap);
        }
    }

#undef SET_ALL_VISIBLE
#undef SET_THIS_VISIBLE
#undef SET_COUNTERPART_VISIBLE
}

/**
 * Returns the first visible counterpart found.
 */
void *
arranger_object_info_get_visible_counterpart (
  ArrangerObjectInfo * self)
{
  Region * region = NULL;
  ChordObject * chord_object = NULL;
  ScaleObject * scale_object = NULL;
  Marker * marker = NULL;
  MidiNote * midi_note = NULL;
  AutomationPoint * automation_point = NULL;

  /* get the objects */
  switch (self->type)
    {
    case AOI_TYPE_REGION:
      region =
        (Region *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_CHORD_OBJECT:
      chord_object =
        (ChordObject *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_SCALE_OBJECT:
      scale_object =
        (ScaleObject *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_MARKER:
      marker =
        (Marker *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_MIDI_NOTE:
      midi_note =
        (MidiNote *)
        arranger_object_info_get_object (self);
      break;
    case AOI_TYPE_AUTOMATION_POINT:
      automation_point =
        (AutomationPoint *)
        arranger_object_info_get_object (self);
      break;
    }

#define RETURN_COUNTERPART_IF_VISIBLE(lc,counterpart) \
  lc = lc##_get_##counterpart (lc); \
  if (arranger_object_info_should_be_visible ( \
        &lc->obj_info)) \
    return (void *) lc


  if (region)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        region, main_region);
      RETURN_COUNTERPART_IF_VISIBLE (
        region, main_trans_region);
      RETURN_COUNTERPART_IF_VISIBLE (
        region, lane_region);
      RETURN_COUNTERPART_IF_VISIBLE (
        region, lane_trans_region);
    }
  if (chord_object)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        chord_object, main_chord_object);
      RETURN_COUNTERPART_IF_VISIBLE (
        chord_object, main_trans_chord_object);
    }
  if (scale_object)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        scale_object, main_scale_object);
      RETURN_COUNTERPART_IF_VISIBLE (
        scale_object, main_trans_scale_object);
    }
  if (marker)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        marker, main_marker);
      RETURN_COUNTERPART_IF_VISIBLE (
        marker, main_trans_marker);
    }
  if (midi_note)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        midi_note, main_midi_note);
      RETURN_COUNTERPART_IF_VISIBLE (
        midi_note, main_trans_midi_note);
    }
  if (automation_point)
    {
      RETURN_COUNTERPART_IF_VISIBLE (
        automation_point, main_automation_point);
      RETURN_COUNTERPART_IF_VISIBLE (
        automation_point,
        main_trans_automation_point);
    }

#undef RETURN_COUNTERPART_IF_VISIBLE

  g_return_val_if_reached (NULL);
}
