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

#ifndef __UNDO_COPY_PLUGINS_ACTION_H__
#define __UNDO_COPY_PLUGINS_ACTION_H__

#include "actions/undoable_action.h"
#include "plugins/plugin.h"

/**
 * @addtogroup actions
 *
 * @{
 */

typedef struct Plugin Plugin;
typedef struct Channel Channel;
typedef struct MixerSelections MixerSelections;

typedef struct CopyPluginsAction
{
  UndoableAction  parent_instance;

  /** Starting slot to copy plugins to. */
  int              slot;

  /** Whether the plugin will be copied into a new
   * Channel or not. */
  int              is_new_channel;

  /** Track position to copy to, if existing, or track
   * position copied to, when undoing. */
  int              track_pos;

  /** Clones of the original Plugins.
   *
   * These must not be used as is. They must be
   * cloned.
   *
   * When doing, their IDs should be set to the newly
   * copied plugins, so they can be found when
   * undoing.
   */
  MixerSelections * ms;
} CopyPluginsAction;

UndoableAction *
copy_plugins_action_new (
  MixerSelections * ms,
  Track *           tr,
  int               slot);

int
copy_plugins_action_do (
	CopyPluginsAction * self);

int
copy_plugins_action_undo (
	CopyPluginsAction * self);

char *
copy_plugins_action_stringize (
	CopyPluginsAction * self);

void
copy_plugins_action_free (
	CopyPluginsAction * self);

/**
 * @}
 */

#endif
