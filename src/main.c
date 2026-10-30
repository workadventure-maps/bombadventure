/*---------------------------------------------------------------------------------
    Bombadventure: a bomb-maze party game in a WorkAdventure office.
    1 to 4 players (P1 on port 1, P2-P4 on a Multitap in port 2).
---------------------------------------------------------------------------------*/
#include <snes.h>

extern char tiles_til, tiles_tilend, tiles_pal;
extern char font_til, font_tilend, font_pal;
extern char p1_til, p2_til, p3_til, p4_til;
extern char p1_pal, p2_pal, p3_pal, p4_pal;
extern char sfx_bomb, sfx_bomb_end, sfx_boom, sfx_boom_end, sfx_power, sfx_power_end;
extern char sfx_death, sfx_death_end, sfx_join, sfx_join_end;
extern char title_til, title_tilend, title_map, title_pal;

// Note: initialized globals live in ROM with this compiler, so only tables are initialized.

// ---- Layout -------------------------------------------------------------------------------
// Arena: 15x13 cells of 16px, at screen (8,16). Cell index = cy * 16 + cx (column 15 unused).
#define AW 15
#define AH 13
#define NCELLS 208
// VRAM (words): sprites $0000-$1FFF, BG2 tiles $2000 (arena, or the title picture: up to 1024 tiles),
// BG3 font $6000, BG3 text map $7000, BG2 map $7400. BG1 is unused.
// CGRAM: BG3 text palettes 0..4 (4 colors each, colors 0..19), arena palette 2 or title palettes 2..7,
// sprite palettes 8..11.
#define VRAM_BG_TILES 0x2000
#define VRAM_FONT 0x6000
#define VRAM_TEXT_MAP 0x7000
#define VRAM_ARENA_MAP 0x7400
#define ARENA_ATTR 0x0800 // BG palette 2
#define TEXT_ATTR 0x2000  // high priority (on top of everything with BG3_MODE1_PRIORITY_HIGH), palette 0

// cell types
#define C_FLOOR 0
#define C_WALL 1
#define C_DESK 2
#define C_BURN 3
#define C_BOMB 4
#define C_FLAME 5
#define C_PU_BOMB 6
#define C_PU_FIRE 7
#define C_PU_SPEED 8

// metatiles, same order as TILES in tools/mkgfx.py
#define MT_FLOOR 0
#define MT_WALL 1
#define MT_DESK 2
#define MT_BURN 3
#define MT_BOMB0 4
#define MT_BOMB1 5
#define MT_FL_C 6
#define MT_FL_H 7
#define MT_FL_V 8
#define MT_FL_L 9
#define MT_FL_R 10
#define MT_FL_U 11
#define MT_FL_D 12
#define MT_PU_BOMB 13
#define MT_PU_FIRE 14
#define MT_PU_SPEED 15
#define MT_FLOOR_SH 16
#define MT_BLANK 17

#define BOMB_TIME 150
#define FLAME_TIME 30
#define BURN_TIME 30
#define ROUND_SECONDS 120
#define MAXBOMBS 32
#define MAXTIMED 96

// game states
#define ST_TITLE 0
#define ST_PLAY 1
#define ST_ENDING 2
#define ST_RESULT 3

#define BOMB_KEYS (KEY_A | KEY_B | KEY_X | KEY_Y)

// sound effects, by rising priority (one plays per frame)
#define SFX_BOMB 0
#define SFX_BOOM 1
#define SFX_POWER 2
#define SFX_DEATH 3
#define SFX_JOIN 4
#define SFX_NONE 255

// ---- Tables (ROM) ---------------------------------------------------------------------------
const u8 spawn_x[4] = {1, 13, 13, 1};
const u8 spawn_y[4] = {1, 11, 1, 11};
const s8 dir_dcell[4] = {-1, 1, -16, 16};                  // left, right, up, down
const u8 dir_mid[4] = {MT_FL_H, MT_FL_H, MT_FL_V, MT_FL_V};
const u8 dir_end[4] = {MT_FL_L, MT_FL_R, MT_FL_U, MT_FL_D};
const u8 walk_cycle[4] = {0, 1, 2, 1};
// HUD on text row 1, columns 2..29: row 0 can be lost to TV overscan, and the arcade cabinet page
// draws its player badge over the top-left corner and its speaker and help icons over the top-right one.
#define HUD_ROW 1
const u8 hud_x[4] = {2, 8, 19, 25}; // the timer sits in between, at columns 14..17
// text palettes: 0 white, then P1..P4 (BGR555)
const u16 hud_colors[5] = {0x7FFF, 0x7FFF, 0x211F, 0x2388, 0x7E0A};
// title screen: the four Wokas stand in a row (sprite x), labels under them, prompt below
const u8 title_x[4] = {40, 88, 136, 184};
#define TITLE_Y 146
#define TITLE_LABEL_ROW 23
#define TITLE_PROMPT_ROW 25

