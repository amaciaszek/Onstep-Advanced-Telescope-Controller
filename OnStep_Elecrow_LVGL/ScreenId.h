#pragma once

// This lives in a header because Arduino generates function prototypes before
// the sketch body. buildTabBar() uses ScreenId in its signature, so the enum
// must be visible before Arduino inserts those generated prototypes.
enum ScreenId { SCR_HOME = 0, SCR_SKY, SCR_CATALOG, SCR_SETTINGS, SCR_COUNT };
