/*
 * Copyright (C) 2002-2004, 2006-2008 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef GAME_INTERACTIVE_PLAYER_DATA_PACKET_H
#define GAME_INTERACTIVE_PLAYER_DATA_PACKET_H

#include "game_data_packet.h"

namespace Widelands {

/*
 * Information about the interactive player. Mostly scrollpos,
 * player number and so on
 */
struct Game_Interactive_Player_Data_Packet : public Game_Data_Packet {
	void Read (FileSystem &, Game *, Map_Map_Object_Loader * = 0)
		throw (_wexception);
	void Write(FileSystem &, Game *, Map_Map_Object_Saver  * = 0)
		throw (_wexception);
};

};

#endif
