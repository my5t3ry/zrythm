/*
 * Copyright (C) 2018-2019 Alexandros Theodotou
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

/** \file
 * Implementation of Port API.
 * Ports are passed when processing audio signals. They carry buffers
 * with data */

#define M_PIF 3.14159265358979323846f

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "project.h"
#include "audio/channel.h"
#include "audio/mixer.h"
#include "audio/pan.h"
#include "audio/port.h"
#include "plugins/plugin.h"
#include "utils/arrays.h"

#include <gtk/gtk.h>

#include <jack/jack.h>

#define SLEEPTIME_USEC 60

typedef jack_default_audio_sample_t   sample_t;
typedef jack_nframes_t                nframes_t;

/**
 * Inits ports just loaded from yml.
 */
void
port_init_loaded (Port * port)
{
  /* set caches */
  for (int j = 0; j < port->num_srcs; j++)
    port->srcs[j] =
      project_get_port (port->src_ids[j]);
  for (int j = 0; j < port->num_dests; j++)
    port->dests[j] =
      project_get_port (port->dest_ids[j]);
  port->owner_pl =
    project_get_plugin (port->owner_pl_id);
  port->owner_ch =
    project_get_channel (port->owner_ch_id);
}

/**
 * Creates port.
 *
 * Sets id and updates appropriate counters.
 */
Port *
port_new (char * label)
{
  Port * port = calloc (1, sizeof (Port));

  port->id = PROJECT->num_ports;
  PROJECT->ports[PROJECT->num_ports++] = port;
  port->num_dests = 0;
  port->buf = calloc (AUDIO_ENGINE->block_length, sizeof (sample_t));
  port->flow = FLOW_UNKNOWN;
  port->label = g_strdup (label);

  g_message ("[port_new] Creating port %s", port->label);

  return port;
}

/**
 * Creates port.
 */
Port *
port_new_with_type (PortType     type,
                    PortFlow     flow,
                    char         * label)
{
  Port * port = port_new (label);

  port->type = type;
  if (port->type == TYPE_EVENT)
    port->midi_events = midi_events_new (1);
  port->flow = flow;

  return port;
}

/**
 * Creates stereo ports.
 */
StereoPorts *
stereo_ports_new (Port * l, Port * r)
{
  StereoPorts * sp = calloc (1, sizeof (StereoPorts));
  sp->l = l;
  sp->r = r;

  return sp;
}

/**
 * Creates port and adds given data to it.
 */
Port *
port_new_with_data (PortInternalType internal_type, ///< the internal data format
                    PortType     type,
                    PortFlow     flow,
                    char         * label,
                    void         * data)   ///< the data
{
  Port * port = port_new_with_type (type, flow, label);

  /** TODO semaphore **/
  port->data = data;
  port->internal_type = internal_type;

  /** TODO end semaphore */

  return port;
}

/**
 * Sets the owner channel & its ID.
 */
void
port_set_owner_channel (Port *    port,
                        Channel * chan)
{
  port->owner_ch = chan;
  port->owner_ch_id = chan->id;
}

/**
 * Sets the owner plugin & its ID.
 */
void
port_set_owner_plugin (Port *   port,
                       Plugin * pl)
{
  port->owner_pl = pl;
  port->owner_pl_id = pl->id;
}

/**
 * Deletes port, doing required cleanup and updating counters.
 */
void
port_free (Port * port)
{
  port_disconnect_all (port);

  if (port->label)
    g_free (port->label);
  if (port->buf)
    free (port->buf);

  array_delete (PROJECT->ports, PROJECT->num_ports, port);
  free (port);
}

/**
 */
/*void*/
/*port_init (Port * port)*/
/*{*/
  /*port->id = MIXER->num_ports++;*/
  /*port->num_destinations = 0;*/
/*}*/

/**
 * Connets src to dest.
 */
int
port_connect (Port * src, Port * dest)
{
  port_disconnect (src, dest);
  if (src->type != dest->type)
    {
      g_warning ("Cannot connect ports, incompatible types");
      return -1;
    }
  src->dests[src->num_dests] = dest;
  src->dest_ids[src->num_dests++] = dest->id;
  dest->srcs[dest->num_srcs] = src;
  dest->src_ids[dest->num_srcs++] = src->id;
  g_message ("Connected port %d(%s) to %d(%s)",
             src->id,
             src->label,
             dest->id,
             dest->label);
  return 0;
}

/*static void*/
/*array_delete (Port ** array, int * size, Port * element)*/
/*{*/
  /*for (int i = 0; i < (* size); i++)*/
    /*{*/
      /*if (array[i] == element)*/
        /*{*/
          /*--(* size);*/
          /*for (int j = i; j < (* size); j++)*/
            /*{*/
              /*array[j] = array[j + 1];*/
            /*}*/
          /*break;*/
        /*}*/
    /*}*/
/*}*/

/**
 * Disconnects src from dest.
 */
int
port_disconnect (Port * src, Port * dest)
{
  if (!src || !dest)
    g_warning ("port_disconnect: src or dest is NULL");

  /* disconnect dest from src */
  array_delete (src->dests, src->num_dests, dest);
  array_delete (dest->srcs, dest->num_srcs, src);
  return 0;
}

