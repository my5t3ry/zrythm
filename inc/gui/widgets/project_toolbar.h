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

#ifndef __GUI_WIDGETS_PROJECT_TOOLBAR_H__
#define __GUI_WIDGETS_PROJECT_TOOLBAR_H__

#include <gtk/gtk.h>

#define PROJECT_TOOLBAR_WIDGET_TYPE \
  (project_toolbar_widget_get_type ())
G_DECLARE_FINAL_TYPE (ProjectToolbarWidget,
                      project_toolbar_widget,
                      Z,
                      PROJECT_TOOLBAR_WIDGET,
                      GtkToolbar)

/**
 * \file
 */

#define MW_PROJECT_TOOLBAR \
  MW_HEADER_NOTEBOOK->project_toolbar

typedef struct _ProjectToolbarWidget
{
  GtkToolbar         parent_instance;
  GtkToolButton *    new;
  GtkToolButton *    open;
  GtkToolButton *    export_as;
} ProjectToolbarWidget;

#endif