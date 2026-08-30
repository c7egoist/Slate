"use strict";
/* ==========================================================================
   udim.js -- UDIM tile model. The single 0-1 UV square becomes an addressable
   grid of 1x1 tiles. A tile (tu,tv) has the UDIM number tu + 1 + tv*10 + 1001,
   so tile (0,0) = 1001, (9,0) = 1010, (0,1) = 1011. Islands can be assigned to,
   packed into, and rendered across tiles. Pure + stateless helpers here; the 2D
   editor renders the grid and the packer offsets each pack by the tile origin.
   ========================================================================== */

/* view + target state. cols/rows = how many tiles the 2D editor draws; activeTile
   = the tile a Repack / Assign targets. Pure VIEW state -- never in history. */
const UDIM = {
    cols: 10,               // tiles drawn across (matches the *10 numbering row)
    rows: 10,               // tiles drawn up
    activeTile: { tu: 0, tv: 0 },   // Repack / Assign target
};

/* tile (tu,tv) -> UDIM number: 1001 + tu + tv*10, so tile (0,0) = 1001,
   (9,0) = 1010, (0,1) = 1011 (10 columns per row). */
function tileToUdim(tu, tv) { return 1001 + tu + tv * 10; }

/* UDIM number -> tile {tu,tv} (inverse of tileToUdim). */
function udimToTile(num) {
    const n = (num | 0) - 1001;
    return { tu: ((n % 10) + 10) % 10, tv: Math.floor(n / 10) };
}

/* which tile does a UV coordinate land in? floor -> integer tile index. */
function tileOf(u, v) { return { tu: Math.floor(u), tv: Math.floor(v) }; }

if (typeof module !== "undefined") {
    module.exports = { UDIM, tileToUdim, udimToTile, tileOf };
}
