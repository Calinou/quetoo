/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#pragma once

#include "collision/cmodel.h"
#include "console.h"
#include "filesystem.h"
#include "cgame/cgame.h"
#include "net/net_chan.h"
#include "renderer/renderer.h"
#include "sound/sound.h"
#include "thread.h"
#include "ui/ui.h"

#include "cl_cgame.h"
#include "cl_cmd.h"
#include "cl_console.h"
#include "cl_demo.h"
#include "cl_entity.h"
#include "cl_http.h"
#include "cl_input.h"
#include "cl_keys.h"
#include "cl_main.h"
#include "cl_media.h"
#include "cl_mouse.h"
#include "cl_parse.h"
#include "cl_predict.h"
#include "cl_screen.h"
#include "cl_server.h"
#include "cl_types.h"
#include "cl_view.h"
