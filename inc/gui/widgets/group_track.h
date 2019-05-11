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

/**
 * \file
 *
 * Bus track for effects.
 */

#ifndef __GUI_WIDGETS_GROUP_TRACK_H__
#define __GUI_WIDGETS_GROUP_TRACK_H__

#include "audio/channel.h"
#include "gui/widgets/track.h"

#include <gtk/gtk.h>

#define GROUP_TRACK_WIDGET_TYPE \
  (group_track_widget_get_type ())
G_DECLARE_FINAL_TYPE (GroupTrackWidget,
                      group_track_widget,
                      Z,
                      GROUP_TRACK_WIDGET,
                      TrackWidget)

typedef struct _AutomationTracklistWidget
  AutomationTracklistWidget;
typedef struct Track GroupTrack;

/**
 * Top is the track part and bot is the automation part
 */
typedef struct _GroupTrackWidget
{
  TrackWidget                parent_instance;
  GtkToggleButton *          record;
  GtkToggleButton *          solo;
  GtkToggleButton *          mute;
  GtkToggleButton *          show_automation;
} GroupTrackWidget;

/**
 * Creates a new track widget from the given track.
 */
GroupTrackWidget *
group_track_widget_new (Track * track);

/**
 * Updates changes in the backend to the ui
 */
void
group_track_widget_refresh (GroupTrackWidget * self);

void
group_track_widget_refresh_buttons (
  GroupTrackWidget * self);

#endif
