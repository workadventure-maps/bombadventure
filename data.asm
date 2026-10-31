.include "hdr.asm"

.section ".rodata1" superfree

tiles_til:
.incbin "gfx/tiles.pic"
tiles_tilend:
tiles_pal:
.incbin "gfx/tiles.pal"

flames_til:
.incbin "gfx/flames.pic"
flames_tilend:
flames_pal:
.incbin "gfx/flames.pal"

font_til:
.incbin "gfx/font.pic"
font_tilend:
font_pal:
.incbin "gfx/font.pal"

title_pal:
.incbin "gfx/title.pal"

p1_pal:
.incbin "gfx/p1.pal"
p2_pal:
.incbin "gfx/p2.pal"
p3_pal:
.incbin "gfx/p3.pal"
p4_pal:
.incbin "gfx/p4.pal"

.ends

.section ".rodata2" superfree

p1_til:
.incbin "gfx/p1.pic"
p2_til:
.incbin "gfx/p2.pic"
p3_til:
.incbin "gfx/p3.pic"
p4_til:
.incbin "gfx/p4.pic"

.ends

.section ".rodata3" superfree

sfx_bomb:
.incbin "sfx/bomb.brr"
sfx_bomb_end:
sfx_boom:
.incbin "sfx/boom.brr"
sfx_boom_end:
sfx_power:
.incbin "sfx/power.brr"
sfx_power_end:
sfx_death:
.incbin "sfx/death.brr"
sfx_death_end:
sfx_join:
.incbin "sfx/join.brr"
sfx_join_end:

.ends

.section ".rodata4" superfree

title_til:
.incbin "gfx/title.pic"
title_tilend:
title_map:
.incbin "gfx/title.map"

.ends