// ---- State (RAM) ----------------------------------------------------------------------------
u16 bgmap[1024];   // arena BG map
u16 txtmap[1024];  // text BG map
u8 map_lo, map_hi; // arena cell rows waiting for VRAM (lo > hi: none)
u8 txt_lo, txt_hi; // text rows waiting for VRAM
u16 cell_bg[NCELLS]; // bgmap index of each cell's top-left tile
u16 mt_tile[18];     // first BG map entry of each metatile
u16 hudpal[20];
u8 title_shown;     // BG2 holds the title picture, not the arena

u8 cell[NCELLS];
u8 cpiece[NCELLS]; // flame metatile
u8 ctimer[NCELLS];
// Cells waiting to be redrawn (src/cells.asm). Game logic changes cells at once; only a few are
// drawn each frame, so a big blast spreads over a few frames.
u8 rq[256];
u8 rq_head, rq_tail;
u8 queued[NCELLS];
u8 timed[MAXTIMED]; // cells with a running timer (flames, burning desks)
u8 ntimed;

u8 bomb_cell[MAXBOMBS], bomb_time[MAXBOMBS], bomb_owner[MAXBOMBS], bomb_range[MAXBOMBS];
u8 nbombs;
u8 bomb_frame;

u16 px[4], py[4];
u8 inround[4], alive[4], dying[4], ready[4], active[4];
u8 dir[4], frame[4], walk[4], acc[4];
u8 speed[4], maxbombs[4], range[4], bombsout[4];
u8 wins[4];
u8 npads;

u8 state;
u16 state_timer;
u8 seconds, subsec;
u8 seeded;
char buf[16];

// ---- Sound ----------------------------------------------------------------------------------
brrsamples sfx_table[5];
u8 sfx_next;

void sfx(u8 id)
{
    if (sfx_next == SFX_NONE || id > sfx_next)
        sfx_next = id;
}

void init_sound(void)
{
    spcBoot();
    spcAllocateSoundRegion(12); // 3KB of SPC RAM, enough for the longest sample
    spcSetSoundDataEntry(15, 8, 2, &sfx_bomb_end - &sfx_bomb, &sfx_bomb, &sfx_table[0]);
    spcSetSoundDataEntry(15, 8, 2, &sfx_boom_end - &sfx_boom, &sfx_boom, &sfx_table[1]);
    spcSetSoundDataEntry(15, 8, 2, &sfx_power_end - &sfx_power, &sfx_power, &sfx_table[2]);
    spcSetSoundDataEntry(15, 8, 2, &sfx_death_end - &sfx_death, &sfx_death, &sfx_table[3]);
    spcSetSoundDataEntry(15, 8, 2, &sfx_join_end - &sfx_join, &sfx_join, &sfx_table[4]);
    spcSetSoundTableEntry(&sfx_table[0]);
    sfx_next = SFX_NONE;
}

extern u8 exploded;

// Starting a sound costs about 40 scanlines, so it waits for a frame without an explosion.
void update_sound(void)
{
    if (sfx_next != SFX_NONE && !exploded)
    {
        spcPlaySound(sfx_next);
        sfx_next = SFX_NONE;
    }
    spcProcess();
}

// ---- Text -----------------------------------------------------------------------------------
void mark_txt(u8 y)
{
    if (y < txt_lo)
        txt_lo = y;
    if (y > txt_hi || txt_hi == 255)
        txt_hi = y;
}

void text(u8 x, u8 y, u8 pal, char *s)
{
    u16 i = (y << 5) + x;
    u16 attr = TEXT_ATTR | (pal << 10);
    while (*s)
    {
        txtmap[i++] = (*s++ - 32) | attr;
    }
    mark_txt(y);
}

