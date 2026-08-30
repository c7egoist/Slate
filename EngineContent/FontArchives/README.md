# FontArchives

Runtime typefaces used by Slate. Download them with `Scripts/DownloadFonts.bat` on Windows or
`python3 Scripts/DownloadFonts.py` on Linux and macOS.

The downloader installs open-source families from their upstream repositories:

- Open Sans
- Archivo
- Inter
- JetBrains Mono

Families may provide Regular, Italic, Thin, ExtraLight, Light, Medium, Semibold, Bold, ExtraBold, and
Black faces. Missing faces must fall back to the nearest available face; the loader must not invent a
weight or italic style silently.

The existing EngineContent carry step copies this folder beside the executable. Keep font licenses and
source records beside any redistributed binaries. Font binaries are not generated source and should not
be committed unless redistribution has been approved for each upstream license.
