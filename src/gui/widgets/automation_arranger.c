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
 * The automation containing regions and other
 * objects.
 */

#include "actions/undoable_action.h"
#include "actions/create_automation_selections_action.h"
#include "actions/undo_manager.h"
#include "actions/duplicate_automation_selections_action.h"
#include "actions/move_automation_selections_action.h"
#include "audio/automation_region.h"
#include "audio/automation_track.h"
#include "audio/automation_tracklist.h"
#include "audio/audio_track.h"
#include "audio/bus_track.h"
#include "audio/channel.h"
#include "audio/chord_object.h"
#include "audio/chord_track.h"
#include "audio/instrument_track.h"
#include "audio/marker_track.h"
#include "audio/master_track.h"
#include "audio/midi_region.h"
#include "audio/mixer.h"
#include "audio/scale_object.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "audio/transport.h"
#include "gui/backend/automation_selections.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/automation_curve.h"
#include "gui/widgets/automation_track.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/chord_object.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/marker.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_arranger_bg.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/piano_roll.h"
#include "gui/widgets/pinned_tracklist.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/region.h"
#include "gui/widgets/scale_object.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/automation_arranger.h"
#include "gui/widgets/automation_arranger_bg.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/arrays.h"
#include "utils/cairo.h"
#include "utils/flags.h"
#include "utils/objects.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (AutomationArrangerWidget,
               automation_arranger_widget,
               ARRANGER_WIDGET_TYPE)

/**
 * To be called from get_child_position in parent widget.
 *
 * Used to allocate the overlay children.
 */
void
automation_arranger_widget_set_allocation (
  AutomationArrangerWidget * self,
  GtkWidget *          widget,
  GdkRectangle *       allocation)
{
  if (Z_IS_AUTOMATION_POINT_WIDGET (widget))
    {
      AutomationPointWidget * ap_widget =
        Z_AUTOMATION_POINT_WIDGET (widget);
      AutomationPoint * ap =
        ap_widget->automation_point;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      if (!ap->region->at->created ||
          !ap->region->at->track->bot_paned_visible)
        return;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (ap->region->at->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);

      allocation->x =
        ui_pos_to_px_automation_editor (&ap->pos, 1) -
        AP_WIDGET_POINT_SIZE / 2;
      allocation->y =
        (wy + automation_point_get_y_in_px (ap)) -
        AP_WIDGET_POINT_SIZE / 2;
      allocation->width = AP_WIDGET_POINT_SIZE;
      allocation->height = AP_WIDGET_POINT_SIZE;
    }
  else if (Z_IS_AUTOMATION_CURVE_WIDGET (widget))
    {
      AutomationCurveWidget * acw =
        Z_AUTOMATION_CURVE_WIDGET (widget);
      AutomationCurve * ac = acw->ac;
      /*Automatable * a = ap->at->automatable;*/

      gint wx, wy;
      if (!ac->region->at->created ||
          !ac->region->at->track->bot_paned_visible)
        return;
      gtk_widget_translate_coordinates (
        GTK_WIDGET (ac->region->at->widget),
        GTK_WIDGET (self),
        0, 0, &wx, &wy);
      AutomationPoint * prev_ap =
        automation_region_get_ap_before_curve (
          ac->region, ac);
      AutomationPoint * next_ap =
        automation_region_get_ap_after_curve (
          ac->region, ac);
      if (!prev_ap || !next_ap)
        g_return_if_reached ();

      allocation->x =
        ui_pos_to_px_automation_editor (
          &prev_ap->pos,
          1) - AC_Y_HALF_PADDING;
      int prev_y =
        automation_point_get_y_in_px (prev_ap);
      int next_y =
        automation_point_get_y_in_px (next_ap);
      allocation->y =
        (wy + (prev_y > next_y ? next_y : prev_y) -
         AC_Y_HALF_PADDING);
      allocation->width =
        (ui_pos_to_px_automation_editor (
          &next_ap->pos,
          1) - allocation->x) + AC_Y_HALF_PADDING;
      allocation->height =
        (prev_y > next_y ?
         prev_y - next_y :
         next_y - prev_y) + AC_Y_PADDING;
    }
}

/**
 * Returns the appropriate cursor based on the
 * current hover_x and y.
 */