void clear_row(u8 y)
{
    u16 i = y << 5;
    u8 n;
    for (n = 0; n < 32; n++)
        txtmap[i + n] = 0;
    mark_txt(y);
}

void center(u8 y, u8 pal, char *s)
{
    u8 n = 0;
    while (s[n])
        n++;
    clear_row(y);
    text(16 - (n >> 1), y, pal, s);
}

void two_digits(char *d, u8 v)
{
    d[0] = '0' + v / 10;
    d[1] = '0' + v % 10;
}

void draw_hud(void)
{
    u8 p;
    if (state == ST_TITLE)
    {
        for (p = 0; p < 4; p++)
        {
            buf[0] = ready[p] ? 'O' : 'P';
            buf[1] = ready[p] ? 'K' : '1' + p;
            buf[2] = 0;
            text((title_x[p] >> 3) + 1, TITLE_LABEL_ROW, 1 + p, buf);
        }
        return;
    }
    for (p = 0; p < 4; p++)
    {
        buf[0] = 'P';
        buf[1] = '1' + p;
        buf[2] = ' ';
        if (inround[p])
            two_digits(buf + 3, wins[p]);
        else if (ready[p])
        {
            buf[3] = 'O';
            buf[4] = 'K';
        }
        else
        {
            buf[3] = '-';
            buf[4] = '-';
        }
        buf[5] = 0;
        text(hud_x[p], HUD_ROW, 1 + p, buf);
    }
}

void draw_timer(void)
{
    buf[0] = '0' + seconds / 60;
    buf[1] = ':';
    two_digits(buf + 2, seconds % 60);
    buf[4] = 0;
    text(14, HUD_ROW, 0, buf);
}

// ---- Arena drawing ----------------------------------------------------------------------------
// in src/cells.asm
void put_mt(u8 c, u8 mt);
void draw_cell(u8 c);
void redraw(u8 c);
void flush_redraws(void);
void set_cell(u8 c, u8 type);
void start_timer(u8 c, u8 t);
void flame(u8 c, u8 mt);
void blast(u8 c, u8 r);
void update_timed(void);
u8 bl_c, bl_r, bl_d, bl_k, bl_pos, ut_i; // scratch for blast() and update_timed()

u8 near_spawn(u8 cx, u8 cy)
{
    return (cx <= 2 || cx >= AW - 3) && (cy <= 2 || cy >= AH - 3) &&
           !((cx == 2 || cx == AW - 3) && (cy == 2 || cy == AH - 3));
}

void build_arena(void)
{
    u8 cx, cy, c;
    for (c = 0; c < NCELLS; c++)
    {
        ctimer[c] = 0;
        queued[c] = 0;
    }
    rq_head = rq_tail = 0;
    ntimed = 0;
    nbombs = 0;
    for (cy = 0; cy < AH; cy++)
    {
        for (cx = 0; cx < 16; cx++)
        {
            c = (cy << 4) + cx;
            if (cx >= AW || cx == 0 || cy == 0 || cx == AW - 1 || cy == AH - 1 || ((cx & 1) == 0 && (cy & 1) == 0))
                cell[c] = C_WALL;
            else if (!near_spawn(cx, cy) && (rand() & 3) != 0)
                cell[c] = C_DESK;
            else
                cell[c] = C_FLOOR;
        }
    }
    for (cy = 0; cy < AH; cy++)
        for (cx = 0; cx < AW; cx++)
            draw_cell((cy << 4) + cx);
}

// ---- Bombs and flames -----------------------------------------------------------------------
void explode(u8 b)
{
    u8 c = bomb_cell[b], r = bomb_range[b];
    bombsout[bomb_owner[b]]--;
    nbombs--;
    bomb_cell[b] = bomb_cell[nbombs];
    bomb_time[b] = bomb_time[nbombs];
    bomb_owner[b] = bomb_owner[nbombs];
    bomb_range[b] = bomb_range[nbombs];
    flame(c, MT_FL_C);
    blast(c, r);
    sfx(SFX_BOOM);
}

// At most one bomb explodes per frame (an explosion is costly); others wait at 1.
u8 exploded;

