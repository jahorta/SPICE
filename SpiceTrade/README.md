# SpiceTrade

`SpiceTrade` is a static library for controlled interchange with external
game-data tools. Its initial compatibility target is the CSV dialect emitted
and consumed by ALX 5.0.0.

The library owns CSV transport only: ordered UTF-8 headers and rows, parsing,
serialization, explicit-file workspaces, diagnostics, and change detection.
It does not own domain interpretation, display models, joins, or binary game
formats.

No ALX data family is admitted by this initial framework. A later whitelist
will contain only gaps for which SPICE intentionally does not plan a native
editor. CSVs representing formats with planned native SPICE editors remain
with those projects. Script-task CSVs are out of scope because SALSA owns that
editing workflow.

ALX's GPLv3 Ruby source may be used as a behavioral reference and external
compatibility oracle. ALX source, templates, and real exported data must not be
copied into this project.