ArrangerCursor
automation_arranger_widget_get_cursor (
  AutomationArrangerWidget * self,
  UiOverlayAction action,
  Tool            tool)
{
  ArrangerCursor ac = ARRANGER_CURSOR_SELECT;

  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPointWidget * apw =
    automation_arranger_widget_get_hit_ap (
      self,
      ar_prv->hover_x,
      ar_prv->hover_y);
  AutomationCurveWidget * acw =
    automation_arranger_widget_get_hit_ac (
      self,
      ar_prv->hover_x,
      ar_prv->hover_y);

  int is_hit = apw || acw;

  switch (action)
    {
    case UI_OVERLAY_ACTION_NONE:
      switch (P_TOOL)
        {
        case TOOL_SELECT_NORMAL:
        {
          if (is_hit)
            {
              return ARRANGER_CURSOR_GRAB;
            }
          else
            {
              /* set cursor to normal */
              return ARRANGER_CURSOR_SELECT;
            }
        }
          break;
        case TOOL_SELECT_STRETCH:
          break;
        case TOOL_EDIT:
          ac = ARRANGER_CURSOR_EDIT;
          break;
        case TOOL_CUT:
          ac = ARRANGER_CURSOR_CUT;
          break;
        case TOOL_ERASER:
          ac = ARRANGER_CURSOR_ERASER;
          break;
        case TOOL_RAMP:
          ac = ARRANGER_CURSOR_RAMP;
          break;
        case TOOL_AUDITION:
          ac = ARRANGER_CURSOR_AUDITION;
          break;
        }
      break;
    case UI_OVERLAY_ACTION_STARTING_DELETE_SELECTION:
    case UI_OVERLAY_ACTION_DELETE_SELECTING:
    case UI_OVERLAY_ACTION_ERASING:
      ac = ARRANGER_CURSOR_ERASER;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING_COPY:
    case UI_OVERLAY_ACTION_MOVING_COPY:
      ac = ARRANGER_CURSOR_GRABBING_COPY;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING:
    case UI_OVERLAY_ACTION_MOVING:
      ac = ARRANGER_CURSOR_GRABBING;
      break;
    case UI_OVERLAY_ACTION_STARTING_MOVING_LINK:
    case UI_OVERLAY_ACTION_MOVING_LINK:
      ac = ARRANGER_CURSOR_GRABBING_LINK;
      break;
    case UI_OVERLAY_ACTION_RESIZING_L:
      ac = ARRANGER_CURSOR_RESIZING_L;
      break;
    case UI_OVERLAY_ACTION_RESIZING_R:
      ac = ARRANGER_CURSOR_RESIZING_R;
      break;
    default:
      ac = ARRANGER_CURSOR_SELECT;
      break;
    }

  return ac;
}

