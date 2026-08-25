"""MANTLE sprite pipeline for Blender 4.1.

Builds rigged characters procedurally, animates them per AnimID
(see src/render/sprites.h) and renders 896x1920 sprite sheets
(7 cols x 15 rows of 128x128 cells = 32 game units at ASSET_SCALE 4).

The look is a smooth "3D figurine" (anti-aliased alpha + dark outline, drawn with
linear filtering), NOT pixel art. The palette-snapping path is legacy and opt-in.

Entry point: mantle_sprites.run.main()
"""
