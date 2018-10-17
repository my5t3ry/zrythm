/*
 * gui/widgets/timeline_bg.c - Timeline background
 *
 * Copyright (C) 2018 Alexandros Theodotou
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

#include "zrythm_app.h"
#include "project.h"
#include "settings_manager.h"
#include "audio/transport.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/tracks.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (TimelineBgWidget, timeline_bg_widget, GTK_TYPE_DRAWING_AREA)

#define MW_RULER MAIN_WINDOW->ruler

static void
draw_borders (GtkWidget * widget,
              cairo_t * cr,
              int y_offset)
{
  guint height = gtk_widget_get_allocated_height (widget);

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_set_line_width (cr, 0.5);
  cairo_move_to (cr, 0, y_offset);
  cairo_line_to (cr, MW_RULER->total_px, y_offset);
  cairo_stroke (cr);
}

static gboolean
draw_cb (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  guint width, height;
  GdkRGBA color;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  /* get positions in px */
  static int playhead_pos_in_px;
  playhead_pos_in_px =
    (TRANSPORT->playhead_pos.bars - 1) * MW_RULER->px_per_bar +
    (TRANSPORT->playhead_pos.beats - 1) * MW_RULER->px_per_beat +
    (TRANSPORT->playhead_pos.quarter_beats - 1) * MW_RULER->px_per_quarter_beat +
    TRANSPORT->playhead_pos.ticks * MW_RULER->px_per_tick;

  gtk_render_background (context, cr, 0, 0, MW_RULER->total_px, height);

  /* handle vertical drawing */
  for (int i = SPACE_BEFORE_START; i < MW_RULER->total_px; i++)
  {
    int actual_pos = i - SPACE_BEFORE_START;
    if (actual_pos == playhead_pos_in_px)
      {
          cairo_set_source_rgb (cr, 1, 0, 0);
          cairo_set_line_width (cr, 2);
          cairo_move_to (cr, i, 0);
          cairo_line_to (cr, i, height);
          cairo_stroke (cr);
      }
      if (actual_pos % MW_RULER->px_per_bar == 0)
      {
          cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
          cairo_set_line_width (cr, 1);
          cairo_move_to (cr, i, 0);
          cairo_line_to (cr, i, height);
          cairo_stroke (cr);
      }
      else if (actual_pos % MW_RULER->px_per_beat == 0)
      {
          cairo_set_source_rgb (cr, 0.25, 0.25, 0.25);
          cairo_set_line_width (cr, 0.5);
          cairo_move_to (cr, i, 0);
          cairo_line_to (cr, i, height);
          cairo_stroke (cr);
      }
  }

  /* handle horizontal drawing */
  GtkWidget * tracks = GTK_WIDGET (MAIN_WINDOW->tracks);
  int y_offset = 0;
  do
    {
      GtkWidget * track_widget = gtk_paned_get_child1 (
              GTK_PANED (tracks));

      gint wx, wy;
      gtk_widget_translate_coordinates(
                track_widget,
                widget,
                0,
                0,
                &wx,
                &wy);
      draw_borders (track_widget, cr, wy - 2);
      tracks = gtk_paned_get_child2 (
                      GTK_PANED (tracks));
  } while (GTK_IS_PANED (tracks));

  gint wx, wy;
  gtk_widget_translate_coordinates(
            GTK_WIDGET (tracks),
            widget,
            0,
            0,
            &wx,
            &wy);
  draw_borders (GTK_WIDGET (tracks), cr, wy - 2);

  return 0;
}

static gboolean
multipress_pressed (GtkGestureMultiPress *gesture,
               gint                  n_press,
               gdouble               x,
               gdouble               y,
               gpointer              user_data)
{
  RulerWidget * self = (RulerWidget *) user_data;
  gtk_widget_grab_focus (GTK_WIDGET (self));
}

static void
drag_begin (GtkGestureDrag * gesture,
               gdouble         start_x,
               gdouble         start_y,
               gpointer        user_data)
{
  RulerWidget * self = (RulerWidget *) user_data;
  self->start_x = start_x;
}

static void
drag_update (GtkGestureDrag * gesture,
               gdouble         offset_x,
               gdouble         offset_y,
               gpointer        user_data)
{
}

static void
drag_end (GtkGestureDrag *gesture,
               gdouble         offset_x,
               gdouble         offset_y,
               gpointer        user_data)
{
  RulerWidget * self = (RulerWidget *) user_data;
  self->start_x = 0;
}

TimelineBgWidget *
timeline_bg_widget_new ()
{
  TimelineBgWidget * self = g_object_new (TIMELINE_BG_WIDGET_TYPE, NULL);

  // set the size
  int ww, hh;
  TracksWidget * tracks = MAIN_WINDOW->tracks;
  gtk_widget_get_size_request (
    GTK_WIDGET (tracks),
    &ww,
    &hh);
  gtk_widget_set_size_request (
    GTK_WIDGET (self),
    MW_RULER->total_px,
    hh);

  gtk_widget_set_can_focus (GTK_WIDGET (self),
                           1);
  gtk_widget_set_focus_on_click (GTK_WIDGET (self),
                                 1);


  g_signal_connect (G_OBJECT (self), "draw",
                    G_CALLBACK (draw_cb), NULL);
  g_signal_connect (G_OBJECT(self->drag), "drag-begin",
                    G_CALLBACK (drag_begin),  self);
  g_signal_connect (G_OBJECT(self->drag), "drag-update",
                    G_CALLBACK (drag_update),  self);
  g_signal_connect (G_OBJECT(self->drag), "drag-end",
                    G_CALLBACK (drag_end),  self);
  g_signal_connect (G_OBJECT (self->multipress), "pressed",
                    G_CALLBACK (multipress_pressed), self);

  return self;
}

static void
timeline_bg_widget_class_init (TimelineBgWidgetClass * klass)
{
}

static void
timeline_bg_widget_init (TimelineBgWidget *self )
{
  self->drag = GTK_GESTURE_DRAG (
                gtk_gesture_drag_new (GTK_WIDGET (self)));
  self->multipress = GTK_GESTURE_MULTI_PRESS (
                gtk_gesture_multi_press_new (GTK_WIDGET (self)));
}
