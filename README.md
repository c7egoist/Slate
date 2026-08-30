# SLATE

Procedural residential architecture — a seed-driven generator of real building typologies, not boxes with decals.

Eight architectural languages, each with its own massing, wall construction, openings, orders, roofs and ornament:

- **Georgian** — brick, sash lights, pedimented Tuscan doorcase
- **Victorian** — bay window, wrap porch, steep gable
- **Haussmann** — limestone, iron balconies, zinc mansard
- **Mediterranean** — stucco, terracotta, arched loggia
- **Modernist** — white volumes, ribbon windows, pilotis
- **Tudor** — timber framing, clustered stacks, casements
- **Art Deco** — vertical fins, setbacks, metal windows
- **Brutalist** — béton brut, deep reveals, chunky slabs

Every wall, floor slab, door, column and window is modelled in three dimensions. Facades are piers and spandrels with true reveals. Textures (brick, ashlar, stucco, slate, terracotta, formwork concrete…) are generated procedurally from the seed.

## Run

```bash
npm install
npm run dev
```

## Parameters

| Control | What it does |
| --- | --- |
| Seed | Deterministic design. Same seed + style = same house. |
| Storeys | 1–8 floor plates with style-aware storey heights |
| Front / depth bays | Window rhythm and footprint |
| Window height / muntins | Opening proportion and glazing bars |
| Columns / order | Portico, pilotis, piers — Tuscan through Corinthian |
| Balconies, dormers, roof | Toggle ornament; override roof family |
| Time of day | Sun, sky, and interior lamp glow |

`R` randomizes the seed. The URL hash stores seed, style and massing so a house can be bookmarked.