#define GET_HIT_WIDGET(caps,cc,sc) \
cc##Widget * \
automation_arranger_widget_get_hit_##sc ( \
  AutomationArrangerWidget *  self, \
  double                    x, \
  double                    y) \
{ \
  GtkWidget * widget = \
    ui_get_hit_child ( \
      GTK_CONTAINER (self), x, y, \
      caps##_WIDGET_TYPE); \
  if (widget) \
    { \
      return Z_##caps##_WIDGET (widget); \
    } \
  return NULL; \
}

GET_HIT_WIDGET (
  AUTOMATION_POINT, AutomationPoint, ap);
GET_HIT_WIDGET (
  AUTOMATION_CURVE, AutomationCurve, ac);

#undef GET_HIT_WIDGET

void
automation_arranger_widget_select_all (
  AutomationArrangerWidget *  self,
  int                       select)
{
  int i;

  automation_selections_clear (AUTOMATION_SELECTIONS);

  Region * region = CLIP_EDITOR->region;

  /* select everything else */
  AutomationPoint * ap;
  for (i = 0; i < region->num_aps; i++)
    {
      ap = region->aps[i];
      automation_point_widget_select (
        ap->widget, select);
    }

  /**
   * Deselect range if deselecting all.
   */
  if (!select)
    {
      project_set_has_range (0);
    }
}

/**
 * Shows context menu.
 *
 * To be called from parent on right click.
 */
void
automation_arranger_widget_show_context_menu (
  AutomationArrangerWidget * self,
  gdouble              x,
  gdouble              y)
{
  GtkWidget *menu, *menuitem;

  /*RegionWidget * clicked_region =*/
    /*automation_arranger_widget_get_hit_region (*/
      /*self, x, y);*/
  /*AutomationPointWidget * clicked_ap =*/
    /*automation_arranger_widget_get_hit_ap (*/
      /*self, x, y);*/
  /*AutomationCurveWidget * ac =*/
    /*automation_arranger_widget_get_hit_curve (*/
      /*self, x, y);*/

  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label("Do something");

  /*g_signal_connect(menuitem, "activate",*/
                   /*(GCallback) view_popup_menu_onDoSomething, treeview);*/

  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

  gtk_widget_show_all(menu);

  gtk_menu_popup_at_pointer (GTK_MENU(menu), NULL);
}

/**
 * Sets the visibility of the transient and non-
 * transient objects, lane and non-lane.
 *
 * E.g. when moving regions, it hides the original
 * ones.
 */
void
automation_arranger_widget_update_visibility (
  AutomationArrangerWidget * self)
{
  ARRANGER_SET_OBJ_VISIBILITY_ARRAY (
    AUTOMATION_SELECTIONS->automation_points,
    AUTOMATION_SELECTIONS->num_automation_points,
    AutomationPoint, automation_point);
}

void
automation_arranger_widget_on_drag_begin_ap_hit (
  AutomationArrangerWidget * self,
  double                   start_x,
  AutomationPointWidget *  ap_widget)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPoint * ap =
    ap_widget->automation_point;
  ar_prv->start_pos_px = start_x;
  self->start_ap = ap;
  if (!automation_selections_contains_ap (
        AUTOMATION_SELECTIONS, ap))
    {
      AUTOMATION_SELECTIONS->automation_points[0] =
        ap;
      AUTOMATION_SELECTIONS->num_automation_points =
        1;
    }

  self->start_ap = ap;

  /* update arranger action */
  ar_prv->action =
    UI_OVERLAY_ACTION_STARTING_MOVING;
  ui_set_cursor_from_name (GTK_WIDGET (ap_widget),
                 "grabbing");

  /* update selection */
  if (ar_prv->ctrl_held)
    {
      ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
        self, ap, F_SELECT, F_APPEND);
    }
  else
    {
      automation_arranger_widget_select_all (self, 0);
      ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
        self, ap, F_SELECT, F_NO_APPEND);
    }
}

/**
 * Create an AutomationPointat the given Position
 * in the given Track's AutomationTrack.
 *
 * @param pos The pre-snapped position.
 */
void
automation_arranger_widget_create_ap (
  AutomationArrangerWidget * self,
  const Position *   pos,
  const double       start_y)
{
  AutomationTrack * at = CLIP_EDITOR->region->at;
  g_warn_if_fail (at);
  ARRANGER_WIDGET_GET_PRIVATE (self);

  float value =
    automation_track_widget_get_fvalue_at_y (
      at->widget, start_y);

  ar_prv->action =
    UI_OVERLAY_ACTION_CREATING_MOVING;

  /* create a new ap */
  AutomationPoint * ap =
    automation_point_new_float (
      value, pos, F_MAIN);
  self->start_ap = ap;

  /* add it to automation track */
  automation_region_add_ap (
    CLIP_EDITOR->region, ap, F_GEN_CURVE_POINTS);

  /* set visibility */
  arranger_object_info_set_widget_visibility_and_state (
    &ap->obj_info, 1);

  /* set position to all counterparts */
  automation_point_set_pos (
    ap, pos, AO_UPDATE_ALL);

  EVENTS_PUSH (
    ET_AUTOMATION_POINT_CREATED, ap);
  ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
    self, ap, F_SELECT,
    F_NO_APPEND);
}

/**
 * First determines the selection type (objects/
 * range), then either finds and selects items or
 * selects a range.
 *
 * @param[in] delete If this is a select-delete
 *   operation
 */
