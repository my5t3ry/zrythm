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

#include "gui/backend/clip_editor.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/editor_toolbar.h"
#include "utils/resources.h"

G_DEFINE_TYPE (ClipEditorWidget,
               clip_editor_widget,
               GTK_TYPE_STACK)

void
clip_editor_widget_setup (
  ClipEditorWidget * self)
{
  clip_editor_inner_widget_setup (
    self->clip_editor_inner);

  gtk_stack_set_visible_child (
    GTK_STACK (self),
    GTK_WIDGET (self->no_selection_label));
}

static void
clip_editor_widget_init (ClipEditorWidget * self)
{
  g_type_ensure (CLIP_EDITOR_INNER_WIDGET_TYPE);
  g_type_ensure (EDITOR_TOOLBAR_WIDGET_TYPE);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
clip_editor_widget_class_init (
  ClipEditorWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "clip_editor.ui");

  gtk_widget_class_set_css_name (
    klass, "clip-editor");

  gtk_widget_class_bind_template_child (
    klass,
    ClipEditorWidget,
    main_box);
  gtk_widget_class_bind_template_child (
    klass,
    ClipEditorWidget,
    editor_toolbar);
  gtk_widget_class_bind_template_child (
    klass,
    ClipEditorWidget,
    clip_editor_inner);
  gtk_widget_class_bind_template_child (
    klass,
    ClipEditorWidget,
    no_selection_label);
}
