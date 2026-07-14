#pragma once
#include "Display.h"

// Dark-site-safe instrument palette.  The surfaces are intentionally close
// together; hierarchy comes from spacing, hairlines and semantic color rather
// than thick boxes.
namespace Theme {
    constexpr Color BG       {  5,  8, 12};   // #05080C blue-black
    constexpr Color PANEL    { 17, 24, 39};   // #111827 raised card
    constexpr Color PANEL2   { 10, 15, 25};   // inset / dock
    constexpr Color PANEL3   { 22, 31, 45};   // selected surface
    constexpr Color BORDER   { 38, 50, 68};   // #263244 subtle outline
    constexpr Color HAIRLINE { 25, 34, 47};   // faint divider
    constexpr Color TEXT     {221,230,240};   // #DDE6F0 primary
    constexpr Color TEXT2    {154,171,193};   // cool secondary
    constexpr Color DIM      { 91,105,126};   // inactive / hints
    constexpr Color BLUE     { 56,167,255};   // motion / coordinates
    constexpr Color CYAN     {  0,229,255};   // active input emphasis
    constexpr Color GREEN    { 49,214,123};   // good / tracking
    constexpr Color YELLOW   {255,202, 58};   // paused / settling
    constexpr Color RED      {255, 65, 65};   // stop / alert
    constexpr Color REDDARK  { 43, 10, 15};   // alert surface
    constexpr Color OFF      { 47, 58, 75};   // unlit indicator

    constexpr Color OK     = GREEN;
    constexpr Color WARN   = YELLOW;
    constexpr Color DANGER = RED;
    constexpr Color ACCENT = BLUE;
    constexpr Color MUTED  = DIM;
}
