/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018 Folke Will <folko@solhost.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "Screen.h"

namespace avitab {

Screen::Screen():
    Widget(nullptr)
{
    lv_obj_t *screen = lv_scr_act();

    lv_theme_t * th = lv_theme_get_current();
    if(th) {
        lv_obj_set_style(screen, th->style.bg);
    }
    setObj(screen);
    setManaged();
}

} /* namespace avitab */
