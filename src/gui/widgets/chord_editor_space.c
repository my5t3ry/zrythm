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

#include "audio/channel.h"
#include "audio/region.h"
#include "audio/track.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/chord_editor_space.h"
#include "gui/widgets/chord_key.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/chord_arranger.h"
#include "gui/widgets/editor_ruler.h"
#include "gui/widgets/ruler.h"
#include "project.h"
#include "utils/gtk.h"
#include "utils/resources.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (
  ChordEditorSpaceWidget,
  chord_editor_space_widget,
  GTK_TYPE_BOX)

#define DEFAULT_PX_PER_KEY 12

/**
 * Links scroll windows after all widgets have been
 * initialized.
 */
static void
link_scrolls (
  ChordEditorSpaceWidget * self)
{
  /* link ruler h scroll to arranger h scroll */
  if (MW_CLIP_EDITOR_INNER->ruler_scroll)
    {
      gtk_scrolled_window_set_hadjustment (
        MW_CLIP_EDITOR_INNER->ruler_scroll,
        gtk_scrolled_window_get_hadjustment (
          self->arranger_scroll));
    }
}

/**
 * See CLIP_EDITOR_INNER_WIDGET_ADD_TO_SIZEGROUP.
 */
void
chord_editor_space_widget_update_size_group (
  ChordEditorSpaceWidget * self,
  int                     visible)
{
  CLIP_EDITOR_INNER_WIDGET_ADD_TO_SIZEGROUP (
    chord_keys_box);
}

void
chord_editor_space_widget_refresh (
  ChordEditorSpaceWidget * self)
{
  link_scrolls (self);
}

void
chord_editor_space_widget_setup (
  ChordEditorSpaceWidget * self)
{
  self->px_per_key =
    DEFAULT_PX_PER_KEY *
    (int) PIANO_ROLL->notes_zoom;
  self->total_key_px =
    self->px_per_key * CHORD_EDITOR->num_chords;

  if (self->arranger)
    {
      arranger_widget_setup (
        Z_ARRANGER_WIDGET (self->arranger),
        ARRANGER_WIDGET_TYPE_CHORD,
        &PROJECT->snap_grid_midi);
      gtk_widget_show_all (
        GTK_WIDGET (self->arranger));
    }

  for (int i = 0; i < CHORD_EDITOR->num_chords; i++)
    {
      self->chord_keys[i] =
        chord_key_widget_new (CHORD_EDITOR->chords[i]);
      GtkBox * box =
        GTK_BOX (
          gtk_box_new (GTK_ORIENTATION_HORIZONTAL,
                       0));
      gtk_widget_set_visible (
        GTK_WIDGET (box), 1);
      gtk_box_pack_start (
        box, GTK_WIDGET (self->chord_keys[i]),
        1, 1, 0);
      z_gtk_widget_add_style_class (
        GTK_WIDGET (box), "chord_key");
      gtk_container_add (
        GTK_CONTAINER (self->chord_keys_box),
        GTK_WIDGET (box));
      self->chord_key_boxes[i] = box;
    }

  chord_editor_space_widget_refresh (self);
}

static void
chord_editor_space_widget_init (
  ChordEditorSpaceWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->arranger->type =
    ARRANGER_WIDGET_TYPE_CHORD;
}

static void
chord_editor_space_widget_class_init (
  ChordEditorSpaceWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass,
    "chord_editor_space.ui");

  gtk_widget_class_bind_template_child (
    klass,
    ChordEditorSpaceWidget,
    arranger_scroll);
  gtk_widget_class_bind_template_child (
    klass,
    ChordEditorSpaceWidget,
    arranger_viewport);
  gtk_widget_class_bind_template_child (
    klass,
    ChordEditorSpaceWidget,
    arranger);
  gtk_widget_class_bind_template_child (
    klass,
    ChordEditorSpaceWidget,
    chord_keys_box);
}
