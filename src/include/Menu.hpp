#pragma once

enum MenuState_ {
    MenuState_Gameplay,
    MenuState_PauseMenu,
    MenuState_Options
};
typedef int MenuState;

// returns true if pause menu opened (only once not while pause menu open)
bool ShowMenu(); 

MenuState GetMenuState();