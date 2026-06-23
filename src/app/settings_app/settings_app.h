/****************************************************************************
 *   Copyright  2024
 *
 *   Settings launcher app - adds a discoverable "settings" icon to the app
 *   grid that jumps straight to the setup tile ( where wlan / bluetooth /
 *   display / ... settings live ).  Without this the setup tile is only
 *   reachable by swiping up from the app grid or via the long-press quickbar
 *   gear, which is not obvious to new users.
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _SETTINGS_APP_H
    #define _SETTINGS_APP_H

    /**
     * @brief setup routine for the settings launcher app
     */
    void settings_app_setup( void );

#endif // _SETTINGS_APP_H