void
automation_arranger_widget_select (
  AutomationArrangerWidget * self,
  double                   offset_x,
  double                   offset_y,
  int                      delete)
{
  int i;

  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (!delete)
    /* deselect all */
    arranger_widget_select_all (
      Z_ARRANGER_WIDGET (self), 0);

#define FIND_ENCLOSED_WIDGETS_OF_TYPE( \
  caps,cc,sc) \
  cc * sc; \
  cc##Widget * sc##_widget; \
  GtkWidget *  sc##_widgets[800]; \
  int          num_##sc##_widgets = 0; \
  arranger_widget_get_hit_widgets_in_range ( \
    Z_ARRANGER_WIDGET (self), \
    caps##_WIDGET_TYPE, \
    ar_prv->start_x, \
    ar_prv->start_y, \
    offset_x, \
    offset_y, \
    sc##_widgets, \
    &num_##sc##_widgets)

  /* find enclosed automation_points */
  FIND_ENCLOSED_WIDGETS_OF_TYPE (
    AUTOMATION_POINT, AutomationPoint,
    automation_point);
  for (i = 0; i < num_automation_point_widgets; i++)
    {
      automation_point_widget =
        Z_AUTOMATION_POINT_WIDGET (
          automation_point_widgets[i]);

      automation_point =
        automation_point_get_main_automation_point (
          automation_point_widget->automation_point);

      if (delete)
        automation_region_remove_ap (
          automation_point->region,
          automation_point, F_FREE);
      else
        ARRANGER_WIDGET_SELECT_AUTOMATION_POINT (
          self, automation_point, F_SELECT, F_APPEND);
    }

#undef FIND_ENCLOSED_WIDGETS_OF_TYPE
}

/**
 * Moves the AutomationSelections by the given
 * amount of ticks.
 *
 * @param ticks_diff Ticks to move by.
 * @param copy_moving 1 if copy-moving.
 */
void
automation_arranger_widget_move_items_x (
  AutomationArrangerWidget * self,
  long                     ticks_diff,
  int                      copy_moving)
{
  automation_selections_add_ticks (
    AUTOMATION_SELECTIONS, ticks_diff, F_USE_CACHED,
    copy_moving ?
      AO_UPDATE_TRANS :
      AO_UPDATE_ALL);

  /* for arranger refresh */
  EVENTS_PUSH (ET_AUTOMATION_OBJECTS_IN_TRANSIT,
               NULL);
}

/**
 * Sets width to ruler width and height to
 * tracklist height.
 */
void
automation_arranger_widget_set_size (
  AutomationArrangerWidget * self)
{
  // set the size
  /*int ww, hh;*/
  /*if (self->is_pinned)*/
    /*gtk_widget_get_size_request (*/
      /*GTK_WIDGET (MW_PINNED_TRACKLIST),*/
      /*&ww,*/
      /*&hh);*/
  /*else*/
    /*gtk_widget_get_size_request (*/
      /*GTK_WIDGET (MW_TRACKLIST),*/
      /*&ww,*/
      /*&hh);*/
  /*RULER_WIDGET_GET_PRIVATE (MW_RULER);*/
  /*gtk_widget_set_size_request (*/
    /*GTK_WIDGET (self),*/
    /*rw_prv->total_px,*/
    /*hh);*/
}

/**
 * To be called once at init time.
 */
void
automation_arranger_widget_setup (
  AutomationArrangerWidget * self)
{
  automation_arranger_widget_set_size (
    self);
}

void
automation_arranger_widget_move_items_y (
  AutomationArrangerWidget * self,
  double                   offset_y)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (AUTOMATION_SELECTIONS->num_automation_points)
    {
      AutomationPoint * ap;
      for (int i = 0;
           i < AUTOMATION_SELECTIONS->
             num_automation_points; i++)
        {
          ap =
            AUTOMATION_SELECTIONS->
              automation_points[i];

          ap =
            automation_point_get_main_automation_point (ap);

          float fval =
            automation_track_widget_get_fvalue_at_y (
              ap->region->at->widget,
              ar_prv->start_y + offset_y);
          automation_point_update_fvalue (
            ap, fval, AO_UPDATE_TRANS);
        }
      automation_point_widget_update_tooltip (
        self->start_ap->widget, 1);
    }
}

/**
 * Returns the ticks objects were moved by since
 * the start of the drag.
 *
 * FIXME not really needed, can use
 * automation_selections_get_start_pos and the
 * arranger's earliest_obj_start_pos.
 */
