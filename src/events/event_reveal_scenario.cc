/*
 * Copyright (C) 2007-2008 by the Widelands Development Team
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

#include "event_reveal_scenario.h"

#include "campvis.h"
#include "profile.h"

namespace Widelands {

Event_Reveal_Scenario::Event_Reveal_Scenario
	(Section & s, Editor_Game_Base & egbase)
	: Event_Reveal(s, egbase)
{}


void Event_Reveal_Scenario::Write(Section & s, Editor_Game_Base & egbase) const
{
	s.set_string("type", "reveal_scenario");
	Event_Reveal::Write(s, egbase);
}


Event::State Event_Reveal_Scenario::run(Game *) {
	Campaign_visibility_save cvs;
	cvs.set_map_visibility(reveal, true);
	return m_state = DONE;
}

};
