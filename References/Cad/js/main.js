"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   main.js — entry point. Runs LAST, after every module has defined its globals
             and the DOM is in place. Boots the standalone CAD workspace: hydrates
             icons, builds both dock carousels (Browser + Properties by default),
             seeds the history, wires the viewport / feature / history controls,
             and paints the initial browser + properties.
   ════════════════════════════════════════════════════════════════════════════ */

// brand logo (CAD compass glyph, --cad tinted via .th-brand .logo)
const _brand=document.getElementById("brandLogo");
if(_brand) _brand.innerHTML=WS_GLYPH_CAD;

// icons across the static markup
hydrateIcons(document);

// LEFT carousel: Browser ⇄ Tools (default Browser)
cadLeftPane="browser";
renderCadLeftCarousel();
selectCadLeftPane("browser");
buildCadToolbarPane();          // prime the Tools pane so its first reveal is instant

// RIGHT carousel: Properties ⇄ Features ⇄ History (default Properties)
cadRightPane="props";
renderCadRightCarousel();
selectCadRightPane("props");

// data
seedHistory();
renderTabs();
renderTree();
clearProps();

// viewport chrome + feature/history wiring
bindViewport();
bindCadFeatures();
bindHistoryButtons();
if(typeof bindContextMenu==="function") bindContextMenu();

// seed the feature timeline count into the browser footer
if(typeof cadSeedTimeline==="function") cadSeedTimeline();
syncBrowserFooter();

// final icon sweep for anything built after the first pass
hydrateIcons(document);