void update_bombs(void)
{
    u8 i = 0;
    while (i < nbombs)
    {
        if (bomb_time[i] > 1)
        {
            bomb_time[i]--;
            i++;
        }
        else if (!exploded)
        {
            explode(i); // moves the last bomb into slot i, so do not advance
            exploded = 1;
        }
        else
            i++;
    }
    if ((snes_vblank_count & 15) == 0)
    {
        bomb_frame ^= 1;
        for (i = 0; i < nbombs; i++)
            redraw(bomb_cell[i]);
    }
}

// ---- Players ----------------------------------------------------------------------------------
u8 open(u8 c)
{
    u8 t = cell[c];
    return t == C_FLOOR || t == C_FLAME || t >= C_PU_BOMB;
}

u8 center_cell(u8 p)
{
    return (((py[p] + 8) >> 4) << 4) + ((px[p] + 8) >> 4);
}

// move one pixel; d = 0 left, 1 right, 2 up, 3 down. Returns 1 if moved.
u8 step(u8 p, u8 d)
{
    u16 x = px[p], y = py[p];
    u8 c = center_cell(p);
    u16 ax = (c & 15) << 4, ay = (c >> 4) << 4;
    u8 target = c + dir_dcell[d];
    if (d < 2)
    {
        if (y != ay)
        {
            // off the row: slide toward it when the way ahead is open (corner assist)
            if (!open(target))
                return 0;
            py[p] = y < ay ? y + 1 : y - 1;
            return 1;
        }
        if (x == ax && !open(target))
            return 0;
        px[p] = d ? x + 1 : x - 1;
    }
    else
    {
        if (x != ax)
        {
            if (!open(target))
                return 0;
            px[p] = x < ax ? x + 1 : x - 1;
            return 1;
        }
        if (y == ay && !open(target))
            return 0;
        py[p] = d == 3 ? y + 1 : y - 1;
    }
    return 1;
}

void update_player(u8 p)
{
    u16 keys = padsCurrent(p);
    u8 hd = 255, vd = 255, moved = 0, n, c, t;

    if (keys)
        active[p] = 1;
    if (keys & KEY_LEFT)
        hd = 0;
    else if (keys & KEY_RIGHT)
        hd = 1;
    if (keys & KEY_UP)
        vd = 2;
    else if (keys & KEY_DOWN)
        vd = 3;

    acc[p] += speed[p];
    n = acc[p] >> 2;
    acc[p] &= 3;
    while (n--)
    {
        if (hd != 255 && step(p, hd))
            moved = 1;
        else if (vd != 255 && step(p, vd))
            moved = 1;
    }
    if (hd != 255)
        dir[p] = hd == 0 ? 1 : 2;
    else if (vd != 255)
        dir[p] = vd == 2 ? 3 : 0;
    if (moved)
        walk[p]++;
    frame[p] = dir[p] * 3 + (moved ? walk_cycle[(walk[p] >> 3) & 3] : 1);

    c = center_cell(p);
    t = cell[c];
    if (t == C_FLAME)
    {
        alive[p] = 0;
        dying[p] = 60;
        sfx(SFX_DEATH);
        return;
    }
    if (t >= C_PU_BOMB)
    {
        if (t == C_PU_BOMB && maxbombs[p] < 8)
            maxbombs[p]++;
        else if (t == C_PU_FIRE && range[p] < 8)
            range[p]++;
        else if (t == C_PU_SPEED && speed[p] < 8)
            speed[p]++;
        set_cell(c, C_FLOOR);
        sfx(SFX_POWER);
    }
    if ((padsDown(p) & BOMB_KEYS) && t == C_FLOOR && bombsout[p] < maxbombs[p] && nbombs < MAXBOMBS)
    {
        bomb_cell[nbombs] = c;
        bomb_time[nbombs] = BOMB_TIME;
        bomb_owner[nbombs] = p;
        bomb_range[nbombs] = range[p];
        nbombs++;
        bombsout[p]++;
        set_cell(c, C_BOMB);
        sfx(SFX_BOMB);
    }
}

// ---- Sprites ----------------------------------------------------------------------------------
u8 last_frame[4];