static long
get_moved_diff (
  AutomationArrangerWidget * self)
{
#define GET_DIFF(sc,pos_name) \
  if (AUTOMATION_SELECTIONS->num_##sc##s) \
    { \
      return \
        position_to_ticks ( \
          &sc##_get_main_trans_##sc ( \
            AUTOMATION_SELECTIONS->sc##s[0])->pos_name) - \
        position_to_ticks ( \
          &sc##_get_main_##sc ( \
            AUTOMATION_SELECTIONS->sc##s[0])->pos_name); \
    }

  GET_DIFF (automation_point, pos);

  g_return_val_if_reached (0);
}

/**
 * Sets the default cursor in all selected regions and
 * intializes start positions.
 */
void
automation_arranger_widget_on_drag_end (
  AutomationArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  AutomationPoint * ap;
  for (int i = 0;
       i < AUTOMATION_SELECTIONS->
             num_automation_points; i++)
    {
      ap =
        AUTOMATION_SELECTIONS->automation_points[i];
      automation_point_widget_update_tooltip (
        ap->widget, 0);
    }

  if (ar_prv->action ==
        UI_OVERLAY_ACTION_RESIZING_L)
    {
    }
  else if (ar_prv->action ==
        UI_OVERLAY_ACTION_RESIZING_R)
    {
    }
  else if (ar_prv->action ==
        UI_OVERLAY_ACTION_STARTING_MOVING)
    {
      /* if something was clicked with ctrl without
       * moving*/
      if (ar_prv->ctrl_held)
        {
          /*if (self->start_region &&*/
              /*self->start_region_was_selected)*/
            /*{*/
              /*[> deselect it <]*/
              /*[>ARRANGER_WIDGET_SELECT_REGION (<]*/
                /*[>self, self->start_region,<]*/
                /*[>F_NO_SELECT, F_APPEND);<]*/
            /*}*/
        }
      else if (ar_prv->n_press == 2)
        {
          /* double click on object */
          /*g_message ("DOUBLE CLICK");*/
        }
    }
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING)
    {
      /*Position earliest_trans_pos;*/
      /*automation_selections_get_start_pos (*/
        /*AUTOMATION_SELECTIONS,*/
        /*&earliest_trans_pos, 1);*/
      /*UndoableAction * ua =*/
        /*(UndoableAction *)*/
        /*move_automation_selections_action_new (*/
          /*TL_SELECTIONS,*/
          /*position_to_ticks (*/
            /*&earliest_trans_pos) -*/
          /*position_to_ticks (*/
            /*&ar_prv->earliest_obj_start_pos),*/
          /*automation_selections_get_highest_track (*/
            /*TL_SELECTIONS, F_TRANSIENTS) -*/
          /*automation_selections_get_highest_track (*/
            /*TL_SELECTIONS, F_NO_TRANSIENTS));*/
      /*undo_manager_perform (*/
        /*UNDO_MANAGER, ua);*/
    }
  /* if copy/link-moved */
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_COPY ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_MOVING_LINK)
    {
      /*Position earliest_trans_pos;*/
      /*automation_selections_get_start_pos (*/
        /*TL_SELECTIONS,*/
        /*&earliest_trans_pos, 1);*/
      /*UndoableAction * ua =*/
        /*(UndoableAction *)*/
        /*duplicate_automation_selections_action_new (*/
          /*TL_SELECTIONS,*/
          /*position_to_ticks (*/
            /*&earliest_trans_pos) -*/
          /*position_to_ticks (*/
            /*&ar_prv->earliest_obj_start_pos),*/
          /*automation_selections_get_highest_track (*/
            /*TL_SELECTIONS, F_TRANSIENTS) -*/
          /*automation_selections_get_highest_track (*/
            /*TL_SELECTIONS, F_NO_TRANSIENTS));*/
      /*automation_selections_reset_transient_poses (*/
        /*TL_SELECTIONS);*/
      /*automation_selections_clear (*/
        /*TL_SELECTIONS);*/
      /*undo_manager_perform (*/
        /*UNDO_MANAGER, ua);*/
    }
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_NONE ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_STARTING_SELECTION)
    {
      automation_selections_clear (
        AUTOMATION_SELECTIONS);
    }
  /* if something was created */
  else if (ar_prv->action ==
             UI_OVERLAY_ACTION_CREATING_MOVING ||
           ar_prv->action ==
             UI_OVERLAY_ACTION_CREATING_RESIZING_R)
    {
      /*automation_selections_set_to_transient_poses (*/
        /*TL_SELECTIONS);*/
      /*automation_selections_set_to_transient_values (*/
        /*TL_SELECTIONS);*/

      /*UndoableAction * ua =*/
        /*(UndoableAction *)*/
        /*create_automation_selections_action_new (*/
          /*TL_SELECTIONS);*/
      /*undo_manager_perform (*/
        /*UNDO_MANAGER, ua);*/
    }
  /* if didn't click on something */
  else
    {
    }
  ar_prv->action = UI_OVERLAY_ACTION_NONE;
  automation_arranger_widget_update_visibility (
    self);

  self->start_ap = NULL;

  EVENTS_PUSH (ET_AUTOMATION_SELECTIONS_CHANGED,
               NULL);
}

