/*
 * Copyright (C) 2002, 2006-2008, 2010-2011, 2013 by the Widelands Development Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef WL_UI_FSMENU_LOADGAME_H
#define WL_UI_FSMENU_LOADGAME_H

#include "ui_fsmenu/base.h"

#include <memory>

#include "graphic/image.h"
#include "io/filesystem/filesystem.h"
#include "ui_basic/button.h"
#include "ui_basic/checkbox.h"
#include "ui_basic/icon.h"
#include "ui_basic/listselect.h"
#include "ui_basic/multilinetextarea.h"
#include "ui_basic/textarea.h"
#include "ui_fsmenu/load_map_or_game.h"

namespace Widelands {
class EditorGameBase;
class Game;
class Map;
class MapLoader;
}
class Image;
class RenderTarget;
class GameController;
struct GameSettingsProvider;

/// Select a Saved Game in Fullscreen Mode. It's a modal fullscreen menu.
struct FullscreenMenuLoadGame : public FullscreenMenuLoadMapOrGame {
	FullscreenMenuLoadGame
		(Widelands::Game&, GameSettingsProvider* gsp = nullptr, GameController* gc = nullptr);

	const std::string & filename() {return m_filename;}

	void think();

	bool handle_key(bool down, SDL_keysym code) override;

private:
	void clicked_ok();
	void clicked_delete();
	void map_selected(uint32_t);
	void double_clicked(uint32_t);
	void fill_list();
	void no_selection();

	UI::Textarea                  m_title;
	UI::Textarea                  m_label_mapname;
	UI::MultilineTextarea         m_ta_mapname;  // Multiline for long names
	UI::Textarea                  m_label_gametime;
	UI::MultilineTextarea         m_ta_gametime; // Multiline because we want tooltips
	UI::Textarea                  m_label_players;
	UI::MultilineTextarea         m_ta_players;
	UI::Textarea                  m_label_win_condition;
	UI::MultilineTextarea         m_ta_win_condition;
	UI::Button                    m_delete;
	int32_t const                 m_minimap_max_width;
	int32_t const                 m_minimap_max_height;
	UI::Icon                      m_minimap_icon;

	std::unique_ptr<const Image>  m_minimap_image;
	UI::Listselect<const char*>   m_list;

	Widelands::Game&              m_game;
	GameSettingsProvider*         m_settings;
	GameController*               m_ctrl;

	FilenameSet                   m_gamefiles;
	std::string                   m_filename;
};


#endif  // end of include guard: WL_UI_FSMENU_LOADGAME_H