// All four sprites are always drawn, in the same order, so each keeps its VRAM slot; hidden ones
// sit below the screen. Each frame change uploads 512 bytes, so at most 2 per frame (VBlank budget).
void draw_sprites(void)
{
    u8 p, uploads = 0;
    t_sprites *s;
    u8 f;
    for (p = 0; p < 4; p++)
    {
        s = &oambuffer[p];
        if (state == ST_TITLE)
        {
            // full-size Wokas; those who joined walk on the spot
            s->oamx = title_x[p];
            s->oamy = TITLE_Y;
            f = ready[p] ? 12 + walk_cycle[((snes_vblank_count >> 3) + p) & 3] : 13;
        }
        else
        {
            s->oamx = px[p];
            s->oamy = (alive[p] || (dying[p] & 4)) ? py[p] : 224;
            f = frame[p];
        }
        if (f != last_frame[p] && uploads < 2)
        {
            s->oamframeid = f;
            s->oamrefresh = 1;
            last_frame[p] = f;
            uploads++;
        }
        oamDynamic32Draw(p);
    }
    oamInitDynamicSpriteEndFrame();
}

// ---- Rounds -----------------------------------------------------------------------------------
u8 count(u8 *a)
{
    return a[0] + a[1] + a[2] + a[3];
}

void clear_messages(void)
{
    u8 y;
    for (y = 10; y < 16; y++)
        clear_row(y);
    clear_row(TITLE_LABEL_ROW);
    clear_row(TITLE_PROMPT_ROW);
}

// The title picture and the arena share BG2's tiles, palettes and map, so switching reloads them
// with the screen off (the arena update budget in prepare_vblank is far too small for this).
void show_title(void)
{
    setScreenOff();
    dmaCopyVram(&title_til, VRAM_BG_TILES, &title_tilend - &title_til);
    dmaCopyVram(&title_map, VRAM_ARENA_MAP, 2048);
    dmaCopyCGram(&title_pal, 32, 6 * 32);
    map_lo = map_hi = 255;
    title_shown = 1;
    setScreenOn();
}

void show_arena(void)
{
    setScreenOff();
    dmaCopyVram(&tiles_til, VRAM_BG_TILES, &tiles_tilend - &tiles_til);
    dmaCopyCGram(&tiles_pal, 32, 32);
    build_arena();
    dmaCopyVram((u8 *)bgmap, VRAM_ARENA_MAP, 2048);
    map_lo = map_hi = 255;
    title_shown = 0;
    setScreenOn();
}

void new_round(void)
{
    u8 p;
    if (title_shown)
        show_arena();
    else
        build_arena();
    for (p = 0; p < 4; p++)
    {
        inround[p] = ready[p];
        alive[p] = ready[p];
        ready[p] = 0;
        active[p] = 0;
        dying[p] = 0;
        px[p] = spawn_x[p] << 4;
        py[p] = spawn_y[p] << 4;
        dir[p] = 0;
        frame[p] = 1;
        acc[p] = 0;
        speed[p] = 4;
        maxbombs[p] = 1;
        range[p] = 2;
        bombsout[p] = 0;
    }
    seconds = ROUND_SECONDS;
    subsec = 0;
    state = ST_PLAY;
    clear_messages();
    draw_hud();
    draw_timer();
}

void to_title(void)
{
    u8 p;
    for (p = 0; p < 4; p++)
    {
        inround[p] = 0;
        alive[p] = 0;
        ready[p] = 0;
        wins[p] = 0;
    }
    state = ST_TITLE;
    state_timer = 0;
    show_title();
    clear_messages();
    clear_row(HUD_ROW);
    draw_hud();
}

void show_result(void)
{
    u8 p, winner = 255, n = count(inround);
    for (p = 0; p < 4; p++)
        if (alive[p])
            winner = p;
    clear_messages();
    if (n > 1 && count(alive) == 1)
    {
        if (wins[winner] < 99)
            wins[winner]++;
        buf[0] = 'P';
        buf[1] = '1' + winner;
        buf[2] = 0;
        center(12, 1 + winner, buf);
        center(13, 0, "WINS THE ROUND!");
    }
    else if (n == 1 && count(alive) == 1)
        center(12, 0, seconds ? "NEW CHALLENGER!" : "TIME UP");
    else if (n == 1)
        center(12, 0, "OUCH!");
    else
        center(12, 0, seconds ? "DRAW" : "TIME UP - DRAW");
    // players who played and pressed something stay in; newcomers join
    for (p = 0; p < 4; p++)
    {
        if (inround[p] && active[p])
            ready[p] = 1;
        alive[p] = 0;
    }
    state = ST_RESULT;
    state_timer = 180;
}

