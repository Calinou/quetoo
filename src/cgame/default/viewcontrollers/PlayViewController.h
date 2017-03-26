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

#include "MenuViewController.h"

/**
 * @file
 * @brief System ViewController.
 */

typedef struct PlayViewController PlayViewController;
typedef struct PlayViewControllerInterface PlayViewControllerInterface;

/**
 * @brief The PlayViewController type.
 * @extends MenuViewController
 * @ingroup
 */
struct PlayViewController {

	/**
	 * @brief The superclass.
	 * @private
	 */
	MenuViewController menuViewController;

	/**
	 * @brief The interface.
	 * @private
	 */
	PlayViewControllerInterface *interface;

	/**
	 * @brief NavigationViewController to contain the individual settings panels.
	 */
	NavigationViewController *navigationViewController;
};

/**
 * @brief The PlayViewController interface.
 */
struct PlayViewControllerInterface {

	/**
	 * @brief The superclass interface.
	 */
	MenuViewControllerInterface menuViewControllerInterface;
};

/**
 * @fn Class *PlayViewController::_PlayViewController(void)
 * @brief The PlayViewController archetype.
 * @return The PlayViewController Class.
 * @memberof PlayViewController
 */
extern Class *_PlayViewController(void);
