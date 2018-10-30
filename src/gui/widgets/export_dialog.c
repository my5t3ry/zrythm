/*
 * gui/widgets/export_dialog.c - Export audio dialog
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

#include "audio/engine.h"
#include "audio/exporter.h"
#include "gui/widgets/export_dialog.h"
#include "project.h"
#include "utils/io.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE (ExportDialogWidget, export_dialog_widget, GTK_TYPE_DIALOG)

ExportDialogWidget *
export_dialog_widget_new ()
{
  ExportDialogWidget * self = g_object_new (EXPORT_DIALOG_WIDGET_TYPE, NULL);

  return self;
}

static void
on_arrangement_clicked (ExportDialogWidget * self,
                        gpointer user_data)
{
}

static void
on_loop_clicked (ExportDialogWidget * self,
                 gpointer user_data)
{
}

static void
on_cancel_clicked (ExportDialogWidget * self,
                   gpointer user_data)
{
  gtk_widget_destroy (GTK_WIDGET (user_data));
}

/**
 * FIXME doesn't work
 */
static void
on_close (ExportDialogWidget * self,
          gpointer user_data)
{
  g_message ("close clicked");
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
on_export_clicked (ExportDialogWidget * self,
                   gpointer user_data)
{
  ExportInfo info;
  info.format = AUDIO_FORMAT_FLAC;
  info.depth = BIT_DEPTH_24;
  info.file_uri = g_strdup_printf ("%s%sexports%sMaster.FLAC",
                                   PROJECT->dir,
                                   io_get_separator (),
                                   io_get_separator ());
  g_message ("exporting");
  AUDIO_ENGINE->run = 0;
  exporter_export (&info);
  AUDIO_ENGINE->run = 1;
  g_message ("exported");
  g_free (info.file_uri);
}

static void
export_dialog_widget_class_init (ExportDialogWidgetClass * klass)
{
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/online/alextee/zrythm/ui/export_dialog.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        cancel_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        export_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        pattern_cb);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        tracks_list);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        arrangement_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        start_box);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        loop_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        end_box);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        format_cb);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        dither_check);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass),
                                        ExportDialogWidget,
                                        output_label);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass),
                                           on_arrangement_clicked);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass),
                                           on_loop_clicked);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass),
                                           on_cancel_clicked);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass),
                                           on_export_clicked);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass),
                                           on_close);
}

static void
export_dialog_widget_init (ExportDialogWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}