// Start presses outside the player's own round: join the next round
void check_joins(void)
{
    u8 p, joined = 0;
    for (p = 0; p < npads; p++)
    {
        if ((padsDown(p) & KEY_START) && !ready[p] && !(state == ST_PLAY && inround[p]))
        {
            if (!seeded)
            {
                srand(snes_vblank_count);
                seeded = 1;
            }
            ready[p] = 1;
            joined = 1;
            sfx(SFX_JOIN);
        }
    }
    if (joined)
        draw_hud();
}

void update_game(void)
{
    u8 p, n;
    exploded = 0;
    check_joins();
    switch (state)
    {
    case ST_TITLE:
        if (count(ready))
        {
            if (state_timer == 0)
                state_timer = 180;
            if (--state_timer == 0)
            {
                new_round();
                return;
            }
            if (state_timer % 60 == 59)
            {
                buf[0] = '0' + (state_timer + 59) / 60;
                buf[1] = 0;
                center(TITLE_PROMPT_ROW, 0, "GET READY");
                text(22, TITLE_PROMPT_ROW, 0, buf); // "GET READY" is centered on columns 12..20
            }
        }
        else if ((snes_vblank_count & 31) == 0)
            center(TITLE_PROMPT_ROW, 0, (snes_vblank_count & 32) ? "" : "PRESS START");
        break;

    case ST_PLAY:
    case ST_ENDING:
        update_bombs();
        update_timed();
        flush_redraws();
        for (p = 0; p < 4; p++)
        {
            if (alive[p])
                update_player(p);
            else if (dying[p])
                dying[p]--;
        }
        if (++subsec == 60)
        {
            subsec = 0;
            if (seconds)
                seconds--;
            draw_timer();
        }
        if (state == ST_PLAY)
        {
            n = count(inround);
            // a lone player's round ends as soon as someone wants to join
            if ((n > 1 && count(alive) <= 1) || (n == 1 && (count(alive) == 0 || count(ready))) || seconds == 0)
            {
                state = ST_ENDING;
                state_timer = n == 1 && count(alive) ? 1 : 60;
            }
        }
        else if (--state_timer == 0)
            show_result();
        break;

    case ST_RESULT:
        if (--state_timer == 0)
        {
            if (count(ready))
                new_round();
            else
                to_title();
        }
        break;
    }
}

// ---- VBlank transfers -------------------------------------------------------------------------
// One DMA per layer, covering the dirty rows. Compiled C is slow, so everything is worked out in
// prepare_vblank() during the frame and vblank_work() only starts the DMAs. Budget: the arena sends
// at most 6 cell rows per frame (a new arena takes 3 frames), text at most 8 rows, and a big arena
// update pushes text to the next frame. tests/smoke.py checks VRAM matches RAM and VBlank timing.
u8 *map_src, *txt_src;
u16 map_vaddr, map_len, txt_vaddr, txt_len;

void prepare_vblank(void)
{
    u8 lo, hi;
    map_len = 0;
    txt_len = 0;
    if (map_hi != 255)
    {
        lo = map_lo;
        hi = map_hi;
        if (hi > lo + 5)
            hi = lo + 5;
        map_src = (u8 *)&bgmap[cell_bg[lo << 4] - 1];
        map_vaddr = VRAM_ARENA_MAP + cell_bg[lo << 4] - 1;
        map_len = (hi - lo + 1) << 7;
        if (hi == map_hi)
        {
            map_lo = 255;
            map_hi = 255;
        }
        else
        {
            map_lo = hi + 1;
            return; // more arena rows next frame, text waits
        }
        if (map_len > 3 * 128)
            return;
    }
    if (txt_hi != 255)
    {
        lo = txt_lo;
        hi = txt_hi;
        if (hi > lo + 7)
            hi = lo + 7;
        txt_src = (u8 *)&txtmap[lo << 5];
        txt_vaddr = VRAM_TEXT_MAP + (lo << 5);
        txt_len = (hi - lo + 1) << 6;
        if (hi == txt_hi)
        {
            txt_lo = 255;
            txt_hi = 255;
        }
        else
            txt_lo = hi + 1;
    }
}