static void
add_children_from_automation_track (
  AutomationArrangerWidget * self,
  AutomationTrack *          at)
{
  if (!at->track->bot_paned_visible)
    return;

  int i,j,k;
  Region * region;
  AutomationPoint * ap;
  AutomationCurve * ac;
  for (i = 0; i < at->num_regions; i++)
    {
      region = at->regions[i];

      for (j = 0; j < region->num_aps; j++)
        {
          ap = region->aps[j];

          for (k = 0; k < 2; k++)
            {
              if (k == 0)
                ap = automation_point_get_main_automation_point (ap);
              else if (k == 1)
                ap = automation_point_get_main_trans_automation_point (ap);

              if (!GTK_IS_WIDGET (ap->widget))
                ap->widget =
                  automation_point_widget_new (ap);

              gtk_overlay_add_overlay (
                GTK_OVERLAY (self),
                GTK_WIDGET (ap->widget));
            }
        }
      for (j = 0; j < region->num_acs; j++)
        {
          ac = region->acs[j];

          if (!GTK_IS_WIDGET (ac->widget))
            ac->widget =
              automation_curve_widget_new (ac);

          gtk_overlay_add_overlay (
            GTK_OVERLAY (self),
            GTK_WIDGET (ac->widget));
        }
    }
}

/**
 * Refreshes visibility of children.
 */
void
automation_arranger_widget_refresh_visibility (
  AutomationArrangerWidget * self)
{
  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  /*GtkWidget * w;*/
  /*RegionWidget * rw;*/
  /*Region * region;*/
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      /*w = GTK_WIDGET (iter->data);*/

      /*if (Z_IS_REGION_WIDGET (w))*/
        /*{*/
          /*rw = Z_REGION_WIDGET (w);*/
          /*REGION_WIDGET_GET_PRIVATE (rw);*/
          /*region = rw_prv->region;*/

          /*arranger_object_info_set_widget_visibility_and_state (*/
            /*&region->obj_info, 1);*/
        /*}*/
    }
  g_list_free (children);
}

/**
 * Readd children.
 */
void
automation_arranger_widget_refresh_children (
  AutomationArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  /* remove all children except bg && playhead */
  GList *children, *iter;

  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      GtkWidget * widget = GTK_WIDGET (iter->data);
      if (widget != (GtkWidget *) ar_prv->bg &&
          widget != (GtkWidget *) ar_prv->playhead)
        {
          /*g_object_ref (widget);*/
          gtk_container_remove (
            GTK_CONTAINER (self),
            widget);
        }
    }
  g_list_free (children);

  /* add tracks */
  AutomationTrack * at = CLIP_EDITOR->region->at;
  add_children_from_automation_track (
    self, at);

  automation_arranger_widget_refresh_visibility (
    self);

  gtk_overlay_reorder_overlay (
    GTK_OVERLAY (self),
    (GtkWidget *) ar_prv->playhead, -1);
}

/**
 * Scroll to the given position.
 */
void
automation_arranger_widget_scroll_to (
  AutomationArrangerWidget * self,
  Position *               pos)
{
  /* TODO */

}

static gboolean
on_focus (GtkWidget       *widget,
          gpointer         user_data)
{
  /*g_message ("automation focused");*/
  MAIN_WINDOW->last_focused = widget;

  return FALSE;
}

static void
automation_arranger_widget_class_init (
  AutomationArrangerWidgetClass * klass)
{
}

static void
automation_arranger_widget_init (
  AutomationArrangerWidget *self )
{
  g_signal_connect (
    self, "grab-focus",
    G_CALLBACK (on_focus), self);
}