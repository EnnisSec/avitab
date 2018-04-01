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
#include "App.h"

namespace avitab {

App::App(FuncsPtr appFuncs, ContPtr container):
    funcs(appFuncs),
    uiContainer(container)
{
    if (!funcs) {
        throw std::runtime_error("No API passed to app");
    }
}

AppFunctions& App::api() {
    return *funcs;
}

void App::setOnExit(ExitFunct onExitFunct) {
    onExit = onExitFunct;
}

App::ExitFunct& App::getOnExit() {
    return onExit;
}

void App::exit() {
    funcs->executeLater([this] () {
        if (onExit) {
            onExit();
        }
    });
}

App::ContPtr App::getContainer() {
    return uiContainer;
}

} /* namespace avitab */