int
ports_connected (Port * src, Port * dest)
{
  if (array_contains ((void **)src->dests,
                      src->num_dests,
                      dest))
    return 1;
  return 0;
}

/**
 * Disconnects all srcs and dests from port.
 */
int
port_disconnect_all (Port * port)
{
  if (!port)
    g_warning ("port_disconnect_all: port is NULL");

  FOREACH_SRCS (port)
    {
      Port * src = port->srcs[i];
      port_disconnect (src, port);
    }

  FOREACH_DESTS (port)
    {
      Port * dest = port->dests[i];
      port_disconnect (port, dest);
    }

  return 0;
}

/**
 * if port buffer size changed, reallocate port buffer, otherwise memset to 0.
 */
/*void*/
/*port_init_buf (Port *port, nframes_t nframes)*/
/*{*/
    /*[> if port buf size changed, reallocate <]*/
    /*if (port->nframes != nframes ||*/
        /*port->nframes == 0)*/
      /*{*/
        /*if (port->nframes > 0)*/
          /*free (port->buf);*/
        /*port->buf = calloc (nframes, sizeof (sample_t));*/
        /*port->nframes = nframes;*/
      /*}*/
    /*else [> otherwise memset to 0 <]*/
      /*{*/
        /*memset (port->buf, '\0', nframes);*/
      /*}*/
/*}*/

/**
 * Apply given fader value to port.
 */
void
port_apply_fader (Port * port, float amp)
{
  int nframes = AUDIO_ENGINE->block_length;
  for (int i = 0; i < nframes; i++)
    {
      if (port->buf[i] != 0.f)
        port->buf[i] *= amp;
    }
}


/**
 * First sets port buf to 0, then sums the given port signal from its inputs.
 */
void
port_sum_signal_from_inputs (Port * port)
{
  /*port_init_buf (port, nframes);*/
  int nframes = AUDIO_ENGINE->block_length;

  /* for any output port pointing to it */
  for (int k = 0; k < port->num_srcs; k++)
    {
      Port * src_port = port->srcs[k];

      /* wait for owner to finish processing */
      if (src_port->is_piano_roll)
        {
        }
      else if (src_port->owner_pl)
        {
          /*g_message ("%s owner port",*/
                     /*src_port->owner_pl->descr->name);*/
          while (!src_port->owner_pl->processed)
            {
              g_usleep (SLEEPTIME_USEC);
            }
          /*g_message ("%s owner port done",*/
                     /*src_port->owner_pl->descr->name);*/
        }
      else if (src_port->owner_ch)
        {
          /*g_message ("%s port, %s owner channel",*/
                     /*src_port->label,*/
                     /*src_port->owner_ch->name);*/
          while (!src_port->owner_ch->processed &&
                 src_port !=
                   src_port->owner_ch->stereo_in->l &&
                 src_port !=
                   src_port->owner_ch->stereo_in->r)
            {
              g_usleep (SLEEPTIME_USEC);
            }
          /*g_message ("%s port, %s owner channel done",*/
                     /*src_port->label,*/
                     /*src_port->owner_ch->name);*/
        }

      /* sum the signals */
      if (port->type == TYPE_AUDIO)
        {
          for (int l = 0; l < nframes; l++)
            {
              port->buf[l] += src_port->buf[l];
            }
        }
      else if (port->type == TYPE_EVENT)
        {
          midi_events_append (src_port->midi_events,
                              port->midi_events);
        }
    }
}

/**
 * Prints all connections.
 */
void
port_print_connections_all ()
{
  for (int i = 0; i < PROJECT->num_ports; i++)
    {
      Port * src = PROJECT->ports[i];
      if (!src->owner_pl && !src->owner_ch && !src->owner_jack)
        {
          g_warning ("Port %s has no owner", src->label);
        }
      for (int j = 0; j < src->num_dests; j++)
        {
          Port * dest = src->dests[j];
          g_assert (dest);
          g_message ("%s connected to %s", src->label, dest->label);
        }
    }
}

/**
 * Clears the port buffer.
 */
void
port_clear_buffer (Port * port)
{
  if (port->type == TYPE_AUDIO)
    {
      if (port->buf)
        memset (port->buf,
                0,
                AUDIO_ENGINE->block_length * sizeof (float));
    }
  else if (port->type == TYPE_EVENT)
    {
      if (port->midi_events)
        port->midi_events->num_events = 0;
    }
}

/**
 * Applies the pan to the given L/R ports.
 */
void
port_apply_pan_stereo (Port *       l,
                       Port *       r,
                       float        pan,
                       PanLaw       pan_law,
                       PanAlgorithm pan_algo)
{
  int nframes = AUDIO_ENGINE->block_length;
  if (pan_algo == PAN_ALGORITHM_SINE_LAW)
    {
      for (int i = 0; i < nframes; i++)
        {
          if (r->buf[i] != 0.f)
            r->buf[i] *= sinf (pan * (M_PIF / 2.f));
          if (l->buf[i] != 0.f)
            l->buf[i] *= sinf ((1.f - pan) * (M_PIF / 2.f));
        }

    }
}

