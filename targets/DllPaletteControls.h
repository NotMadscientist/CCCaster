#pragma once

#include "MouseManager.h"
#include "DllPaletteManager.h"

#include <unordered_map>


class DllPaletteControls : public MouseManager::Owner
{
    bool enabling = false, disabling = false;

    uint32_t flushing = 0;

protected:

    std::unordered_map<uint32_t, DllPaletteManager> palMans;

public:

    // Indicates if the palette editor is enabled
    bool isPaletteEditorEnabled() const;

    // Update palette editor controls and state
    void updatePaletteEditor();

    // MouseManager callback
    void mouseEvent ( int x, int y, bool isDown, bool pressed, bool released ) override;
};