void vblank_work(void)
{
    oamVramQueueUpdate();
    if (map_len)
        dmaCopyVram(map_src, map_vaddr, map_len);
    if (txt_len)
        dmaCopyVram(txt_src, txt_vaddr, txt_len);
}

// Scanline where this frame's VBlank work ended (VBlank is lines 225..261). vbl_late counts
// frames where it ran into the visible picture, where VRAM writes are lost. Read by the tests.
// work_end is the scanline where the frame's game logic was done (the tests use it for CPU load).
u16 vbl_end;
u16 vbl_late;
u16 work_end;

u16 scanline(void)
{
    u8 lo;
    lo = *(vuint8 *)0x2137; // latch the PPU counters
    lo = *(vuint8 *)0x213F; // reset the read flip-flop
    lo = *(vuint8 *)0x213D;
    return lo | ((*(vuint8 *)0x213D & 1) << 8);
}

void measure_vblank(void)
{
    vbl_end = scanline();
    if (vbl_end < 225 && vbl_late < 65535)
        vbl_late++;
}

// ---- Main -------------------------------------------------------------------------------------
int main(void)
{
    u16 i;
    u8 p;

    init_sound();

    // text on BG3 (2bpp), arena or title picture on BG2, players are 32x32 sprites
    bgInitTileSet(2, &font_til, &font_pal, 0, (&font_tilend - &font_til), 4 * 2, BG_4COLORS, VRAM_FONT);
    bgSetMapPtr(2, VRAM_TEXT_MAP, SC_32x32);
    bgSetGfxPtr(1, VRAM_BG_TILES);
    bgSetMapPtr(1, VRAM_ARENA_MAP, SC_32x32);

    // text palettes 0..4: white, then each player's color; color 0 is also the backdrop
    for (i = 0; i < 20; i++)
        hudpal[i] = 0;
    for (p = 0; p < 5; p++)
    {
        hudpal[p * 4 + 1] = hud_colors[p];
        hudpal[p * 4 + 2] = 0x1043;
    }
    hudpal[0] = 0x2108;
    dmaCopyCGram((u8 *)hudpal, 0, 20 * 2);

    oamInitDynamicSprite(0x0000, 0x1000, 0, 0, OBJ_SIZE16_L32);
    dmaCopyCGram((u8 *)&p1_pal, 128, 32);
    dmaCopyCGram((u8 *)&p2_pal, 144, 32);
    dmaCopyCGram((u8 *)&p3_pal, 160, 32);
    dmaCopyCGram((u8 *)&p4_pal, 176, 32);
    for (p = 0; p < 4; p++)
    {
        oambuffer[p].oamattribute = OBJ_PRIO(2) | OBJ_PAL(p) | OBJ_SIZEL;
        oambuffer[p].oamrefresh = 1;
    }
    oambuffer[0].oamgraphics = &p1_til;
    oambuffer[1].oamgraphics = &p2_til;
    oambuffer[2].oamgraphics = &p3_til;
    oambuffer[3].oamgraphics = &p4_til;

    for (i = 0; i < 18; i++)
        mt_tile[i] = ((i >> 3) << 5) + ((i & 7) << 1) + ARENA_ATTR;
    for (i = 0; i < 1024; i++)
    {
        bgmap[i] = mt_tile[MT_BLANK]; // around the arena: empty, the backdrop shows
        txtmap[i] = 0;
    }
    for (i = 0; i < NCELLS; i++)
        cell_bg[i] = (((i >> 4) + 1) << 6) + 1 + ((i & 15) << 1);
    map_lo = txt_lo = 255;
    map_hi = txt_hi = 255;
    for (p = 0; p < 4; p++)
    {
        wins[p] = 0;
        last_frame[p] = 255;
    }
    seeded = 0;
    vbl_end = 0;
    vbl_late = 0;
    bomb_frame = 0;

    detectMPlay5();
    npads = snes_mplay5 ? 4 : 2;

    dmaCopyVram((u8 *)txtmap, VRAM_TEXT_MAP, 2048);
    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetDisable(0);
    title_shown = 0;
    to_title(); // turns the screen on

    while (1)
    {
        update_game();
        draw_sprites();
        update_sound();
        prepare_vblank();
        work_end = scanline();
        WaitForVBlank();
        vblank_work();
        measure_vblank();
    }
    return 0;
}
