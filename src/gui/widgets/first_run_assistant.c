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

/**
 * \file
 *
 */

#include "audio/engine.h"
#include "audio/engine_alsa.h"
#include "audio/engine_jack.h"
#include "audio/engine_pa.h"
#include "gui/widgets/first_run_assistant.h"
#include "gui/widgets/midi_controller_mb.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/localization.h"
#include "utils/io.h"
#include "utils/resources.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (FirstRunAssistantWidget,
               first_run_assistant_widget,
               GTK_TYPE_ASSISTANT)

static void
on_reset_clicked (
  GtkButton *widget,
  FirstRunAssistantWidget * self)
{
  g_message ("reset");
  char * dir =
    g_build_filename (io_get_home_dir (),
                      "zrythm",
                      NULL);
  gtk_file_chooser_select_filename (
    GTK_FILE_CHOOSER (self->fc_btn),
    dir);
  g_settings_set_string (
    S_GENERAL, "dir", dir);
  g_free (dir);
}

/**
 * Validates the current audio/MIDI backend
 * selection.
 */
static void
audio_midi_backend_selection_validate (
  FirstRunAssistantWidget * self)
{
  AudioBackend ab =
    (AudioBackend)
    gtk_combo_box_get_active (self->audio_backend);
  MidiBackend mb =
    (MidiBackend)
    gtk_combo_box_get_active (self->midi_backend);

  /* test audio backends */
  switch (ab)
    {
    case AUDIO_BACKEND_JACK:
#ifdef HAVE_JACK
      if (engine_jack_test (GTK_WINDOW (self)))
        return;
#else
      ui_show_error_message (
        self,
        _("JACK functionality is disabled"));
      return;
#endif
      break;
    case AUDIO_BACKEND_ALSA:
#ifdef HAVE_ALSA
      if (engine_alsa_test (GTK_WINDOW (self)))
        return;
#endif
      break;
    case AUDIO_BACKEND_PORT_AUDIO:
#ifdef HAVE_PORT_AUDIO
      if (engine_pa_test (GTK_WINDOW (self)))
        return;
#else
      ui_show_error_message (
        self,
        _("PortAudio functionality is disabled"));
      return;
#endif
      break;
    default:
      break;
    }

  /* test MIDI backends */
  switch (mb)
    {
    case MIDI_BACKEND_JACK:
#ifdef HAVE_JACK
      if (engine_jack_test (GTK_WINDOW (self)))
        return;
#else
      ui_show_error_message (
        self,
        _("JACK functionality is disabled"));
      return;
#endif
      break;
    case MIDI_BACKEND_ALSA:
#ifdef HAVE_ALSA
      if (engine_alsa_test (GTK_WINDOW (self)))
        return;
#endif
      break;
    default:
      break;
    }

  if (ab == AUDIO_BACKEND_ALSA &&
      mb == MIDI_BACKEND_JACK)
    {
      ui_show_error_message (
        self,
        _("Backend combination not supported"));
      return;
    }

  ui_show_error_message (
    self,
    _("The selected backends are operational"));
  return;
}

static void
on_test_backends_clicked (
  GtkButton * widget,
  FirstRunAssistantWidget * self)
{
  audio_midi_backend_selection_validate (self);
}

static void
on_audio_backend_changed (
  GtkComboBox *widget,
  FirstRunAssistantWidget * self)
{
  AudioBackend ab =
    (AudioBackend)
    gtk_combo_box_get_active (widget);

  /* update settings */
  g_settings_set_enum (
    S_PREFERENCES,
    "audio-backend",
    (int) ab);
}

static void
on_midi_backend_changed (
  GtkComboBox *widget,
  FirstRunAssistantWidget * self)
{
  MidiBackend mb =
    (MidiBackend)
    gtk_combo_box_get_active (widget);

  /* update settings */
  g_settings_set_enum (
    S_PREFERENCES,
    "midi-backend",
    (int) mb);
}

static void
on_language_changed (
  GtkComboBox *widget,
  FirstRunAssistantWidget * self)
{
  LocalizationLanguage lang =
    (LocalizationLanguage)
    gtk_combo_box_get_active (widget);

  /* update settings */
  g_settings_set_enum (
    S_PREFERENCES, "language", (int) lang);

  /* if locale exists */
  if (localization_init ())
    {
      /* enable "Next" */
      gtk_assistant_set_page_complete (
        GTK_ASSISTANT (self),
        gtk_assistant_get_nth_page (
          GTK_ASSISTANT (self), 0),
        1);
      gtk_widget_set_visible (
        GTK_WIDGET (self->locale_not_available),
        F_NOT_VISIBLE);
    }
  /* locale doesn't exist */
  else
    {
      /* disable "Next" */
      gtk_assistant_set_page_complete (
        GTK_ASSISTANT (self),
        gtk_assistant_get_nth_page (
          GTK_ASSISTANT (self), 0),
        0);

      /* show warning */
      char * str =
        ui_get_locale_not_available_string (lang);
      gtk_label_set_markup (
        self->locale_not_available, str);
      g_free (str);

      gtk_widget_set_visible (
        GTK_WIDGET (self->locale_not_available),
        F_VISIBLE);
    }
}

