;---------------------------------------------------------------------------------
; Arena drawing, in assembly because these run for every changed cell and compiled C
; is about 5x slower. Called from C (816-tcc): args on the stack after the 3-byte return
; address (u8 args take 1 byte), A/X/Y 16-bit on entry and exit, data bank = $7E.
;---------------------------------------------------------------------------------
.include "hdr.asm"
.accu 16
.index 16
.16bit
.BASE $80

.DEFINE REDRAWS_PER_FRAME 8
; keep in sync with src/main.c
.DEFINE MAXTIMED 96
.DEFINE FLAME_TIME 30
.DEFINE BURN_TIME 30
.DEFINE C_FLOOR 0
.DEFINE C_WALL 1
.DEFINE C_DESK 2
.DEFINE C_BURN 3
.DEFINE C_BOMB 4
.DEFINE C_FLAME 5
.DEFINE C_PU_BOMB 6
.DEFINE MT_FL_C 6

.SECTION ".cells_text" SUPERFREE

; metatile of each cell type (floor, bomb and flame are handled in draw_cell)
cells_type_mt:
    .db 0, 1, 2, 3, 4, 6, 13, 14, 15
; blast arms: left, right, up, down
blast_delta:
    .db $FF, $01, $F0, $10
blast_mid:
    .db 7, 7, 8, 8          ; MT_FL_H, MT_FL_V
blast_end:
    .db 9, 10, 11, 12       ; MT_FL_L, R, U, D

;---------------------------------------------------------------------------------
; void put_mt(u8 c, u8 mt): write metatile mt into bgmap at cell c, mark its row dirty
put_mt:
    php
    rep #$30
    lda 5,s                 ; c
    and #$00FF
    sta.b tcc__r0
    asl a
    tax
    lda.w cell_bg,x         ; word index of the cell's top-left tile
    asl a
    tay
    lda 6,s                 ; mt
    and #$00FF
    asl a
    tax
    lda.w mt_tile,x
    sta.w bgmap,y
    inc a
    sta.w bgmap+2,y
    clc
    adc #15
    sta.w bgmap+64,y
    inc a
    sta.w bgmap+66,y

    sep #$20                ; row = c >> 4, widen map_lo..map_hi
    lda.b tcc__r0
    lsr a
    lsr a
    lsr a
    lsr a
    cmp.w map_lo
    bcs +
    sta.w map_lo
+   sep #$10                ; 8-bit X for the u8 compare
    ldx.w map_hi
    cpx #$FF
    beq @sethi
    cmp.w map_hi
    bcc @done
@sethi:
    sta.w map_hi
@done:
    plp
    rtl

;---------------------------------------------------------------------------------
; void draw_cell(u8 c): redraw cell c from its type
draw_cell:
    php
    rep #$30
    lda 5,s
    and #$00FF
    tax                     ; x = c
    sep #$20
    lda.w cell,x
    beq @floor
    cmp #5
    beq @flame
    cmp #4
    beq @bomb
    rep #$20
    and #$00FF
    phx
    tax
    sep #$20
    lda.l cells_type_mt,x
    plx
    bra @put
@flame:
    lda.w cpiece,x
    bra @put
@bomb:
    lda.w bomb_frame
    clc
    adc #4
    bra @put
@floor:                     ; shaded floor under walls, desks and burning desks (types 1..3)
    cpx #16
    bcc @plain
    lda.w cell-16,x
    beq @plain
    cmp #4
    bcs @plain
    lda #16
    bra @put
@plain:
    lda #0
@put:
    pha                     ; put_mt(c, mt): push mt, then c
    txa
    pha
    rep #$20
    jsl put_mt
    pla                     ; drop both bytes
    plp
    rtl

;---------------------------------------------------------------------------------
; void redraw(u8 c): queue cell c for drawing (once)
redraw:
    php
    rep #$30
    lda 5,s
    and #$00FF
    tax
    sep #$20
    lda.w queued,x
    bne @done
    lda #1
    sta.w queued,x
    rep #$20
    lda.w rq_tail
    and #$00FF
    tay
    sep #$20
    txa
    sta.w rq,y
    inc.w rq_tail
@done:
    plp
    rtl

;---------------------------------------------------------------------------------
; void flush_redraws(void): draw up to REDRAWS_PER_FRAME queued cells
flush_redraws:
    php
    sep #$20
    lda #REDRAWS_PER_FRAME
    sta.b tcc__r1
@loop:
    lda.w rq_head
    cmp.w rq_tail
    beq @done
    rep #$20
    and #$00FF
    tax
    sep #$20
    lda.w rq,x
    inc.w rq_head
    rep #$20
    and #$00FF
    tax
    sep #$20
    stz.w queued,x
    txa
    pha
    rep #$20
    jsl draw_cell
    sep #$20
    pla
    dec.b tcc__r1
    bne @loop
@done:
    plp
    rtl

;---------------------------------------------------------------------------------
; void set_cell(u8 c, u8 type): change a cell and queue it, plus the cell below (its shadow)
set_cell:
    php
    rep #$30
    lda 5,s
    and #$00FF
    tax
    sep #$20
    lda 6,s
    sta.w cell,x
    txa
    pha
    rep #$20
    jsl redraw
    sep #$20
    pla
    rep #$30
    lda 5,s
    and #$00FF
    cmp #208-16
    bcs @done
    clc
    adc #16
    tax
    sep #$20
    lda.w cell,x
    bne @done
    txa
    pha
    rep #$20
    jsl redraw
    sep #$20
    pla
