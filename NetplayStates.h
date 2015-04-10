#pragma once

#include "Enum.h"


/* Netplay state transitions

    Unknown -> PreInitial -> Initial -> { AutoCharaSelect (spectate only), CharaSelect }

    { AutoCharaSelect (spectate only), CharaSelect } -> Loading

    Loading -> { Skippable, InGame (training mode) }

    Skippable -> { InGame (versus mode), RetryMenu }

    InGame -> { Skippable, CharaSelect (not on netplay) }

    RetryMenu -> { Loading, CharaSelect }

*/


// PreInitial: The period while we are preparing communication channels
// Initial: The game starting phase
// AutoCharaSelect: Automatic character select (spectate only)
// CharaSelect: Character select
// Loading: Loading screen, distinct from skippable, so we can transition properly
// Skippable: Skippable states (chara intros, round transitions, post-game, pre-retry)
// InGame: In-game state
// RetryMenu: Post-game retry menu
ENUM ( NetplayState, PreInitial, Initial, AutoCharaSelect, CharaSelect, Loading, Skippable, InGame, RetryMenu );