static void
on_file_set (
  GtkFileChooserButton *widget,
  FirstRunAssistantWidget * self)
{
  GFile * file =
    gtk_file_chooser_get_file (
      GTK_FILE_CHOOSER (widget));
  char * str =
    g_file_get_path (file);
  g_settings_set_string (
    S_GENERAL, "dir", str);
  char * str2 =
    g_build_filename (
      str, ZRYTHM_PROJECTS_DIR, NULL);
  g_settings_set_string (
    S_GENERAL, "last-project-dir", str);
  g_free (str);
  g_free (str2);
}

FirstRunAssistantWidget *
first_run_assistant_widget_new (
  GtkWindow * parent)
{
  FirstRunAssistantWidget * self =
    g_object_new (
      FIRST_RUN_ASSISTANT_WIDGET_TYPE,
      "modal", 1,
      "urgency-hint", 1,
      NULL);

  /* setup languages */
  ui_setup_language_combo_box (
    self->language_cb);

  /* setup backends */
  ui_setup_audio_backends_combo_box (
    self->audio_backend);
  ui_setup_midi_backends_combo_box (
    self->midi_backend);

  /* set zrythm dir */
  char * dir;
  dir = zrythm_get_dir (ZRYTHM);
  if (strlen (dir) == 0)
    {
      g_free (dir);
      dir = g_build_filename (io_get_home_dir (),
                        "zrythm",
                        NULL);
    }
  io_mkdir (dir);
  gtk_file_chooser_set_current_folder (
    GTK_FILE_CHOOSER (self->fc_btn),
    dir);
  g_settings_set_string (
    S_GENERAL, "dir", dir);
  g_free (dir);

  /*g_message (*/
    /*"n pages %d",*/
    /*gtk_assistant_get_n_pages (*/
      /*GTK_ASSISTANT (self)));*/

  gtk_window_set_position (
    GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_transient_for (
    GTK_WINDOW (self), parent);
  gtk_window_set_attached_to (
    GTK_WINDOW (self), GTK_WIDGET (parent));
  gtk_window_set_keep_above (
    GTK_WINDOW (self), 1);

  return self;
}

static void
first_run_assistant_widget_class_init (
  FirstRunAssistantWidgetClass * _klass)
{
  GtkWidgetClass * klass =
    GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "first_run_assistant.ui");

#define BIND(x) \
  gtk_widget_class_bind_template_child ( \
    klass, \
    FirstRunAssistantWidget, \
    x)

  BIND (language_cb);
  BIND (audio_backend);
  BIND (midi_backend);
  BIND (locale_not_available);
  BIND (fc_btn);
  BIND (reset);
  BIND (test_backends);

#undef BIND
}

static void
first_run_assistant_widget_init (
  FirstRunAssistantWidget * self)
{
  g_type_ensure (MIDI_CONTROLLER_MB_WIDGET_TYPE);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (
    G_OBJECT (self->language_cb), "changed",
    G_CALLBACK (on_language_changed), self);
  g_signal_connect (
    G_OBJECT (self->audio_backend), "changed",
    G_CALLBACK (on_audio_backend_changed), self);
  g_signal_connect (
    G_OBJECT (self->midi_backend), "changed",
    G_CALLBACK (on_midi_backend_changed), self);
  g_signal_connect (
    G_OBJECT (self->language_cb), "changed",
    G_CALLBACK (on_language_changed), self);
  g_signal_connect (
    G_OBJECT (self->fc_btn), "file-set",
    G_CALLBACK (on_file_set), self);
  g_signal_connect (
    G_OBJECT (self->reset), "clicked",
    G_CALLBACK (on_reset_clicked), self);
#ifdef _WIN32
  gtk_widget_set_visible (
    GTK_WIDGET (self->test_backends), 0);
#else
  g_signal_connect (
    G_OBJECT (self->test_backends), "clicked",
    G_CALLBACK (on_test_backends_clicked), self);
#endif
}