@done:
    plp
    rtl

;---------------------------------------------------------------------------------
; void start_timer(u8 c, u8 t): (re)start the timer of cell c
start_timer:
    php
    rep #$30
    lda 5,s
    and #$00FF
    tax
    sep #$20
    lda.w ctimer,x
    bne @set
    lda.w ntimed
    cmp #MAXTIMED
    bcs @set
    rep #$20
    and #$00FF
    tay
    sep #$20
    txa
    sta.w timed,y
    inc.w ntimed
@set:
    lda 6,s
    sta.w ctimer,x
    plp
    rtl

;---------------------------------------------------------------------------------
; void flame(u8 c, u8 mt): put a flame on cell c (crossing flames become a center piece)
flame:
    php
    rep #$30
    lda 5,s
    and #$00FF
    tax
    sep #$20
    lda.w cell,x
    cmp #C_FLAME
    bne @own
    lda #MT_FL_C
    bra @piece
@own:
    lda 6,s
@piece:
    sta.w cpiece,x
    lda #C_FLAME
    sta.w cell,x
    txa
    pha
    rep #$20
    jsl redraw
    sep #$20
    pla
    lda #FLAME_TIME         ; start_timer(c, FLAME_TIME)
    pha
    lda 6,s                 ; c, one byte deeper now
    pha
    rep #$20
    jsl start_timer
    pla
    plp
    rtl

;---------------------------------------------------------------------------------
; void blast(u8 c, u8 r): the four arms of an explosion on cell c with range r.
; Arms stop at walls and burning desks, set desks on fire, set off other bombs
; (on the next frame) and burn power-ups. Uses the bl_* globals of main.c.
blast:
    php
    sep #$20
    rep #$10
    lda 5,s
    sta.w bl_c
    lda 6,s
    sta.w bl_r
    stz.w bl_d
@dir:
    lda.w bl_c
    sta.w bl_pos
    lda #1
    sta.w bl_k
@step:
    rep #$20
    lda.w bl_d
    and #$00FF
    tax
    sep #$20
    lda.w bl_pos
    clc
    adc.l blast_delta,x
    sta.w bl_pos
    rep #$20
    and #$00FF
    tax
    sep #$20
    lda.w cell,x
    cmp #C_WALL
    beq @nd
    cmp #C_BURN
    beq @nd
    cmp #C_DESK
    bne @not_desk
    lda #C_BURN             ; set_cell(pos, C_BURN)
    pha
    lda.w bl_pos
    pha
    rep #$20
    jsl set_cell
    pla
    sep #$20
    lda #BURN_TIME          ; start_timer(pos, BURN_TIME)
    pha
    lda.w bl_pos
    pha
    rep #$20
    jsl start_timer
    pla
    sep #$20
@nd:
    jmp @next_dir           ; (the arm is too long for short branches)
@not_desk:
    cmp #C_BOMB
    bne @not_bomb
    rep #$20                ; chain reaction: that bomb goes off next frame
    lda.w nbombs
    and #$00FF
    tax
    sep #$20
    lda.w bl_pos
-   dex
    bmi @nd
    cmp.w bomb_cell,x
    bne -
    pha
    lda #1
    sta.w bomb_time,x
    pla
    bra -
@not_bomb:
    pha                     ; keep the cell type
    rep #$20
    lda.w bl_d
    and #$00FF
    tax
    sep #$20
    lda.w bl_k
    cmp.w bl_r
    beq @tip
    lda.l blast_mid,x
    bra @fl
@tip:
    lda.l blast_end,x
@fl:
    pha                     ; flame(pos, mt)
    lda.w bl_pos
    pha
    rep #$20
    jsl flame
    pla
    sep #$20
    pla
    cmp #C_PU_BOMB          ; a power-up burns and stops the arm
    bcs @next_dir
    lda.w bl_k
    cmp.w bl_r
    bcs @next_dir
    inc a
    sta.w bl_k
    jmp @step
@next_dir:
    lda.w bl_d
    inc a
    sta.w bl_d
    cmp #4
    bcs @end
    jmp @dir
@end:
    plp
    rtl

;---------------------------------------------------------------------------------
; void update_timed(void): tick flame and burning-desk timers. An expired flame leaves
; floor; an expired desk leaves floor or, 3 times in 8, a power-up.
update_timed:
    php
    sep #$20
    rep #$10
    stz.w ut_i
@loop:
    lda.w ut_i
    cmp.w ntimed
    bcc @tick
    plp
    rtl
@tick:
    rep #$20
    and #$00FF
    tay
    sep #$20
    lda.w timed,y
    rep #$20
    and #$00FF
    tax
    sep #$20
    dec.w ctimer,x
    beq @expire
    inc.w ut_i
    bra @loop
@expire:
    phx                     ; timed[i] = timed[--ntimed]
    dec.w ntimed
    rep #$20
    lda.w ntimed
    and #$00FF
    tax
    sep #$20
    lda.w timed,x
    sta.w timed,y
    plx
    lda.w cell,x
    cmp #C_BURN
    bne @floor
    phx
    rep #$20
    jsl rand
    plx
    sep #$20
    lda.b tcc__r0
    and #7
    cmp #3
    bcs @floor
    clc
    adc #C_PU_BOMB
    bra @set
@floor:
    lda #C_FLOOR
@set:
    pha                     ; set_cell(c, type); slot i now holds another timer
    txa
    pha
    rep #$20
    jsl set_cell
    pla
    sep #$20
    jmp @loop

.ENDS
