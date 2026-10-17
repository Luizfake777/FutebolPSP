/* ======================================================================
 * FUTEBOL PSP - jogo completo em um unico arquivo (src/main.c)
 *
 * Menu, selecao (Brasil, Argentina, Franca, Espanha), uniformes, campo 3D,
 * jogadores 3D animados, bola com fisica simples, IA, goleiros, placar,
 * cronometro e controles PSP. Somente PSP SDK: pspkernel, pspdisplay,
 * pspgu, pspgum, pspctrl, math.h e string.h.
 *
 * Controles em partida:
 *   Analogico / direcional ... mover
 *   Quadrado ou R ............ correr (gasta folego)
 *   X (segurar e soltar) ..... chute (forca pelo tempo segurado)
 *   O ........................ passe (sem bola: carrinho/desarme)
 *   Triangulo ................ passe alto (sem bola: trocar jogador)
 *   L ........................ trocar jogador
 *   START .................... pausa
 * ====================================================================== */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("FutebolPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/* ---------------------------------------------------------------------
 * Constantes
 * ------------------------------------------------------------------- */
#define SW 480
#define BW 512
#define SH 272

#define PI          3.14159265f
#define DT          (1.0f / 60.0f)

#define FIELD_X     9.0f
#define FIELD_Z     17.0f
#define GOAL_HW     1.8f
#define GOAL_H      1.6f
#define GOAL_DEPTH  1.8f
#define BALL_R      0.22f
#define GRAV        12.0f
#define MATCH_TIME  180.0f
#define INTRO_T     5.0f
#define GOAL_T      4.2f

#define BALL_VERTS  360

#define RGB(r, g, b)     (0xFF000000u | ((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(r))
#define RGBA(r, g, b, a) (((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(r))

#define VTX_3D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define VTX_2D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

/* ---------------------------------------------------------------------
 * Tipos
 * ------------------------------------------------------------------- */
typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    unsigned int shirt, shirt2, shorts, socks;
    int stripes;            /* 0 liso, 1 listras verticais, 2 faixa diagonal/horizontal */
    unsigned int trim;      /* cor da gola, punhos e detalhes */
    unsigned int boots;
} Kit;

typedef struct {
    const char *name;
    const char *abbr;
    Kit kit;
    Kit alt;                /* reserva, usado se as cores forem parecidas */
} Team;

typedef struct {
    float x, z, vx, vz;
    float dvx, dvz;
    float face, anim, speed;
    float cool, diveT, diveDir, slideT, slx, slz;
    float homeX, homeZ, think, stamina;
    float aimX, aimZ;
    float dribVx, dribVz, dribT, dribSide, spinA, kickT, celebT, jump, oHold, diveH;
    float introX, introZ, bob;
    int team, role, skin, number, celebType, hair;
} Player;

typedef struct {
    float x, y, z, vx, vy, vz;
    int owner, lastTeam, lastTouch, passTarget;
    float passT, spinX, spinZ;
} Ball;

enum {
    ST_MENU, ST_CONTROLS, ST_SEL_HOME, ST_SEL_AWAY,
    ST_INTRO, ST_PLAY, ST_GOAL, ST_PAUSE, ST_END
};

static void doDribble(int idx, float side);

/* ---------------------------------------------------------------------
 * Dados estaticos
 * ------------------------------------------------------------------- */
static const Team teams[4] = {
    { "BRASIL", "BRA",
      { RGB(250, 214, 0),   RGB(250, 214, 0),   RGB(0, 70, 170),    RGB(240, 240, 240), 0, RGB(0, 140, 60),    RGB(20, 20, 20) },
      { RGB(0, 70, 170),    RGB(0, 70, 170),    RGB(245, 245, 245), RGB(0, 70, 170),    0, RGB(250, 214, 0),   RGB(20, 20, 20) } },
    { "ARGENTINA", "ARG",
      { RGB(130, 195, 240), RGB(250, 250, 250), RGB(20, 20, 20),    RGB(20, 20, 20),    1, RGB(20, 20, 20),    RGB(20, 20, 20) },
      { RGB(30, 30, 90),    RGB(30, 30, 90),    RGB(30, 30, 90),    RGB(30, 30, 90),    0, RGB(130, 195, 240), RGB(240, 240, 240) } },
    { "FRANCA", "FRA",
      { RGB(25, 55, 155),   RGB(25, 55, 155),   RGB(245, 245, 245), RGB(200, 30, 45),   0, RGB(240, 240, 240), RGB(20, 20, 20) },
      { RGB(245, 245, 245), RGB(245, 245, 245), RGB(25, 55, 155),   RGB(245, 245, 245), 0, RGB(200, 30, 45),   RGB(20, 20, 20) } },
    { "ESPANHA", "ESP",
      { RGB(205, 25, 35),   RGB(205, 25, 35),   RGB(20, 40, 125),   RGB(20, 40, 125),   2, RGB(250, 205, 40),  RGB(20, 20, 20) },
      { RGB(250, 205, 40),  RGB(250, 205, 40),  RGB(205, 25, 35),   RGB(250, 205, 40),  0, RGB(205, 25, 35),   RGB(20, 20, 20) } }
};

static const Kit gkKits[2] = {
    { RGB(30, 170, 60),  RGB(30, 170, 60),  RGB(20, 20, 20), RGB(20, 20, 20), 2, RGB(250, 250, 250), RGB(240, 240, 240) },
    { RGB(245, 130, 20), RGB(245, 130, 20), RGB(20, 20, 20), RGB(20, 20, 20), 2, RGB(250, 250, 250), RGB(240, 240, 240) }
};

static const unsigned int skinTones[4] = { RGB(240, 200, 160), RGB(200, 150, 110), RGB(140, 95, 65), RGB(95, 62, 42) };
static const unsigned int hairTones[4] = { RGB(30, 25, 20), RGB(70, 45, 25), RGB(220, 190, 90), RGB(20, 20, 20) };

/* Fonte bitmap 5x7: 0-9, A-Z, : - . ! < > / espaco */
static const char FONT_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-.!<>/ ";
static const unsigned char FONT[44][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00}, /* : */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* ! */
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, /* < */
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, /* > */
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, /* / */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}  /* espaco */
};

/* ---------------------------------------------------------------------
 * Estado global
 * ------------------------------------------------------------------- */
static unsigned int __attribute__((aligned(16))) list[262144];
static Vertex ballMesh[BALL_VERTS];

static Player pl[8];
static Ball ball;
static const Kit *kits[8];

static int gState = ST_MENU;
static int running = 1;
static int menuSel = 0;
static int selHome = 0, selAway = 1;
static int teamOf[2] = { 0, 1 };
static int ctrl = 2;
static int lastOwner = -2;
static int celebIdx = -1;
static float celebElapsed = 0.0f;
static int score[2];
static int scorerTeam = 0;
static float timeLeft = MATCH_TIME;
static float stateT = 0.0f;
static float gkHold = 0.0f;
static float switchT = 0.0f;
static float charge = 0.0f;
static float camX = 0.0f, camZ = 0.0f;
static float menuAngle = 0.0f;
static int frameCount = 0;
static unsigned int gPressed = 0, gReleased = 0;
static unsigned int rngState = 2463534242u;

/* ---------------------------------------------------------------------
 * Callbacks de saida (botao HOME / PPSSPP)
 * ------------------------------------------------------------------- */
static int exitCallback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callbackThread(SceSize args, void *argp) {
    int cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

/* ---------------------------------------------------------------------
 * Utilidades
 * ------------------------------------------------------------------- */
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float signf(float v) { return v < 0.0f ? -1.0f : 1.0f; }

static float wrapAng(float a) {
    while (a > PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

static float distf(float ax, float az, float bx, float bz) {
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

static float rnd01(void) {
    rngState = rngState * 1664525u + 1013904223u;
    return (float)((rngState >> 8) & 0xFFFFu) / 65535.0f;
}

static float rndRange(float a, float b) { return a + (b - a) * rnd01(); }

static unsigned int shade(unsigned int c, float f) {
    unsigned int r = (unsigned int)((float)(c & 0xFFu) * f);
    unsigned int g = (unsigned int)((float)((c >> 8) & 0xFFu) * f);
    unsigned int b = (unsigned int)((float)((c >> 16) & 0xFFu) * f);
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

static void appendInt(char *b, int v) {
    char t[12];
    int n = 0, l;
    if (v < 0) v = 0;
    if (v == 0) t[n++] = '0';
    while (v > 0 && n < 11) { t[n++] = (char)('0' + v % 10); v /= 10; }
    l = (int)strlen(b);
    while (n > 0) b[l++] = t[--n];
    b[l] = 0;
}

static void fmtTime(char *b, int s) {
    int m, ss;
    if (s < 0) s = 0;
    m = s / 60; ss = s % 60;
    b[0] = (char)('0' + (m / 10) % 10);
    b[1] = (char)('0' + m % 10);
    b[2] = ':';
    b[3] = (char)('0' + ss / 10);
    b[4] = (char)('0' + ss % 10);
    b[5] = 0;
}

/* ---------------------------------------------------------------------
 * Inicializacao do GU (igual ao codigo original + clip/shade)
 * ------------------------------------------------------------------- */
static void init3D(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BW);
    sceGuDispBuffer(SW, SH, (void *)0x88000, BW);
    sceGuDepthBuffer((void *)0x110000, BW);
    sceGuOffset(2048 - SW / 2, 2048 - SH / 2);
    sceGuViewport(2048, 2048, SW, SH);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthRange(65535, 0);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuDisplay(GU_TRUE);
}

static void initBallMesh(void) {
    int i, j, n = 0;
    for (i = 0; i < 6; i++) {
        float la0 = -PI * 0.5f + PI * (float)i / 6.0f;
        float la1 = -PI * 0.5f + PI * (float)(i + 1) / 6.0f;
        for (j = 0; j < 10; j++) {
            float lo0 = 2.0f * PI * (float)j / 10.0f;
            float lo1 = 2.0f * PI * (float)(j + 1) / 10.0f;
            unsigned int c = ((i + j) & 1) ? RGB(30, 30, 30) : RGB(245, 245, 245);
            float p[4][3];
            static const int idx[6] = { 0, 1, 3, 0, 3, 2 };
            int k;
            p[0][0] = BALL_R * cosf(la0) * cosf(lo0); p[0][1] = BALL_R * sinf(la0); p[0][2] = BALL_R * cosf(la0) * sinf(lo0);
            p[1][0] = BALL_R * cosf(la0) * cosf(lo1); p[1][1] = BALL_R * sinf(la0); p[1][2] = BALL_R * cosf(la0) * sinf(lo1);
            p[2][0] = BALL_R * cosf(la1) * cosf(lo0); p[2][1] = BALL_R * sinf(la1); p[2][2] = BALL_R * cosf(la1) * sinf(lo0);
            p[3][0] = BALL_R * cosf(la1) * cosf(lo1); p[3][1] = BALL_R * sinf(la1); p[3][2] = BALL_R * cosf(la1) * sinf(lo1);
            for (k = 0; k < 6; k++) {
                ballMesh[n].color = c;
                ballMesh[n].x = p[idx[k]][0];
                ballMesh[n].y = p[idx[k]][1];
                ballMesh[n].z = p[idx[k]][2];
                n++;
            }
        }
    }
}

/* ---------------------------------------------------------------------
 * Desenho 2D (HUD, menus, texto)
 * ------------------------------------------------------------------- */
static void begin2D(void) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

static void end2D(void) {
    sceGuDisable(GU_BLEND);
}

static void rect2d(float x, float y, float w, float h, unsigned int c) {
    Vertex *v = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    v[0].color = c; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = c; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, VTX_2D, 2, 0, v);
}

static void rectGrad(float x, float y, float w, float h, unsigned int top, unsigned int bot) {
    Vertex *v = (Vertex *)sceGuGetMemory(6 * sizeof(Vertex));
    v[0].color = top; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = top; v[1].x = x + w; v[1].y = y;     v[1].z = 0.0f;
    v[2].color = bot; v[2].x = x + w; v[2].y = y + h; v[2].z = 0.0f;
    v[3].color = top; v[3].x = x;     v[3].y = y;     v[3].z = 0.0f;
    v[4].color = bot; v[4].x = x + w; v[4].y = y + h; v[4].z = 0.0f;
    v[5].color = bot; v[5].x = x;     v[5].y = y + h; v[5].z = 0.0f;
    sceGuDrawArray(GU_TRIANGLES, VTX_2D, 6, 0, v);
}

static int glyphIndex(char c) {
    int i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    for (i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return i;
    return -1;
}

static float textW(const char *s, float sc) { return (float)strlen(s) * 6.0f * sc; }

static void drawText(float x, float y, float sc, unsigned int col, const char *s) {
    while (*s) {
        int gi = glyphIndex(*s);
        if (gi >= 0) {
            int row;
            for (row = 0; row < 7; row++) {
                unsigned char bits = FONT[gi][row];
                int c = 0;
                while (c < 5) {
                    if (bits & (0x10 >> c)) {
                        int st = c;
                        while (c < 5 && (bits & (0x10 >> c))) c++;
                        rect2d(x + (float)st * sc, y + (float)row * sc, (float)(c - st) * sc, sc, col);
                    } else {
                        c++;
                    }
                }
            }
        }
        x += 6.0f * sc;
        s++;
    }
}

static void drawTextS(float x, float y, float sc, unsigned int col, const char *s) {
    float o = sc >= 3.0f ? 2.0f : 1.0f;
    drawText(x + o, y + o, sc, RGBA(0, 0, 0, 200), s);
    drawText(x, y, sc, col, s);
}

static void drawTextC(float y, float sc, unsigned int col, const char *s) {
    drawTextS((float)SW * 0.5f - textW(s, sc) * 0.5f, y, sc, col, s);
}

/* ---------------------------------------------------------------------
 * Primitivas 3D
 * ------------------------------------------------------------------- */
static void tr(float x, float y, float z) {
    ScePspFVector3 v;
    v.x = x; v.y = y; v.z = z;
    sceGumTranslate(&v);
}

/* quad horizontal no plano XZ (igual ao original) */
static void quadXZ(float x1, float z1, float x2, float z2, float y, unsigned int color) {
    Vertex *v = (Vertex *)sceGuGetMemory(6 * sizeof(Vertex));
    v[0].color = color; v[0].x = x1; v[0].y = y; v[0].z = z1;
    v[1].color = color; v[1].x = x2; v[1].y = y; v[1].z = z1;
    v[2].color = color; v[2].x = x2; v[2].y = y; v[2].z = z2;
    v[3].color = color; v[3].x = x1; v[3].y = y; v[3].z = z1;
    v[4].color = color; v[4].x = x2; v[4].y = y; v[4].z = z2;
    v[5].color = color; v[5].x = x1; v[5].y = y; v[5].z = z2;
    sceGumDrawArray(GU_TRIANGLES, VTX_3D, 6, 0, v);
}

/* linha com espessura no chao */
static void lineXZ(float x1, float z1, float x2, float z2, float w, unsigned int c) {
    float dx = x2 - x1, dz = z2 - z1;
    float l = sqrtf(dx * dx + dz * dz);
    float nx, nz;
    Vertex *v;
    if (l < 0.0001f) return;
    nx = -dz / l * w * 0.5f;
    nz = dx / l * w * 0.5f;
    v = (Vertex *)sceGuGetMemory(6 * sizeof(Vertex));
    v[0].color = c; v[0].x = x1 + nx; v[0].y = 0.0f; v[0].z = z1 + nz;
    v[1].color = c; v[1].x = x1 - nx; v[1].y = 0.0f; v[1].z = z1 - nz;
    v[2].color = c; v[2].x = x2 - nx; v[2].y = 0.0f; v[2].z = z2 - nz;
    v[3].color = c; v[3].x = x1 + nx; v[3].y = 0.0f; v[3].z = z1 + nz;
    v[4].color = c; v[4].x = x2 - nx; v[4].y = 0.0f; v[4].z = z2 - nz;
    v[5].color = c; v[5].x = x2 + nx; v[5].y = 0.0f; v[5].z = z2 + nz;
    sceGumDrawArray(GU_TRIANGLES, VTX_3D, 6, 0, v);
}

static void rectLines(float x1, float z1, float x2, float z2, float w, unsigned int c) {
    lineXZ(x1, z1, x2, z1, w, c);
    lineXZ(x2, z1, x2, z2, w, c);
    lineXZ(x2, z2, x1, z2, w, c);
    lineXZ(x1, z2, x1, z1, w, c);
}

static void circleLines(float cx, float cz, float r, int seg, float w, unsigned int c) {
    int i;
    for (i = 0; i < seg; i++) {
        float a0 = 2.0f * PI * (float)i / (float)seg;
        float a1 = 2.0f * PI * (float)(i + 1) / (float)seg;
        lineXZ(cx + cosf(a0) * r, cz + sinf(a0) * r, cx + cosf(a1) * r, cz + sinf(a1) * r, w, c);
    }
}

/* caixa centrada na origem do sistema atual, com sombreamento por face */
static void boxRawO(float ox, float oy, float oz, float hx, float hy, float hz, unsigned int c) {
    static const signed char cc[8][3] = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
    };
    static const unsigned char ff[6][4] = {
        {3,2,6,7}, {0,1,5,4}, {4,5,6,7}, {0,1,2,3}, {1,2,6,5}, {0,3,7,4}
    };
    static const float sh[6] = { 1.0f, 0.45f, 0.88f, 0.72f, 0.62f, 0.80f };
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    Vertex *v = (Vertex *)sceGuGetMemory(36 * sizeof(Vertex));
    int f, k, n = 0;
    for (f = 0; f < 6; f++) {
        unsigned int col = shade(c, sh[f]);
        for (k = 0; k < 6; k++) {
            int id = ff[f][tri[k]];
            v[n].color = col;
            v[n].x = ox + (float)cc[id][0] * hx;
            v[n].y = oy + (float)cc[id][1] * hy;
            v[n].z = oz + (float)cc[id][2] * hz;
            n++;
        }
    }
    sceGumDrawArray(GU_TRIANGLES, VTX_3D, 36, 0, v);
}

static void boxRaw(float hx, float hy, float hz, unsigned int c) {
    boxRawO(0.0f, 0.0f, 0.0f, hx, hy, hz, c);
}

/* caixa em coordenadas de mundo */
static void boxAt(float x, float y, float z, float hx, float hy, float hz, unsigned int c) {
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    tr(x, y, z);
    boxRaw(hx, hy, hz, c);
}

/* parte de um corpo (usa a matriz atual como base) */
static void partBox(float x, float y, float z, float hx, float hy, float hz, unsigned int c) {
    boxRawO(x, y, z, hx, hy, hz, c);
}

/* ---------------------------------------------------------------------
 * Desenho do campo, gols e estadio
 * ------------------------------------------------------------------- */
static void drawField(void) {
    int i;
    unsigned int wl = RGB(240, 240, 240);
    float zt = FIELD_Z + 1.4f;
    float band = (2.0f * zt) / 8.0f;

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    quadXZ(-34.0f, -34.0f, 34.0f, 34.0f, -0.01f, RGB(28, 92, 36));
    quadXZ(-FIELD_X - 0.9f, -zt, FIELD_X + 0.9f, zt, 0.0f, RGB(36, 128, 44));
    for (i = 0; i < 8; i += 2) {
        float z0 = -zt + (float)i * band;
        quadXZ(-FIELD_X - 0.9f, z0, FIELD_X + 0.9f, z0 + band, 0.0f, RGB(44, 142, 52));
    }

    rectLines(-FIELD_X, -FIELD_Z, FIELD_X, FIELD_Z, 0.14f, wl);
    lineXZ(-FIELD_X, 0.0f, FIELD_X, 0.0f, 0.14f, wl);
    circleLines(0.0f, 0.0f, 2.6f, 24, 0.12f, wl);
    quadXZ(-0.12f, -0.12f, 0.12f, 0.12f, 0.0f, wl);

    rectLines(-4.6f, FIELD_Z - 4.2f, 4.6f, FIELD_Z, 0.12f, wl);
    rectLines(-2.4f, FIELD_Z - 1.6f, 2.4f, FIELD_Z, 0.12f, wl);
    rectLines(-4.6f, -FIELD_Z, 4.6f, -FIELD_Z + 4.2f, 0.12f, wl);
    rectLines(-2.4f, -FIELD_Z, 2.4f, -FIELD_Z + 1.6f, 0.12f, wl);
    quadXZ(-0.1f, FIELD_Z - 3.1f, 0.1f, FIELD_Z - 2.9f, 0.0f, wl);
    quadXZ(-0.1f, -FIELD_Z + 2.9f, 0.1f, -FIELD_Z + 3.1f, 0.0f, wl);
}

static void drawShadows(void) {
    int i;
    float s;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    for (i = 0; i < 8; i++)
        quadXZ(pl[i].x - 0.34f, pl[i].z - 0.24f, pl[i].x + 0.34f, pl[i].z + 0.24f, 0.0f, RGBA(0, 0, 0, 95));
    s = 0.22f / (1.0f + ball.y * 0.6f);
    quadXZ(ball.x - s, ball.z - s, ball.x + s, ball.z + s, 0.0f, RGBA(0, 0, 0, 110));
    sceGuDisable(GU_BLEND);
}

static void drawGoal(float zl, float dir) {
    unsigned int w = RGB(245, 245, 245), n = RGB(215, 215, 215);
    float bz = zl + dir * GOAL_DEPTH;
    float mz = zl + dir * GOAL_DEPTH * 0.5f;
    int k;

    boxAt(-GOAL_HW, GOAL_H * 0.5f, zl, 0.07f, GOAL_H * 0.5f, 0.07f, w);
    boxAt( GOAL_HW, GOAL_H * 0.5f, zl, 0.07f, GOAL_H * 0.5f, 0.07f, w);
    boxAt(0.0f, GOAL_H, zl, GOAL_HW + 0.07f, 0.07f, 0.07f, w);

    boxAt(-GOAL_HW, GOAL_H * 0.5f, bz, 0.04f, GOAL_H * 0.5f, 0.04f, n);
    boxAt( GOAL_HW, GOAL_H * 0.5f, bz, 0.04f, GOAL_H * 0.5f, 0.04f, n);
    boxAt(0.0f, GOAL_H, bz, GOAL_HW, 0.04f, 0.04f, n);
    boxAt(-GOAL_HW, GOAL_H, mz, 0.04f, 0.04f, GOAL_DEPTH * 0.5f, n);
    boxAt( GOAL_HW, GOAL_H, mz, 0.04f, 0.04f, GOAL_DEPTH * 0.5f, n);

    for (k = 1; k < 8; k++)
        boxAt(-GOAL_HW + (float)k * (2.0f * GOAL_HW) / 8.0f, GOAL_H * 0.5f, bz, 0.012f, GOAL_H * 0.5f, 0.012f, n);
    for (k = 1; k < 4; k++)
        boxAt(0.0f, (float)k * GOAL_H / 4.0f, bz, GOAL_HW, 0.012f, 0.012f, n);
    for (k = 1; k < 4; k++)
        boxAt(-GOAL_HW + (float)k * (2.0f * GOAL_HW) / 4.0f, GOAL_H, mz, 0.012f, 0.012f, GOAL_DEPTH * 0.5f, n);
}

static void drawStadium(void) {
    static const unsigned int adc[4] = { RGB(200, 40, 40), RGB(240, 240, 240), RGB(40, 80, 190), RGB(240, 200, 40) };
    static const unsigned int crowd[6] = { RGB(220, 60, 60), RGB(240, 240, 240), RGB(60, 110, 220), RGB(240, 200, 50), RGB(60, 180, 90), RGB(200, 90, 200) };
    float seg = (2.0f * (FIELD_Z + 1.4f)) / 8.0f;
    float segx = (2.0f * (FIELD_X + 1.4f)) / 6.0f;
    int i;

    for (i = 0; i < 8; i++) {
        float z = -FIELD_Z - 1.4f + seg * ((float)i + 0.5f);
        boxAt(-FIELD_X - 1.4f, 0.25f, z, 0.08f, 0.25f, seg * 0.5f - 0.03f, adc[i & 3]);
        boxAt( FIELD_X + 1.4f, 0.25f, z, 0.08f, 0.25f, seg * 0.5f - 0.03f, adc[(i + 1) & 3]);
    }
    for (i = 0; i < 6; i++) {
        float x = -FIELD_X - 1.4f + segx * ((float)i + 0.5f);
        boxAt(x, 0.25f, -FIELD_Z - 3.3f, segx * 0.5f - 0.03f, 0.25f, 0.08f, adc[(i + 2) & 3]);
        boxAt(x, 0.25f,  FIELD_Z + 3.3f, segx * 0.5f - 0.03f, 0.25f, 0.08f, adc[(i + 3) & 3]);
    }

    boxAt(-(FIELD_X + 3.8f), 1.1f, 0.0f, 1.6f, 1.1f, FIELD_Z + 3.5f, RGB(70, 72, 92));
    boxAt( (FIELD_X + 3.8f), 1.1f, 0.0f, 1.6f, 1.1f, FIELD_Z + 3.5f, RGB(70, 72, 92));
    boxAt(0.0f, 1.1f, -(FIELD_Z + 5.6f), FIELD_X + 3.8f, 1.1f, 1.6f, RGB(60, 62, 82));
    boxAt(0.0f, 1.1f,  (FIELD_Z + 5.6f), FIELD_X + 3.8f, 1.1f, 1.6f, RGB(60, 62, 82));

    for (i = 0; i < 16; i++) {
        float z = -FIELD_Z - 2.0f + (float)i * 2.5f;
        boxAt(-(FIELD_X + 3.8f), 2.35f, z, 0.5f, 0.35f, 0.5f, crowd[(i * 5) % 6]);
        boxAt( (FIELD_X + 3.8f), 2.35f, z, 0.5f, 0.35f, 0.5f, crowd[(i * 7 + 2) % 6]);
    }
    for (i = 0; i < 9; i++) {
        float x = -FIELD_X - 2.0f + (float)i * 2.5f;
        boxAt(x, 2.35f, -(FIELD_Z + 5.6f), 0.5f, 0.35f, 0.5f, crowd[(i * 5 + 1) % 6]);
        boxAt(x, 2.35f,  (FIELD_Z + 5.6f), 0.5f, 0.35f, 0.5f, crowd[(i * 7 + 3) % 6]);
    }
}

/* ---------------------------------------------------------------------
 * Jogador 3D animado (cabeca, tronco, bracos, pernas)
 * ------------------------------------------------------------------- */
/* Bloco de camisa: liso, listras verticais ou faixa de detalhe */
static void shirtBlock(float cy, float hx, float hy, float hz, const Kit *k) {
    if (k->stripes == 1) {
        int j;
        float w = hx / 5.0f;
        for (j = 0; j < 5; j++) {
            unsigned int c = (j & 1) ? k->shirt2 : k->shirt;
            partBox(-hx + w + (float)j * 2.0f * w, cy, 0.0f, w, hy, hz, c);
        }
    } else {
        partBox(0.0f, cy, 0.0f, hx, hy, hz, k->shirt);
    }
}

/* Jogador 3D com cara de gente: pescoco, cabeca com cabelo e rosto,
 * tronco com ombros, bracos com cotovelo, pernas com joelho e chuteira. */
static void drawPlayer(const Player *p, const Kit *k) {
    float sp = clampf(p->speed / 6.0f, 0.0f, 1.0f);
    float sw = sinf(p->anim) * 1.0f * sp;
    float lean = 0.20f * sp;
    unsigned int skin = skinTones[p->skin & 3];
    unsigned int skinD = shade(skin, 0.82f);
    unsigned int hair = hairTones[(p->hair + p->skin) & 3];
    unsigned int dark = RGB(25, 25, 30);
    unsigned int glove = RGB(240, 240, 240);
    int gk = (p->role == 0);
    float kk = (p->kickT > 0.0f) ? sinf(clampf(p->kickT, 0.0f, 1.0f) * PI) : 0.0f;
    float bobY = fabsf(sinf(p->anim)) * 0.05f * sp + sinf((float)frameCount * 0.07f + (float)p->number) * 0.004f;
    float armL, armR, legL, legR, elbL, elbR, kneeL, kneeR;
    float armSpread = 0.06f;
    float headTilt = 0.0f;
    float s;
    ScePspFVector3 sc;

    legL = sw; legR = -sw;
    armL = -sw * 1.1f; armR = sw * 1.1f;
    elbL = -(0.25f + sp * 0.9f);
    elbR = -(0.25f + sp * 0.9f);

    if (kk > 0.0f) {
        legR = -1.6f * kk;
        legL = 0.35f * kk;
        armL = -0.9f * kk;
        armR = 0.7f * kk;
        lean += 0.15f * kk;
    }
    if (p->slideT > 0.0f) {
        legL = -1.2f; legR = -1.0f;
        armL = -2.2f; armR = -2.2f;
        elbL = -0.2f; elbR = -0.2f;
        armSpread = 0.5f;
    }
    if (p->dribT > 0.0f) {
        float d = sinf(clampf(p->dribT / 0.45f, 0.0f, 1.0f) * PI);
        legL = 0.9f * d * p->dribSide;
        legR = -0.9f * d * p->dribSide;
        armL = -0.6f * d;
        armR = -0.6f * d;
        armSpread = 0.35f * d + 0.06f;
    }
    if (gk && p->diveT > 0.0f) {
        armL = -3.0f; armR = -3.0f;
        elbL = -0.1f; elbR = -0.1f;
        armSpread = 0.15f;
        legL = 0.3f; legR = -0.2f;
    } else if (gk && p->think > 0.0f && p->speed < 1.0f) {
        armSpread = 0.7f;
    }
    if (p->celebT > 0.0f) {
        float b = sinf(p->celebT * 14.0f);
        legL = 0.15f * b; legR = -0.15f * b;
        if (p->celebType == 0) {          /* bracos pro alto */
            armL = -2.9f + 0.15f * b; armR = -2.9f - 0.15f * b;
            elbL = -0.15f; elbR = -0.15f;
            armSpread = 0.35f;
        } else if (p->celebType == 1) {   /* aviaozinho */
            armL = 0.0f; armR = 0.0f;
            elbL = -0.05f; elbR = -0.05f;
            armSpread = 1.5f;
            lean = 0.35f;
        } else {                          /* soquinhos */
            armL = -1.2f + 0.5f * b; armR = -1.2f - 0.5f * b;
            elbL = -1.7f; elbR = -1.7f;
            armSpread = 0.1f;
        }
    }
    kneeL = 0.10f + (legL > 0.0f ? legL * 1.15f : 0.0f);
    kneeR = 0.10f + (legR > 0.0f ? legR * 1.15f : 0.0f);
    if (p->slideT > 0.0f) { kneeL = 0.15f; kneeR = 0.9f; }
    headTilt = sinf(p->anim * 0.5f) * 0.05f * sp;

    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();
    tr(p->x, p->jump, p->z);
    if (gk && p->diveT > 0.0f) {
        float fa = clampf(p->diveT / 0.55f, 0.0f, 1.0f);
        float ang = sinf((1.0f - fa) * PI * 0.5f + 0.35f) * 1.35f;
        tr(0.0f, 0.45f + p->diveH * 0.4f, 0.0f);
        sceGumRotateZ(-p->diveDir * ang);
        tr(0.0f, -0.45f, 0.0f);
    }
    sceGumRotateY(p->face + p->spinA);
    sc.x = 1.2f; sc.y = 1.2f; sc.z = 1.2f;
    sceGumScale(&sc);
    if (p->slideT > 0.0f) {
        tr(0.0f, 0.12f, 0.0f);
        sceGumRotateX(-1.25f);
    }
    if (p->dribT > 0.0f)
        sceGumRotateZ(p->dribSide * sinf(clampf(p->dribT / 0.45f, 0.0f, 1.0f) * PI) * 0.35f);

    /* ----- pernas ----- */
    for (s = -1.0f; s <= 1.0f; s += 2.0f) {
        float la = (s < 0.0f) ? legL : legR;
        float kn = (s < 0.0f) ? kneeL : kneeR;
        sceGumPushMatrix();
        tr(s * 0.095f, 0.53f + bobY, 0.0f);
        sceGumRotateX(la);
        partBox(0.0f, -0.10f, 0.0f, 0.082f, 0.11f, 0.09f, k->shorts);
        partBox(0.0f, -0.225f, 0.0f, 0.06f, 0.03f, 0.07f, skin);
        tr(0.0f, -0.26f, 0.0f);
        sceGumRotateX(kn);
        partBox(0.0f, -0.045f, 0.0f, 0.054f, 0.06f, 0.06f, skin);
        partBox(0.0f, -0.17f, 0.0f, 0.057f, 0.10f, 0.062f, k->socks);
        partBox(0.0f, -0.10f, 0.0f, 0.060f, 0.012f, 0.065f, k->trim);
        partBox(0.0f, -0.262f, 0.035f, 0.060f, 0.032f, 0.10f, k->boots);
        partBox(0.0f, -0.292f, 0.035f, 0.062f, 0.008f, 0.102f, RGB(235, 235, 235));
        sceGumPopMatrix();
    }

    /* ----- tronco ----- */
    sceGumPushMatrix();
    tr(0.0f, 0.53f + bobY, 0.0f);
    sceGumRotateX(lean);

    partBox(0.0f, 0.03f, 0.0f, 0.165f, 0.06f, 0.10f, k->shorts);
    shirtBlock(0.155f, 0.145f, 0.085f, 0.095f, k);
    shirtBlock(0.325f, 0.178f, 0.100f, 0.105f, k);
    if (k->stripes == 2)
        partBox(0.0f, 0.345f, 0.0f, 0.181f, 0.022f, 0.108f, k->trim);
    if (k->stripes == 1 || k->stripes == 0)
        partBox(0.0f, 0.228f, 0.0f, 0.15f, 0.008f, 0.098f, shade(k->shirt, 0.85f));
    /* detalhe do peito (escudo) */
    partBox(0.07f, 0.37f, 0.108f, 0.022f, 0.026f, 0.004f, k->trim);
    /* numero nas costas */
    partBox(0.0f, 0.31f, -0.108f, 0.075f, 0.075f, 0.004f, k->trim);
    partBox(0.0f, 0.31f, -0.113f, 0.045f, 0.052f, 0.003f, (k->stripes == 1) ? k->shirt : k->shirt2);
    /* gola e pescoco */
    partBox(0.0f, 0.43f, 0.0f, 0.075f, 0.018f, 0.075f, k->trim);
    partBox(0.0f, 0.465f, 0.0f, 0.04f, 0.03f, 0.04f, skin);

    /* bracos */
    for (s = -1.0f; s <= 1.0f; s += 2.0f) {
        float aa = (s < 0.0f) ? armL : armR;
        float el = (s < 0.0f) ? elbL : elbR;
        sceGumPushMatrix();
        tr(s * 0.225f, 0.39f, 0.0f);
        sceGumRotateZ(s * armSpread);
        sceGumRotateX(aa);
        partBox(0.0f, -0.075f, 0.0f, 0.047f, 0.085f, 0.052f, k->shirt);
        partBox(0.0f, -0.152f, 0.0f, 0.050f, 0.012f, 0.055f, k->trim);
        if (k->stripes == 2)
            partBox(0.0f, -0.06f, 0.0f, 0.049f, 0.014f, 0.054f, k->trim);
        tr(0.0f, -0.17f, 0.0f);
        sceGumRotateX(el);
        partBox(0.0f, -0.07f, 0.0f, 0.038f, 0.075f, 0.042f, gk ? k->shirt : skin);
        partBox(0.0f, -0.165f, 0.0f, 0.042f, 0.032f, 0.046f, gk ? glove : skinD);
        sceGumPopMatrix();
    }

    /* cabeca */
    sceGumPushMatrix();
    tr(0.0f, 0.5f, 0.0f);
    sceGumRotateZ(headTilt);
    partBox(0.0f, 0.085f, 0.0f, 0.088f, 0.098f, 0.088f, skin);
    partBox(-0.093f, 0.085f, 0.0f, 0.012f, 0.026f, 0.016f, skinD);
    partBox( 0.093f, 0.085f, 0.0f, 0.012f, 0.026f, 0.016f, skinD);
    partBox(0.0f, 0.073f, 0.094f, 0.013f, 0.016f, 0.012f, skinD);
    partBox(-0.036f, 0.105f, 0.090f, 0.015f, 0.012f, 0.006f, RGB(245, 245, 245));
    partBox( 0.036f, 0.105f, 0.090f, 0.015f, 0.012f, 0.006f, RGB(245, 245, 245));
    partBox(-0.036f, 0.105f, 0.095f, 0.008f, 0.010f, 0.006f, dark);
    partBox( 0.036f, 0.105f, 0.095f, 0.008f, 0.010f, 0.006f, dark);
    partBox(0.0f, 0.045f, 0.091f, 0.026f, 0.007f, 0.005f, shade(skin, 0.6f));
    switch ((p->hair) & 3) {
    case 0:  /* curto */
        partBox(0.0f, 0.18f, -0.004f, 0.092f, 0.032f, 0.092f, hair);
        partBox(0.0f, 0.13f, -0.088f, 0.090f, 0.07f, 0.012f, hair);
        break;
    case 1:  /* mais comprido atras */
        partBox(0.0f, 0.18f, -0.004f, 0.094f, 0.034f, 0.094f, hair);
        partBox(0.0f, 0.10f, -0.09f, 0.094f, 0.10f, 0.02f, hair);
        partBox(-0.092f, 0.14f, 0.0f, 0.014f, 0.05f, 0.07f, hair);
        partBox( 0.092f, 0.14f, 0.0f, 0.014f, 0.05f, 0.07f, hair);
        break;
    case 2:  /* volumoso */
        partBox(0.0f, 0.2f, -0.005f, 0.112f, 0.06f, 0.112f, hair);
        partBox(0.0f, 0.12f, -0.1f, 0.105f, 0.09f, 0.02f, hair);
        break;
    default: /* raspado + topete */
        partBox(0.0f, 0.18f, 0.0f, 0.090f, 0.014f, 0.09f, hair);
        partBox(0.0f, 0.195f, 0.045f, 0.06f, 0.022f, 0.03f, hair);
        break;
    }
    sceGumPopMatrix();

    sceGumPopMatrix();
    sceGumPopMatrix();
}

static void drawBall(void) {
    Vertex *v;
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    tr(ball.x, ball.y, ball.z);
    sceGumRotateZ(ball.spinZ);
    sceGumRotateX(ball.spinX);
    v = (Vertex *)sceGuGetMemory(sizeof(ballMesh));
    memcpy(v, ballMesh, sizeof(ballMesh));
    sceGumDrawArray(GU_TRIANGLES, VTX_3D, BALL_VERTS, 0, v);
}

/* ---------------------------------------------------------------------
 * Camera e mundo
 * ------------------------------------------------------------------- */
static void updateCamera(void) {
    float fx = ball.x * 0.7f + pl[ctrl].x * 0.3f;
    float fz = ball.z * 0.7f + pl[ctrl].z * 0.3f;
    if (gState == ST_GOAL && celebIdx >= 0) { fx = pl[celebIdx].x; fz = pl[celebIdx].z; }
    fz = clampf(fz, -12.0f, 12.0f);
    camX += (fx - camX) * 0.08f;
    camZ += (fz - camZ) * 0.08f;
}

static void drawWorld(void) {
    int i;
    ScePspFVector3 eye, ctr, up;

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(50.0f, 16.0f / 9.0f, 0.5f, 100.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    eye.x = camX * 0.7f; eye.y = 9.5f; eye.z = camZ + 11.5f;
    ctr.x = camX * 0.9f; ctr.y = 0.0f; ctr.z = camZ - 3.0f;
    if (gState == ST_INTRO) {
        /* varredura de camera: de cima e do lado, descendo ate a posicao de jogo */
        float t = clampf((INTRO_T - stateT) / (INTRO_T - 0.6f), 0.0f, 1.0f);
        float e = t * t * (3.0f - 2.0f * t);
        eye.x = (1.0f - e) * 9.0f;
        eye.y = 9.5f + (1.0f - e) * 6.0f;
        eye.z = camZ + 11.5f + (1.0f - e) * 6.0f;
        ctr.x = (1.0f - e) * 3.0f;
        ctr.z = camZ - 3.0f + (1.0f - e) * 2.0f;
    } else if (gState == ST_GOAL) {
        float t = clampf(celebElapsed / 1.5f, 0.0f, 1.0f);
        eye.y = 9.5f - 3.2f * t;
        eye.z = camZ + 11.5f - 3.6f * t;
        ctr.z = camZ - 3.0f + 2.6f * t;
        ctr.x = camX * 0.9f;
    }
    up.x = 0.0f; up.y = 1.0f; up.z = 0.0f;
    sceGumLookAt(&eye, &ctr, &up);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    /* chao: sem teste de profundidade, na ordem de desenho */
    sceGuDisable(GU_DEPTH_TEST);
    drawField();
    drawShadows();
    sceGuEnable(GU_DEPTH_TEST);

    drawStadium();
    drawGoal(-FIELD_Z, -1.0f);
    drawGoal(FIELD_Z, 1.0f);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    for (i = 0; i < 8; i++)
        drawPlayer(&pl[i], kits[i]);

    if (gState == ST_PLAY || gState == ST_INTRO) {
        float t = (float)frameCount;
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        tr(pl[ctrl].x, 1.95f + 0.07f * sinf(t * 0.12f), pl[ctrl].z);
        sceGumRotateY(t * 0.06f);
        boxRaw(0.09f, 0.09f, 0.09f, RGB(255, 235, 40));
    }

    drawBall();
}

/* ---------------------------------------------------------------------
 * HUD e telas sobrepostas
 * ------------------------------------------------------------------- */
static unsigned int withAlpha(unsigned int c, unsigned int a) {
    return (c & 0x00FFFFFFu) | (a << 24);
}

static unsigned int textOn(unsigned int c) {
    int lum = (int)(c & 0xFFu) * 3 + (int)((c >> 8) & 0xFFu) * 6 + (int)((c >> 16) & 0xFFu);
    return (lum > 1400) ? RGB(20, 22, 30) : RGB(255, 255, 255);
}

static const Kit *teamKit(int t) {
    return kits[t * 4 + 1];
}

/* placar estilo transmissao de TV */
static void drawScoreboard(void) {
    char tb[8], sa[8], sb[8];
    const Team *ta = &teams[teamOf[0]];
    const Team *tbm = &teams[teamOf[1]];
    const Kit *ka = teamKit(0), *kb = teamKit(1);
    int pos = (ball.owner >= 0) ? pl[ball.owner].team : -1;
    float x0 = 100.0f, y0 = 6.0f, w = 280.0f, h = 30.0f;
    float tw;

    /* sombra e base */
    rect2d(x0 - 2.0f, y0 + 2.0f, w + 4.0f, h + 2.0f, RGBA(0, 0, 0, 110));
    rectGrad(x0, y0, w, h, RGBA(34, 38, 52, 235), RGBA(10, 12, 20, 235));
    rect2d(x0, y0 + h - 2.0f, w, 2.0f, RGBA(255, 255, 255, 40));

    /* blocos com as cores do uniforme (camisa em cima, calcao embaixo) */
    rect2d(x0, y0, 14.0f, h * 0.65f, ka->shirt);
    rect2d(x0, y0 + h * 0.65f, 14.0f, h * 0.35f, ka->shorts);
    rect2d(x0 + w - 14.0f, y0, 14.0f, h * 0.65f, kb->shirt);
    rect2d(x0 + w - 14.0f, y0 + h * 0.65f, 14.0f, h * 0.35f, kb->shorts);

    drawTextS(x0 + 20.0f, y0 + 9.0f, 2.0f, RGB(255, 255, 255), ta->abbr);
    drawTextS(x0 + w - 20.0f - textW(tbm->abbr, 2.0f), y0 + 9.0f, 2.0f, RGB(255, 255, 255), tbm->abbr);

    /* caixas de placar */
    sa[0] = 0; sb[0] = 0;
    appendInt(sa, score[0]);
    appendInt(sb, score[1]);
    rect2d(x0 + 62.0f, y0 + 3.0f, 30.0f, h - 6.0f, RGBA(245, 245, 250, 240));
    rect2d(x0 + w - 92.0f, y0 + 3.0f, 30.0f, h - 6.0f, RGBA(245, 245, 250, 240));
    tw = textW(sa, 3.0f);
    drawText(x0 + 77.0f - tw * 0.5f + 1.0f, y0 + 6.0f, 3.0f, RGB(15, 18, 28), sa);
    tw = textW(sb, 3.0f);
    drawText(x0 + w - 77.0f - tw * 0.5f + 1.0f, y0 + 6.0f, 3.0f, RGB(15, 18, 28), sb);

    /* cronometro no centro */
    fmtTime(tb, (int)ceilf(timeLeft));
    rect2d(x0 + 100.0f, y0 + 3.0f, 80.0f, h - 6.0f,
           (timeLeft < 20.0f && ((frameCount / 15) & 1)) ? RGBA(230, 60, 50, 245) : RGBA(250, 205, 50, 245));
    drawText(x0 + 140.0f - textW(tb, 2.0f) * 0.5f + 1.0f, y0 + 8.0f, 2.0f, RGB(20, 20, 28), tb);

    /* posse de bola */
    if (pos == 0) rect2d(x0 + 96.0f, y0 + 12.0f, 3.0f, 6.0f, RGB(250, 205, 50));
    if (pos == 1) rect2d(x0 + 181.0f, y0 + 12.0f, 3.0f, 6.0f, RGB(250, 205, 50));
}

static void drawHUD(void) {
    begin2D();
    drawScoreboard();

    if (gState == ST_PLAY) {
        float st = clampf(pl[ctrl].stamina, 0.0f, 1.0f);
        rect2d(8.0f, 244.0f, 74.0f, 18.0f, RGBA(0, 0, 0, 140));
        drawTextS(12.0f, 247.0f, 1.0f, RGB(255, 255, 255), "FOLEGO");
        rect2d(12.0f, 255.0f, 66.0f, 5.0f, RGBA(255, 255, 255, 60));
        rectGrad(12.0f, 255.0f, 66.0f * st, 5.0f,
                 RGB(130, 230, 255), (st > 0.3f) ? RGB(40, 150, 230) : RGB(230, 80, 60));
        if (charge > 0.02f) {
            rect2d(176.0f, 246.0f, 128.0f, 16.0f, RGBA(0, 0, 0, 150));
            rect2d(180.0f, 252.0f, 120.0f, 6.0f, RGBA(255, 255, 255, 60));
            rectGrad(180.0f, 252.0f, 120.0f * charge, 6.0f,
                     RGB((int)(120 + 135 * charge), 230, 60), RGB(255, (int)(210 - 160 * charge), 30));
            drawTextS(214.0f, 238.0f, 1.0f, RGB(255, 255, 255), "FORCA");
        }
        if (timeLeft > MATCH_TIME - 12.0f) {
            rect2d(0.0f, 264.0f, (float)SW, 8.0f, RGBA(0, 0, 0, 130));
            drawTextC(265.0f, 1.0f, RGB(255, 255, 255), "X PASSE  QUADRADO CHUTA  O CORRE  L R DRIBLE  TRIANGULO PASSE ALTO");
        }
    }
    end2D();
}

static void drawGameOverlays(void) {
    char buf[48];
    if (gState == ST_INTRO) {
        float t = INTRO_T - stateT;
        float e = clampf(t / 0.9f, 0.0f, 1.0f);
        float off = (1.0f - e) * (1.0f - e) * 280.0f;
        float fade = clampf((stateT - 0.4f) / 0.6f, 0.0f, 1.0f);
        const Team *ta = &teams[teamOf[0]];
        const Team *tbm = &teams[teamOf[1]];
        const Kit *ka = teamKit(0), *kb = teamKit(1);
        unsigned int al = (unsigned int)(fade * 255.0f);
        begin2D();
        rect2d(0.0f, 84.0f, (float)SW, 92.0f * e, RGBA(0, 0, 0, (unsigned int)(150.0f * fade)));
        if (e > 0.2f) {
            drawTextC(90.0f, 2.0f, withAlpha(RGB(255, 235, 120), al), "AMISTOSO");
            rect2d(20.0f - off, 108.0f, 190.0f, 52.0f, withAlpha(ka->shirt, al));
            rect2d(20.0f - off, 152.0f, 190.0f, 8.0f, withAlpha(ka->shorts, al));
            rect2d((float)SW - 210.0f + off, 108.0f, 190.0f, 52.0f, withAlpha(kb->shirt, al));
            rect2d((float)SW - 210.0f + off, 152.0f, 190.0f, 8.0f, withAlpha(kb->shorts, al));
            drawTextS(115.0f - off - textW(ta->name, 2.0f) * 0.5f, 122.0f, 2.0f, withAlpha(textOn(ka->shirt), al), ta->name);
            drawTextS((float)SW - 115.0f + off - textW(tbm->name, 2.0f) * 0.5f, 122.0f, 2.0f, withAlpha(textOn(kb->shirt), al), tbm->name);
            drawTextS(240.0f - textW("VS", 4.0f) * 0.5f, 120.0f, 4.0f, withAlpha(RGB(255, 255, 255), al), "VS");
        }
        if (stateT < 1.6f && stateT > 0.2f && ((frameCount / 10) & 1) == 0)
            drawTextC(190.0f, 3.0f, RGB(255, 235, 120), "PREPARE-SE!");
        end2D();
    } else if (gState == ST_GOAL) {
        float t = celebElapsed;
        float e = clampf(t / 0.35f, 0.0f, 1.0f);
        float pulse = 9.0f + sinf(t * 9.0f) * 0.8f;
        const Kit *k = teamKit(scorerTeam);
        int i;
        begin2D();
        rect2d(0.0f, 52.0f, (float)SW, 84.0f * e, withAlpha(k->shirt, 215));
        rect2d(0.0f, 52.0f, (float)SW, 4.0f, RGBA(255, 255, 255, 220));
        rect2d(0.0f, 52.0f + 84.0f * e - 4.0f, (float)SW, 4.0f, withAlpha(k->shorts, 255));
        for (i = 0; i < 12; i++) {
            float fx = (float)((i * 97 + (int)(t * 160.0f)) % SW);
            rect2d(fx, 56.0f + (float)((i * 53) % 70), 6.0f, 6.0f, RGBA(255, 255, 255, 140));
        }
        if (e > 0.6f) {
            drawTextS(240.0f - textW("GOL!", pulse) * 0.5f, 62.0f, pulse, textOn(k->shirt) == RGB(255, 255, 255) ? RGB(255, 235, 40) : RGB(20, 22, 30), "GOL!");
            strcpy(buf, teams[teamOf[scorerTeam]].name);
            if (celebIdx >= 0) {
                strcat(buf, " - CAMISA ");
                appendInt(buf, pl[celebIdx].number);
            }
            drawTextS(240.0f - textW(buf, 2.0f) * 0.5f, 126.0f - 10.0f + 4.0f, 2.0f, textOn(k->shirt), buf);
        }
        end2D();
    } else if (gState == ST_PAUSE) {
        begin2D();
        rect2d(0.0f, 0.0f, (float)SW, (float)SH, RGBA(0, 0, 0, 160));
        rect2d(110.0f, 60.0f, 260.0f, 150.0f, RGBA(20, 26, 40, 235));
        rect2d(110.0f, 60.0f, 260.0f, 4.0f, RGB(250, 205, 50));
        drawTextC(76.0f, 5.0f, RGB(255, 255, 255), "PAUSE");
        drawTextC(130.0f, 2.0f, RGB(255, 235, 120), "START: CONTINUAR");
        drawTextC(160.0f, 2.0f, RGB(255, 235, 120), "SELECT: SAIR");
        end2D();
    } else if (gState == ST_END) {
        begin2D();
        rect2d(0.0f, 50.0f, (float)SW, 170.0f, RGBA(0, 0, 0, 185));
        rect2d(0.0f, 50.0f, (float)SW, 4.0f, RGB(250, 205, 50));
        drawTextC(62.0f, 4.0f, RGB(255, 235, 40), "FIM DE JOGO");
        strcpy(buf, teams[teamOf[0]].abbr);
        strcat(buf, " ");
        appendInt(buf, score[0]);
        strcat(buf, "-");
        appendInt(buf, score[1]);
        strcat(buf, " ");
        strcat(buf, teams[teamOf[1]].abbr);
        drawTextC(112.0f, 4.0f, RGB(255, 255, 255), buf);
        if (score[0] == score[1]) {
            drawTextC(160.0f, 3.0f, RGB(255, 255, 255), "EMPATE");
        } else {
            strcpy(buf, "VITORIA: ");
            strcat(buf, teams[teamOf[score[0] > score[1] ? 0 : 1]].name);
            drawTextC(160.0f, 3.0f, RGB(255, 255, 255), buf);
        }
        drawTextC(194.0f, 2.0f, RGB(255, 235, 120), "X: MENU");
        end2D();
    }
}

static void drawGame(void) {
    drawWorld();
    drawHUD();
    drawGameOverlays();
}

/* ---------------------------------------------------------------------
 * Menus
 * ------------------------------------------------------------------- */
static void drawMenuModel(const Kit *k, float ox, int skin, int pose) {
    Player mp;
    ScePspFVector3 eye, ctr, up;
    memset(&mp, 0, sizeof(mp));
    mp.x = ox;
    mp.face = menuAngle;
    mp.skin = skin;
    mp.hair = skin + 1;
    mp.number = 10;
    mp.role = 1;
    mp.speed = 0.0f;
    mp.anim = (float)frameCount * 0.17f;
    if (pose == 0) {                       /* jogando parado, respirando */
        mp.speed = 0.0f;
    } else if (pose == 1) {                /* trotando */
        mp.speed = 3.4f;
    } else {                               /* comemorando */
        mp.celebT = 1.0f + (float)frameCount * 0.0f;
        mp.celebType = (frameCount / 90) % 3;
        mp.jump = fabsf(sinf((float)frameCount * 0.12f)) * 0.22f;
    }

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(50.0f, 16.0f / 9.0f, 0.5f, 50.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    eye.x = 0.0f; eye.y = 1.05f; eye.z = 2.9f;
    ctr.x = 0.0f; ctr.y = 0.7f; ctr.z = 0.0f;
    up.x = 0.0f; up.y = 1.0f; up.z = 0.0f;
    sceGumLookAt(&eye, &ctr, &up);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuDisable(GU_DEPTH_TEST);
    circleLines(ox, 0.0f, 0.62f, 24, 0.05f, RGB(250, 205, 50));
    circleLines(ox, 0.0f, 0.50f, 24, 0.02f, RGB(255, 255, 255));
    quadXZ(ox - 0.45f, -0.45f, ox + 0.45f, 0.45f, 0.0f, RGB(14, 60, 28));
    sceGuEnable(GU_DEPTH_TEST);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    drawPlayer(&mp, k);
}

static void drawMenuBG(void) {
    int i;
    float t = (float)frameCount;
    begin2D();
    rectGrad(0.0f, 0.0f, (float)SW, (float)SH, RGB(10, 84, 38), RGB(2, 24, 12));
    /* faixas de gramado em movimento lento */
    for (i = -1; i < 9; i++) {
        float x = (float)i * 64.0f + fmodf(t * 0.25f, 128.0f);
        if (((i + 8) & 1) == 0) rect2d(x, 0.0f, 64.0f, (float)SH, RGBA(255, 255, 255, 14));
    }
    /* feixe de luz varrendo */
    {
        float bx = fmodf(t * 1.1f, (float)SW + 200.0f) - 100.0f;
        rectGrad(bx, 0.0f, 70.0f, (float)SH, RGBA(255, 255, 255, 0), RGBA(255, 255, 255, 22));
    }
    /* confetes */
    for (i = 0; i < 18; i++) {
        float fx = fmodf((float)(i * 137) + t * (0.4f + (float)(i % 4) * 0.2f), (float)SW);
        float fy = fmodf((float)(i * 71) + t * (0.8f + (float)(i % 3) * 0.4f), (float)SH);
        unsigned int cc = (i % 3 == 0) ? RGBA(250, 205, 50, 150) : ((i % 3 == 1) ? RGBA(255, 255, 255, 110) : RGBA(110, 190, 255, 120));
        rect2d(fx, fy, 3.0f, 3.0f, cc);
    }
    /* barras superior e inferior */
    rect2d(0.0f, 0.0f, (float)SW, 6.0f, RGB(250, 205, 50));
    rect2d(0.0f, (float)SH - 6.0f, (float)SW, 6.0f, RGB(250, 205, 50));
    end2D();
}

static void keycap(float x, float y, float sc, const char *label) {
    float w = textW(label, sc) + 10.0f;
    float h = 7.0f * sc + 8.0f;
    rect2d(x, y + 2.0f, w, h, RGB(10, 14, 22));
    rect2d(x, y, w, h, RGB(60, 68, 90));
    rect2d(x + 2.0f, y + 2.0f, w - 4.0f, h - 5.0f, RGB(228, 232, 242));
    drawText(x + 5.0f, y + 4.0f, sc, RGB(20, 24, 36), label);
}

static void teamTile(float x, float y, int t, int selected, int disabled) {
    const Kit *k = &teams[t].kit;
    float w = 92.0f, h = 58.0f;
    float lift = selected ? 6.0f : 0.0f;
    unsigned int al = disabled ? 90u : 255u;
    y -= lift;
    if (selected) rect2d(x - 3.0f, y - 3.0f, w + 6.0f, h + 6.0f, RGB(250, 205, 50));
    rect2d(x, y, w, h, withAlpha(RGB(18, 24, 38), al));
    rect2d(x + 4.0f, y + 4.0f, w - 8.0f, 26.0f, withAlpha(k->shirt, al));
    if (k->stripes == 1) {
        int i;
        for (i = 0; i < 5; i += 2) rect2d(x + 4.0f + (float)i * 16.8f, y + 4.0f, 16.8f, 26.0f, withAlpha(k->shirt2, al));
    }
    if (k->stripes == 2) rect2d(x + 4.0f, y + 14.0f, w - 8.0f, 5.0f, withAlpha(k->trim, al));
    rect2d(x + 4.0f, y + 30.0f, w - 8.0f, 8.0f, withAlpha(k->shorts, al));
    rect2d(x + 4.0f, y + 38.0f, w - 8.0f, 4.0f, withAlpha(k->socks, al));
    drawTextS(x + w * 0.5f - textW(teams[t].abbr, 2.0f) * 0.5f, y + 43.0f, 2.0f, withAlpha(RGB(255, 255, 255), al), teams[t].abbr);
}

static void drawMenu(void) {
    static const char *items[3] = { "JOGAR", "CONTROLES", "SAIR" };
    static const char *keys[9] = { "ANALOGICO", "O SEGURAR", "O TOQUE", "X", "QUADRADO", "TRIANGULO", "L OU R", "L SEM BOLA", "START" };
    static const char *desc[9] = {
        "MOVER", "CORRER", "CARRINHO SEM BOLA", "PASSE", "CHUTE SEGURE PARA FORCA",
        "PASSE ALTO OU TROCAR", "DRIBLE COM BOLA", "TROCAR JOGADOR", "PAUSAR"
    };
    int i;
    char buf[32];

    drawMenuBG();

    switch (gState) {
    case ST_MENU: {
        int tm = (frameCount / 240) & 3;
        int pose = (frameCount / 120) % 3;
        drawMenuModel(&teams[tm].kit, 1.2f, tm, pose);
        begin2D();
        rect2d(26.0f, 28.0f, 250.0f, 3.0f, RGB(250, 205, 50));
        drawTextS(28.0f, 38.0f, 5.0f, RGB(255, 255, 255), "FUTEBOL");
        drawTextS(28.0f, 76.0f, 5.0f, RGB(250, 205, 50), "PSP");
        for (i = 0; i < 3; i++) {
            float y = 128.0f + (float)i * 40.0f;
            if (i == menuSel) {
                float sl = 6.0f + sinf((float)frameCount * 0.15f) * 3.0f;
                rect2d(28.0f + sl, y, 210.0f, 32.0f, RGB(250, 205, 50));
                rect2d(28.0f + sl, y + 28.0f, 210.0f, 4.0f, RGB(200, 150, 20));
                drawText(44.0f + sl, y + 6.0f, 3.0f, RGB(20, 24, 36), items[i]);
                drawText(218.0f + sl, y + 6.0f, 3.0f, RGB(20, 24, 36), ">");
            } else {
                rect2d(28.0f, y, 210.0f, 32.0f, RGBA(0, 0, 0, 120));
                drawText(44.0f, y + 6.0f, 3.0f, RGB(235, 240, 235), items[i]);
            }
        }
        rect2d(0.0f, 252.0f, (float)SW, 14.0f, RGBA(0, 0, 0, 130));
        drawTextC(255.0f, 1.0f, RGB(210, 230, 210), "CIMA BAIXO ESCOLHE   X CONFIRMA");
        end2D();
        break;
    }

    case ST_CONTROLS:
        begin2D();
        rect2d(0.0f, 14.0f, (float)SW, 22.0f, RGBA(0, 0, 0, 140));
        drawTextC(18.0f, 2.5f, RGB(250, 205, 50), "CONTROLES");
        for (i = 0; i < 9; i++) {
            float y = 44.0f + (float)i * 22.0f;
            keycap(24.0f, y, 1.0f, keys[i]);
            drawTextS(128.0f, y + 3.0f, 1.0f, RGB(255, 255, 255), desc[i]);
        }
        rect2d(0.0f, 252.0f, (float)SW, 14.0f, RGBA(0, 0, 0, 130));
        drawTextC(255.0f, 1.0f, RGB(210, 230, 210), "X OU O VOLTA");
        end2D();
        break;

    case ST_SEL_HOME:
    case ST_SEL_AWAY: {
        int sel = (gState == ST_SEL_HOME) ? selHome : selAway;
        int other = (gState == ST_SEL_AWAY) ? selHome : -1;
        const Kit *k = &teams[sel].kit;
        drawMenuModel(k, 1.15f, sel, (frameCount / 150) % 3 == 1 ? 1 : 0);
        begin2D();
        rect2d(0.0f, 10.0f, (float)SW, 24.0f, RGBA(0, 0, 0, 150));
        drawTextC(15.0f, 2.5f, RGB(250, 205, 50), gState == ST_SEL_HOME ? "ESCOLHA SEU TIME" : "ESCOLHA O ADVERSARIO");
        rect2d(20.0f, 50.0f, 6.0f, 62.0f, k->shirt);
        drawTextS(34.0f, 52.0f, 3.0f, RGB(255, 255, 255), teams[sel].name);
        /* amostras do uniforme */
        drawTextS(34.0f, 84.0f, 1.0f, RGB(210, 230, 210), "UNIFORME");
        rect2d(34.0f, 96.0f, 30.0f, 14.0f, k->shirt);
        rect2d(68.0f, 96.0f, 30.0f, 14.0f, k->shorts);
        rect2d(102.0f, 96.0f, 30.0f, 14.0f, k->socks);
        rect2d(136.0f, 96.0f, 30.0f, 14.0f, k->trim);
        if (gState == ST_SEL_AWAY) {
            strcpy(buf, teams[selHome].abbr);
            strcat(buf, " X ");
            strcat(buf, teams[sel].abbr);
            drawTextS(34.0f, 124.0f, 2.0f, RGB(255, 235, 120), buf);
        }
        drawTextS(8.0f, 138.0f, 4.0f, RGB(255, 255, 255), "<");
        for (i = 0; i < 4; i++) {
            float x = 34.0f + (float)i * 100.0f;
            teamTile(x, 178.0f, i, i == sel, i == other);
        }
        drawTextS((float)SW - 26.0f, 138.0f, 4.0f, RGB(255, 255, 255), ">");
        rect2d(0.0f, 252.0f, (float)SW, 14.0f, RGBA(0, 0, 0, 130));
        drawTextC(255.0f, 1.0f, RGB(210, 230, 210), "ESQ DIR ESCOLHE   X CONFIRMA   O VOLTA");
        end2D();
        break;
    }
    default:
        break;
    }
}

/* ---------------------------------------------------------------------
 * Acoes de bola
 * ------------------------------------------------------------------- */
static void takeBall(int i) {
    ball.owner = i;
    ball.lastTeam = pl[i].team;
    ball.lastTouch = i;
    ball.passTarget = -1;
    ball.passT = 0.0f;
}

static void kickBall(int idx, float vx, float vy, float vz) {
    Player *p = &pl[idx];
    ball.owner = -1;
    ball.vx = vx; ball.vy = vy; ball.vz = vz;
    if (ball.y < BALL_R + 0.02f) ball.y = BALL_R + 0.03f;
    ball.lastTeam = p->team;
    ball.lastTouch = idx;
    p->cool = 0.4f;
    p->kickT = 0.001f;
}

static int canKick(int i) {
    if (ball.owner == i) return 1;
    if (ball.owner == -1 && distf(pl[i].x, pl[i].z, ball.x, ball.z) < 1.15f && ball.y < 1.0f) return 1;
    return 0;
}

static void doShoot(int idx, float power, float inputX) {
    Player *p = &pl[idx];
    float dirZ = (p->team == 0) ? -1.0f : 1.0f;
    float fx = p->aimX, fz = p->aimZ;
    float gz = dirZ * FIELD_Z;
    float dz = gz - p->z;
    float dx, dzv, l, sp, vy;

    if (fx == 0.0f && fz == 0.0f) { fx = sinf(p->face); fz = cosf(p->face); }

    if (fz * dirZ > 0.3f && fabsf(dz) < 24.0f) {
        float tx = clampf(inputX * 1.5f, -1.45f, 1.45f) + rndRange(-1.0f, 1.0f) * (0.12f + 0.55f * power);
        dx = tx - p->x;
        dzv = dz;
    } else {
        dx = fx;
        dzv = fz;
    }
    l = sqrtf(dx * dx + dzv * dzv);
    if (l < 0.001f) { dx = 0.0f; dzv = dirZ; l = 1.0f; }
    dx /= l; dzv /= l;

    sp = 8.0f + 13.0f * power;
    vy = 1.0f + 3.8f * power;
    kickBall(idx, dx * sp, vy, dzv * sp);
}

static void doPass(int idx, int mate, int lofted) {
    Player *p = &pl[idx];
    Player *m = &pl[mate];
    float d = distf(p->x, p->z, m->x, m->z);
    float sp = clampf(6.5f + d * 0.85f, 7.0f, 15.5f);
    float t = d / sp;
    float tx = m->x + m->vx * t * 0.7f;
    float tz = m->z + m->vz * t * 0.7f;
    float dx = tx - ball.x, dz = tz - ball.z;
    float l = sqrtf(dx * dx + dz * dz);
    float vy = lofted ? (2.5f + d * 0.12f) : 0.0f;
    if (l < 0.01f) l = 1.0f;
    (void)p;
    kickBall(idx, dx / l * sp, vy, dz / l * sp);
    ball.passTarget = mate;
    ball.passT = 1.8f;
}

static int nearestOutfielder(int team, float x, float z, int exclude) {
    int i, best = -1;
    float bd = 1.0e9f;
    for (i = team * 4 + 1; i < team * 4 + 4; i++) {
        float d;
        if (i == exclude) continue;
        d = distf(pl[i].x, pl[i].z, x, z);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static int bestMate(int idx, float ax, float az, int useFacing) {
    Player *p = &pl[idx];
    int t = p->team, i, best = -1;
    float bs = -1.0e9f;
    float dirZ = (t == 0) ? -1.0f : 1.0f;
    for (i = t * 4 + 1; i < t * 4 + 4; i++) {
        float dx, dz, d, s;
        if (i == idx) continue;
        dx = pl[i].x - p->x;
        dz = pl[i].z - p->z;
        d = sqrtf(dx * dx + dz * dz);
        if (useFacing) {
            float c = (dx * ax + dz * az) / (d + 0.001f);
            s = c * 3.0f - d * 0.08f;
            if (c < -0.1f) s -= 5.0f;
        } else {
            s = dirZ * dz * 0.25f - d * 0.06f;
        }
        if (s > bs) { bs = s; best = i; }
    }
    return best;
}

/* ---------------------------------------------------------------------
 * Inteligencia artificial
 * ------------------------------------------------------------------- */
static void moveToward(Player *p, float tx, float tz, float spd) {
    float dx = tx - p->x, dz = tz - p->z;
    float d = sqrtf(dx * dx + dz * dz);
    float s;
    if (d < 0.12f) { p->dvx = 0.0f; p->dvz = 0.0f; return; }
    s = spd * clampf(d * 0.9f, 0.0f, 1.0f);
    p->dvx = dx / d * s;
    p->dvz = dz / d * s;
}

static void anchorPos(const Player *p, float *ax, float *az) {
    int t = p->team;
    float dirZ = (t == 0) ? -1.0f : 1.0f;
    int has = (ball.owner >= 0 && pl[ball.owner].team == t);
    float push = has ? 6.0f : 0.0f;
    float x, z;
    if (p->role == 1) push *= 0.5f;
    x = p->homeX + ball.x * 0.25f;
    z = p->homeZ + ball.z * 0.45f + dirZ * push;
    *ax = clampf(x, -8.0f, 8.0f);
    *az = clampf(z, -14.5f, 14.5f);
}

static void aiCarrier(int i) {
    Player *p = &pl[i];
    int t = p->team, o;
    float dirZ = (t == 0) ? -1.0f : 1.0f;
    float gz = dirZ * FIELD_Z;
    float d = fabsf(gz - p->z);
    float od = 1.0e9f;
    int on = (1 - t) * 4;
    float tx, tz;

    for (o = (1 - t) * 4; o < (1 - t) * 4 + 4; o++) {
        float dd = distf(pl[o].x, pl[o].z, p->x, p->z);
        if (dd < od) { od = dd; on = o; }
    }

    tx = clampf(p->x * 0.9f, -4.0f, 4.0f);
    tz = gz;
    if (od < 2.5f) tx += signf(p->x - pl[on].x) * 2.0f;
    tx = clampf(tx, -7.5f, 7.5f);

    if (p->think <= 0.0f) {
        if (d < 9.0f && rnd01() < 0.75f) {
            p->aimX = 0.0f;
            p->aimZ = dirZ;
            doShoot(i, rndRange(0.55f, 1.0f), rndRange(-1.0f, 1.0f));
            p->think = 0.8f;
            return;
        }
        if (od < 1.7f && p->dribT <= 0.0f && rnd01() < 0.45f) {
            p->aimX = 0.0f;
            p->aimZ = dirZ;
            doDribble(i, signf(p->x - pl[on].x + 0.001f));
            p->think = 0.5f;
            return;
        }
        if (od < 1.9f && rnd01() < 0.7f) {
            int m = bestMate(i, 0.0f, 0.0f, 0);
            if (m >= 0) {
                doPass(i, m, 0);
                p->think = 0.6f;
                return;
            }
        }
        p->think = rndRange(0.25f, 0.5f);
    }
    moveToward(p, tx, tz, (t == 0) ? 4.4f : 4.2f);
}

static void aiKeeper(int i) {
    Player *p = &pl[i];
    int t = p->team;
    float own = (t == 0) ? FIELD_Z : -FIELD_Z;
    float sgn = (t == 0) ? 1.0f : -1.0f;
    float bs = sqrtf(ball.vx * ball.vx + ball.vz * ball.vz);
    float dBall = distf(p->x, p->z, ball.x, ball.z);
    float bx, bz, dl, ln, r, tx, tz, spd = 4.8f;
    int shot = 0;

    /* com a bola: segura, olha para um companheiro e repoe */
    if (ball.owner == i) {
        int m = bestMate(i, 0.0f, 0.0f, 0);
        p->dvx = 0.0f; p->dvz = 0.0f;
        if (m >= 0) p->face = wrapAng(p->face + wrapAng(atan2f(pl[m].x - p->x, pl[m].z - p->z) - p->face) * 0.2f);
        gkHold -= DT;
        if (gkHold <= 0.0f && m >= 0) doPass(i, m, 1);
        return;
    }

    /* posicao base: na bissetriz entre a bola e o centro do gol */
    bx = ball.x; bz = ball.z;
    dl = fabsf(own - bz);
    r = clampf(1.0f + dl * 0.07f, 1.0f, 2.2f);
    tx = clampf(bx * (r / (dl + 0.5f)) * 1.6f, -2.2f, 2.2f);
    tz = own - sgn * r;

    /* leitura de chute: bola solta indo para o gol */
    if (ball.owner < 0 && ball.vz * sgn > 1.5f && bs > 5.0f) {
        float th = (own - ball.z) / ball.vz;
        if (th > 0.0f && th < 1.4f) {
            float py = ball.y + ball.vy * th - 0.5f * GRAV * th * th;
            float px = ball.x + ball.vx * th;
            if (fabsf(px) < GOAL_HW + 0.8f && py < GOAL_H + 0.6f) {
                shot = 1;
                if (p->aimZ == 0.0f) {          /* tempo de reacao e erro de leitura, uma vez por chute */
                    p->aimX = rndRange(-0.55f, 0.55f);
                    p->aimZ = 1.0f;
                    p->think = rndRange(0.06f, 0.18f);
                }
                if (p->think <= 0.0f) {
                    tx = clampf(px + p->aimX, -2.4f, 2.4f);
                    tz = own - sgn * 0.7f;
                    spd = 6.0f;
                    if (fabsf(tx - p->x) > 0.8f && th < 0.5f && p->diveT <= 0.0f && p->cool <= 0.0f) {
                        p->diveT = 0.55f;
                        p->diveDir = signf(tx - p->x);
                    }
                }
            }
        }
    }
    if (!shot) p->aimZ = 0.0f;

    /* saida do gol: atacante sozinho perto da area ou bola solta perto */
    if (!shot) {
        if (ball.owner >= 0 && pl[ball.owner].team != t && dl < 6.5f && dBall < 6.5f) {
            tx = clampf(bx * 0.8f, -3.0f, 3.0f);
            tz = own - sgn * clampf(dl - 1.6f, 1.0f, 4.2f);
            spd = 5.6f;
            if (dBall < 1.6f && p->diveT <= 0.0f && p->cool <= 0.0f && rnd01() < 0.05f) {
                p->diveT = 0.4f;
                p->diveDir = signf(bx - p->x + 0.001f);
            }
        } else if (ball.owner < 0 && bs < 5.0f && dl < 5.0f && fabsf(bx) < 4.0f) {
            tx = bx; tz = bz - sgn * 0.3f;
            spd = 5.4f;
        }
    }
    ln = tz;
    moveToward(p, tx, ln, spd);
}

static void updateAI(void) {
    int i, t;
    int ownerTeam = (ball.owner >= 0) ? pl[ball.owner].team : -1;
    int chaser[2];

    chaser[0] = nearestOutfielder(0, ball.x, ball.z, ctrl);
    chaser[1] = nearestOutfielder(1, ball.x, ball.z, -1);

    for (i = 0; i < 8; i++) {
        Player *p = &pl[i];
        if (i == ctrl) continue;
        t = p->team;

        if (p->role == 0) { aiKeeper(i); continue; }
        if (ball.owner == i) { aiCarrier(i); continue; }

        {
            float tx, tz, spd = 3.8f;
            int chase = 0;
            int pt = ball.passTarget;

            if (pt == i) chase = 1;
            else if (i == chaser[t] && ownerTeam != t && !(pt >= 0 && pl[pt].team == t)) chase = 1;

            if (chase) {
                tx = ball.x + ball.vx * 0.2f;
                tz = ball.z + ball.vz * 0.2f;
                spd = (t == 0) ? 5.0f : 4.7f;
                if (ownerTeam >= 0 && ownerTeam != t) {
                    float d = distf(p->x, p->z, ball.x, ball.z);
                    if (d < 1.5f && p->think <= 0.0f && p->slideT <= 0.0f && rnd01() < 0.02f) {
                        float dx = ball.x - p->x, dz = ball.z - p->z;
                        float l = sqrtf(dx * dx + dz * dz);
                        if (l < 0.01f) l = 1.0f;
                        p->slideT = 0.4f;
                        p->slx = dx / l;
                        p->slz = dz / l;
                        p->think = 1.6f;
                    }
                }
            } else {
                anchorPos(p, &tx, &tz);
            }
            moveToward(p, tx, tz, spd);
        }
    }
}

/* ---------------------------------------------------------------------
 * Controle do jogador humano
 * ------------------------------------------------------------------- */
static void switchPlayer(void) {
    int n = nearestOutfielder(0, ball.x, ball.z, ctrl);
    if (n >= 0) { ctrl = n; charge = 0.0f; }
}

static void doDribble(int idx, float side) {
    Player *p = &pl[idx];
    float hx, hz, rx, rz, l;
    if (p->dribT > 0.0f || p->slideT > 0.0f) return;
    hx = p->aimX; hz = p->aimZ;
    l = sqrtf(hx * hx + hz * hz);
    if (l < 0.01f) { hx = sinf(p->face); hz = cosf(p->face); l = 1.0f; }
    hx /= l; hz /= l;
    rx = -hz; rz = hx;                 /* direita relativa ao sentido do corpo */
    p->dribSide = side;
    p->dribT = 0.45f;
    p->dribVx = (hx * 0.55f + rx * side * 1.0f) * 7.2f;
    p->dribVz = (hz * 0.55f + rz * side * 1.0f) * 7.2f;
}

static void updateUser(const SceCtrlData *pad) {
    Player *c = &pl[ctrl];
    float mx = 0.0f, mz = 0.0f, mag, spd = 4.4f;
    float ax = ((float)((int)pad->Lx - 128)) / 128.0f;
    float ay = ((float)((int)pad->Ly - 128)) / 128.0f;
    int wantSprint, hasBall = (ball.owner == ctrl);

    if (pad->Buttons & PSP_CTRL_LEFT)  mx -= 1.0f;
    if (pad->Buttons & PSP_CTRL_RIGHT) mx += 1.0f;
    if (pad->Buttons & PSP_CTRL_UP)    mz -= 1.0f;
    if (pad->Buttons & PSP_CTRL_DOWN)  mz += 1.0f;
    if (fabsf(ax) > 0.22f) mx += ax;
    if (fabsf(ay) > 0.22f) mz += ay;

    mag = sqrtf(mx * mx + mz * mz);
    if (mag > 1.0f) { mx /= mag; mz /= mag; mag = 1.0f; }

    /* O corre (segurar). Sem a bola, um toque rapido em O da carrinho.
     * R tambem corre quando o jogador esta sem a bola. */
    wantSprint = (pad->Buttons & PSP_CTRL_CIRCLE) != 0;
    if (!hasBall && (pad->Buttons & PSP_CTRL_RTRIGGER)) wantSprint = 1;
    if (wantSprint && c->stamina > 0.02f && mag > 0.3f) {
        spd = 6.3f;
        c->stamina -= 0.28f * DT;
    } else {
        c->stamina += 0.12f * DT;
    }
    c->stamina = clampf(c->stamina, 0.0f, 1.0f);

    if (c->slideT <= 0.0f && c->dribT <= 0.0f) {
        c->dvx = mx * spd;
        c->dvz = mz * spd;
    }

    if (mag > 0.25f) {
        c->aimX = mx / mag;
        c->aimZ = mz / mag;
    } else {
        c->aimX = sinf(c->face);
        c->aimZ = cosf(c->face);
    }

    /* QUADRADO: chute (segure para dar forca, solte para chutar) */
    if (pad->Buttons & PSP_CTRL_SQUARE) charge = clampf(charge + DT / 0.85f, 0.0f, 1.0f);
    if (gReleased & PSP_CTRL_SQUARE) {
        if (canKick(ctrl)) doShoot(ctrl, charge, mx);
        charge = 0.0f;
    } else if (!(pad->Buttons & PSP_CTRL_SQUARE)) {
        charge = 0.0f;
    }

    /* X: passe rasteiro */
    if (gPressed & PSP_CTRL_CROSS) {
        if (canKick(ctrl)) {
            int m = bestMate(ctrl, c->aimX, c->aimZ, 1);
            if (m >= 0) doPass(ctrl, m, 0);
        }
    }

    /* O: toque rapido sem a bola = carrinho */
    if (pad->Buttons & PSP_CTRL_CIRCLE) c->oHold += DT;
    if (gReleased & PSP_CTRL_CIRCLE) {
        if (!canKick(ctrl) && c->oHold < 0.22f && c->think <= 0.0f && c->slideT <= 0.0f) {
            c->slideT = 0.42f;
            c->slx = c->aimX;
            c->slz = c->aimZ;
            c->think = 1.1f;
        }
        c->oHold = 0.0f;
    } else if (!(pad->Buttons & PSP_CTRL_CIRCLE)) {
        c->oHold = 0.0f;
    }

    /* TRIANGULO: passe alto (com bola) ou troca de jogador (sem bola) */
    if (gPressed & PSP_CTRL_TRIANGLE) {
        if (canKick(ctrl)) {
            int m = bestMate(ctrl, c->aimX, c->aimZ, 1);
            if (m >= 0) doPass(ctrl, m, 1);
        } else {
            switchPlayer();
        }
    }

    /* L / R com a bola: dribles (esquerda / direita). Sem a bola, L troca de jogador. */
    if (hasBall) {
        if (gPressed & PSP_CTRL_LTRIGGER) doDribble(ctrl, -1.0f);
        if (gPressed & PSP_CTRL_RTRIGGER) doDribble(ctrl, 1.0f);
    } else if (gPressed & PSP_CTRL_LTRIGGER) {
        switchPlayer();
    }
}

/* ---------------------------------------------------------------------
 * Atualizacao dos jogadores e da bola
 * ------------------------------------------------------------------- */
static void updatePlayers(void) {
    int i, j;
    for (i = 0; i < 8; i++) {
        Player *p = &pl[i];
        float dvx = p->dvx, dvz = p->dvz, a, tf, d;

        if (p->slideT > 0.0f) {
            p->slideT -= DT;
            dvx = p->slx * 8.5f;
            dvz = p->slz * 8.5f;
            a = 0.35f;
        } else {
            a = clampf(DT * 9.0f, 0.0f, 1.0f);
        }
        if (p->dribT > 0.0f) {
            float pr = 1.0f - p->dribT / 0.45f;
            p->dribT -= DT;
            dvx = p->dribVx * (1.0f - 0.5f * pr);
            dvz = p->dribVz * (1.0f - 0.5f * pr);
            a = 0.30f;
            p->spinA = p->dribSide * sinf(clampf(pr, 0.0f, 1.0f) * PI) * 1.1f;
            if (p->dribT <= 0.0f) { p->dribT = 0.0f; p->spinA = 0.0f; }
        }
        if (p->diveT > 0.0f) {
            p->diveT -= DT;
            dvx += p->diveDir * 6.5f;
            p->diveH = clampf(p->diveT / 0.55f, 0.0f, 1.0f);
        } else {
            p->diveH = 0.0f;
        }
        if (p->kickT > 0.0f) {
            p->kickT += DT / 0.32f;
            if (p->kickT >= 1.0f) p->kickT = 0.0f;
        }
        if (p->celebT > 0.0f) {
            p->celebT -= DT;
            p->jump = fabsf(sinf(p->celebT * 7.0f)) * 0.35f;
            if (p->celebT <= 0.0f) { p->celebT = 0.0f; p->jump = 0.0f; }
        }

        p->vx += (dvx - p->vx) * a;
        p->vz += (dvz - p->vz) * a;
        p->x += p->vx * DT;
        p->z += p->vz * DT;
        p->x = clampf(p->x, -FIELD_X - 0.8f, FIELD_X + 0.8f);
        p->z = clampf(p->z, -FIELD_Z + 0.3f, FIELD_Z - 0.3f);
        p->speed = sqrtf(p->vx * p->vx + p->vz * p->vz);

        if (p->speed > 0.8f) tf = atan2f(p->vx, p->vz);
        else tf = atan2f(ball.x - p->x, ball.z - p->z);
        d = wrapAng(tf - p->face);
        p->face = wrapAng(p->face + d * clampf(DT * 10.0f, 0.0f, 1.0f));

        p->anim += p->speed * DT * 2.2f;
        if (p->cool > 0.0f) p->cool -= DT;
        if (p->think > 0.0f) p->think -= DT;
    }

    for (i = 0; i < 8; i++) {
        for (j = i + 1; j < 8; j++) {
            float dx = pl[j].x - pl[i].x;
            float dz = pl[j].z - pl[i].z;
            float d = sqrtf(dx * dx + dz * dz);
            if (d < 0.65f && d > 0.0001f) {
                float push = (0.65f - d) * 0.5f;
                float nx = dx / d, nz = dz / d;
                pl[i].x -= nx * push; pl[i].z -= nz * push;
                pl[j].x += nx * push; pl[j].z += nz * push;
            }
        }
    }
}

static void updateBallControl(void) {
    int i;
    float bs = sqrtf(ball.vx * ball.vx + ball.vz * ball.vz);

    for (i = 0; i < 8; i++) {
        Player *p = &pl[i];
        float dx, dz, d2;
        if (p->role == 0 || p->cool > 0.0f) continue;
        dx = ball.x - p->x;
        dz = ball.z - p->z;
        d2 = dx * dx + dz * dz;

        if (ball.owner == -1) {
            if (d2 < 0.64f && ball.y < 0.95f && bs < 18.0f) takeBall(i);
        } else if (ball.owner != i && pl[ball.owner].team != p->team) {
            if (d2 < 0.72f) {
                float chance = (p->slideT > 0.0f) ? 0.12f : 0.035f;
                if (pl[ball.owner].dribT > 0.0f) chance *= 0.1f;
                if (rnd01() < chance) {
                    int prev = ball.owner;
                    takeBall(i);
                    pl[prev].cool = 0.6f;
                }
            }
        }
    }

    /* goleiros: agarrar, espalmar ou roubar a bola dos pes do atacante */
    for (i = 0; i < 8; i += 4) {
        Player *p = &pl[i];
        float reach, hmax;
        if (p->cool > 0.0f) continue;
        reach = (p->diveT > 0.0f) ? 1.6f : 1.0f;
        hmax = (p->diveT > 0.0f) ? 2.0f : 1.6f;

        if (ball.owner >= 0 && pl[ball.owner].team != p->team) {
            if (p->diveT > 0.0f && distf(p->x, p->z, ball.x, ball.z) < 1.2f && pl[ball.owner].dribT <= 0.0f) {
                int prev = ball.owner;
                takeBall(i);
                pl[prev].cool = 0.7f;
                gkHold = 1.0f;
            }
            continue;
        }
        if (ball.owner != -1) continue;

        if (distf(p->x, p->z, ball.x, ball.z) < reach && ball.y < hmax) {
            float easy = (bs < 9.0f) ? 1.0f : (bs < 16.0f ? 0.6f : 0.15f);
            if (p->diveT > 0.0f) easy += 0.15f;
            if (rnd01() < easy) {
                takeBall(i);
                gkHold = 1.1f;
            } else {
                float side = (ball.x >= p->x) ? 1.0f : -1.0f;
                float away = (p->team == 0) ? -1.0f : 1.0f;
                ball.vz = away * (3.0f + rnd01() * 3.0f);
                ball.vx = side * rndRange(3.0f, 6.0f);
                ball.vy = 2.0f + rnd01() * 1.5f;
                ball.lastTeam = p->team;
                ball.lastTouch = i;
                p->cool = 0.5f;
            }
        }
    }
}

static void throwIn(void) {
    int t = 1 - ball.lastTeam;
    float sx = signf(ball.x) * (FIELD_X - 0.35f);
    float sz = clampf(ball.z, -FIELD_Z + 2.0f, FIELD_Z - 2.0f);
    int n = nearestOutfielder(t, sx, sz, -1);
    Player *p = &pl[n];
    p->x = sx - signf(sx) * 0.5f;
    p->z = sz;
    p->vx = 0.0f; p->vz = 0.0f; p->dvx = 0.0f; p->dvz = 0.0f;
    p->face = atan2f(-signf(sx), 0.0f);
    takeBall(n);
    ball.x = sx - signf(sx) * 0.45f;
    ball.z = sz;
    ball.y = BALL_R;
    ball.vx = 0.0f; ball.vy = 0.0f; ball.vz = 0.0f;
}

static void goalKick(void) {
    int team = (ball.z > 0.0f) ? 0 : 1;
    int k = team * 4;
    float sg = (team == 0) ? 1.0f : -1.0f;
    Player *p = &pl[k];
    p->x = 0.0f;
    p->z = sg * (FIELD_Z - 2.2f);
    p->vx = 0.0f; p->vz = 0.0f; p->dvx = 0.0f; p->dvz = 0.0f;
    p->diveT = 0.0f;
    p->face = (team == 0) ? PI : 0.0f;
    takeBall(k);
    gkHold = 1.2f;
    ball.x = 0.0f;
    ball.z = p->z - sg * 0.5f;
    ball.y = 0.6f;
    ball.vx = 0.0f; ball.vy = 0.0f; ball.vz = 0.0f;
}

static void goalScored(int team) {
    int sc = -1;
    score[team]++;
    scorerTeam = team;
    gState = ST_GOAL;
    stateT = GOAL_T;
    celebElapsed = 0.0f;
    ball.owner = -1;
    ball.passTarget = -1;
    charge = 0.0f;
    if (ball.lastTouch >= 0 && ball.lastTouch < 8 &&
        pl[ball.lastTouch].team == team && pl[ball.lastTouch].role != 0)
        sc = ball.lastTouch;
    if (sc < 0) sc = nearestOutfielder(team, ball.x, ball.z, -1);
    celebIdx = sc;
    if (sc >= 0) pl[sc].celebType = (int)(rnd01() * 2.99f);
}

static void ballVsGoalFrame(void) {
    float postX = signf(ball.x) * GOAL_HW;
    float postZ = signf(ball.z) * FIELD_Z;
    float nx = ball.x - postX, nz = ball.z - postZ;
    float d = sqrtf(nx * nx + nz * nz);

    if (d < 0.3f && d > 0.0001f && ball.y < GOAL_H + 0.1f) {
        float dot;
        nx /= d; nz /= d;
        dot = ball.vx * nx + ball.vz * nz;
        if (dot < 0.0f) {
            ball.vx -= 1.7f * dot * nx;
            ball.vz -= 1.7f * dot * nz;
        }
        ball.x = postX + nx * 0.3f;
        ball.z = postZ + nz * 0.3f;
    }

    if (fabsf(ball.x) < GOAL_HW && fabsf(ball.z - postZ) < 0.28f && fabsf(ball.y - GOAL_H) < 0.28f) {
        if (ball.y > GOAL_H) {
            ball.vy = fabsf(ball.vy) * 0.4f + 1.0f;
            ball.y = GOAL_H + 0.28f;
        } else {
            ball.vy = -fabsf(ball.vy) * 0.4f;
            ball.y = GOAL_H - 0.28f;
        }
        ball.vz *= 0.5f;
    }
}

static void updateBall(void) {
    float f;

    if (ball.owner >= 0) {
        Player *o = &pl[ball.owner];
        float fx = sinf(o->face), fz = cosf(o->face);
        float tx = o->x + fx * 0.52f, tz = o->z + fz * 0.52f;
        ball.x += (tx - ball.x) * 0.55f;
        ball.z += (tz - ball.z) * 0.55f;
        ball.y = (o->role == 0) ? 0.6f : BALL_R;
        ball.vx = o->vx;
        ball.vz = o->vz;
        ball.vy = 0.0f;
    } else {
        ball.vy -= GRAV * DT;
        ball.x += ball.vx * DT;
        ball.y += ball.vy * DT;
        ball.z += ball.vz * DT;

        if (ball.y <= BALL_R) {
            ball.y = BALL_R;
            if (ball.vy < -1.2f) ball.vy = -ball.vy * 0.55f;
            else ball.vy = 0.0f;
            f = 1.0f - 1.15f * DT;
            ball.vx *= f;
            ball.vz *= f;
            if (sqrtf(ball.vx * ball.vx + ball.vz * ball.vz) < 0.12f) {
                ball.vx = 0.0f;
                ball.vz = 0.0f;
            }
        } else {
            f = 1.0f - 0.2f * DT;
            ball.vx *= f;
            ball.vz *= f;
        }

        if (ball.passT > 0.0f) {
            ball.passT -= DT;
            if (ball.passT <= 0.0f) ball.passTarget = -1;
        }
    }

    ball.spinX += ball.vz * DT / BALL_R;
    ball.spinZ -= ball.vx * DT / BALL_R;

    if (gState == ST_GOAL) {
        /* rede do gol: mantem a bola dentro */
        if (fabsf(ball.z) > FIELD_Z + GOAL_DEPTH - 0.25f) {
            ball.z = signf(ball.z) * (FIELD_Z + GOAL_DEPTH - 0.25f);
            ball.vz *= -0.25f;
        }
        if (fabsf(ball.x) > GOAL_HW - 0.2f) {
            ball.x = signf(ball.x) * (GOAL_HW - 0.2f);
            ball.vx *= -0.3f;
        }
        return;
    }

    ballVsGoalFrame();

    if (fabsf(ball.z) > FIELD_Z + 0.1f) {
        if (fabsf(ball.x) < GOAL_HW - 0.1f && ball.y < GOAL_H - 0.12f)
            goalScored(ball.z > 0.0f ? 1 : 0);
        else
            goalKick();
    } else if (fabsf(ball.x) > FIELD_X + 0.15f) {
        throwIn();
    }
}

/* ---------------------------------------------------------------------
 * Partida: formacao, saida de bola, atualizacao geral
 * ------------------------------------------------------------------- */
static int colDiff(unsigned int a, unsigned int b) {
    int dr = (int)(a & 0xFFu) - (int)(b & 0xFFu);
    int dg = (int)((a >> 8) & 0xFFu) - (int)((b >> 8) & 0xFFu);
    int db = (int)((a >> 16) & 0xFFu) - (int)((b >> 16) & 0xFFu);
    return dr * dr + dg * dg + db * db;
}

static int kitClash(const Kit *a, const Kit *b) {
    return colDiff(a->shirt, b->shirt) < 9000;
}

static void setupPlayers(void) {
    static const float hx[4] = { 0.0f, 0.0f, -4.5f, 4.5f };
    static const float hz[4] = { 15.8f, 9.0f, 3.8f, 3.8f };
    int i;
    for (i = 0; i < 8; i++) {
        Player *p = &pl[i];
        float sg;
        memset(p, 0, sizeof(*p));
        p->team = i / 4;
        p->role = i % 4;
        sg = (p->team == 0) ? 1.0f : -1.0f;
        p->homeX = hx[p->role];
        p->homeZ = hz[p->role] * sg;
        p->x = p->homeX;
        p->z = p->homeZ;
        p->face = (p->team == 0) ? PI : 0.0f;
        p->stamina = 1.0f;
        p->skin = (i * 3 + 1) & 3;
        p->hair = (i * 5 + 2) & 3;
        {
            static const int nums[4] = { 1, 5, 10, 9 };
            p->number = nums[p->role];
        }
        if (p->role == 0) kits[i] = &gkKits[p->team];
        else if (p->team == 1 && kitClash(&teams[teamOf[0]].kit, &teams[teamOf[1]].kit)) kits[i] = &teams[teamOf[1]].alt;
        else kits[i] = &teams[teamOf[p->team]].kit;
    }
}

static void kickoff(int kickTeam) {
    int i, k;
    float sg = (kickTeam == 0) ? 1.0f : -1.0f;
    for (i = 0; i < 8; i++) {
        Player *p = &pl[i];
        p->x = p->homeX;
        p->z = p->homeZ;
        p->vx = 0.0f; p->vz = 0.0f; p->dvx = 0.0f; p->dvz = 0.0f;
        p->diveT = 0.0f; p->slideT = 0.0f;
        p->cool = 0.0f; p->think = 0.0f;
        p->celebT = 0.0f; p->jump = 0.0f; p->dribT = 0.0f; p->spinA = 0.0f;
        p->kickT = 0.0f; p->oHold = 0.0f; p->diveH = 0.0f;
        p->face = (p->team == 0) ? PI : 0.0f;
    }
    celebIdx = -1;
    k = kickTeam * 4 + 2;
    pl[k].x = 0.0f;
    pl[k].z = sg * 0.7f;

    ball.x = 0.0f;
    ball.y = BALL_R;
    ball.z = sg * 0.7f - sg * 0.52f;
    ball.vx = 0.0f; ball.vy = 0.0f; ball.vz = 0.0f;
    takeBall(k);
    ctrl = 2;
    lastOwner = ball.owner;
    gkHold = 0.0f;
    charge = 0.0f;
    switchT = 0.5f;
}

static void startMatch(void) {
    teamOf[0] = selHome;
    teamOf[1] = selAway;
    setupPlayers();
    score[0] = 0;
    score[1] = 0;
    timeLeft = MATCH_TIME;
    rngState ^= (unsigned int)frameCount * 2654435761u;
    memset(&ball, 0, sizeof(ball));
    kickoff(0);
    ball.owner = -1;
    ball.passTarget = -1;
    {
        int i;
        for (i = 0; i < 8; i++) {
            Player *p = &pl[i];
            float sg = (p->team == 0) ? 1.0f : -1.0f;
            p->introX = p->x;
            p->introZ = p->z;
            p->x = FIELD_X + 0.6f;               /* entram pela lateral, em fila */
            p->z = sg * (2.6f + (float)p->role * 1.0f);
            p->face = -PI * 0.5f;
        }
    }
    camX = 0.0f;
    camZ = 0.0f;
    gState = ST_INTRO;
    stateT = INTRO_T;
}

static void updateGame(const SceCtrlData *pad) {
    int i;

    switch (gState) {
    case ST_INTRO: {
        float el = INTRO_T - stateT;
        stateT -= DT;
        for (i = 0; i < 8; i++) {
            Player *p = &pl[i];
            float delay = 0.5f + (float)p->role * 0.4f + (p->team ? 0.2f : 0.0f);
            p->dvx = 0.0f; p->dvz = 0.0f;
            if (el >= delay) moveToward(p, p->introX, p->introZ, 3.4f);
        }
        updatePlayers();
        updateBall();
        updateCamera();
        if (stateT <= 0.0f) {
            takeBall(2);
            lastOwner = 2;
            ctrl = 2;
            gState = ST_PLAY;
        }
        return;
    }

    case ST_PAUSE:
        if (gPressed & PSP_CTRL_START) gState = ST_PLAY;
        if (gPressed & PSP_CTRL_SELECT) gState = ST_MENU;
        return;

    case ST_END:
        if (gPressed & PSP_CTRL_CROSS) gState = ST_MENU;
        return;

    case ST_GOAL: {
        float el;
        stateT -= DT;
        celebElapsed += DT;
        el = celebElapsed;
        for (i = 0; i < 8; i++) { pl[i].dvx = 0.0f; pl[i].dvz = 0.0f; }
        if (celebIdx >= 0) {
            Player *sc = &pl[celebIdx];
            float dirZ = (sc->team == 0) ? -1.0f : 1.0f;
            if (el < 1.3f) {
                moveToward(sc, signf(sc->x + 0.001f) * 6.5f, clampf(sc->z - dirZ * 5.0f, -12.0f, 12.0f), 6.4f);
            } else if (el - DT < 1.3f) {
                sc->celebT = 2.8f;
            }
            for (i = 0; i < 8; i++) {
                Player *p = &pl[i];
                if (i == celebIdx || p->team != sc->team || p->role == 0) continue;
                if (el > 0.5f && p->think <= 0.0f) {
                    moveToward(p, sc->x + (float)(p->role - 2) * 1.2f, sc->z + 1.1f, 5.6f);
                    if (distf(p->x, p->z, sc->x, sc->z) < 2.3f) {
                        p->celebT = 2.2f;
                        p->celebType = 2;
                        p->think = 10.0f;
                    }
                }
            }
        }
        updatePlayers();
        updateBall();
        updateCamera();
        if (stateT <= 0.0f && gState == ST_GOAL) {
            kickoff(1 - scorerTeam);
            gState = ST_PLAY;
        }
        return;
    }

    default:
        break;
    }

    /* ST_PLAY */
    if (gPressed & PSP_CTRL_START) { gState = ST_PAUSE; return; }

    timeLeft -= DT;
    if (timeLeft <= 0.0f) {
        timeLeft = 0.0f;
        gState = ST_END;
        return;
    }

    if (ball.owner != lastOwner) {
        lastOwner = ball.owner;
        if (ball.owner >= 1 && ball.owner < 4) { ctrl = ball.owner; charge = 0.0f; }
    }
    {
        int ot = (ball.owner >= 0) ? pl[ball.owner].team : -1;
        switchT -= DT;
        if (ot != 0 && switchT <= 0.0f) {
            int n = nearestOutfielder(0, ball.x, ball.z, -1);
            if (n >= 0 && n != ctrl &&
                distf(pl[n].x, pl[n].z, ball.x, ball.z) + 1.2f < distf(pl[ctrl].x, pl[ctrl].z, ball.x, ball.z)) {
                ctrl = n;
                charge = 0.0f;
            }
            switchT = 0.4f;
        }
    }

    updateUser(pad);
    updateAI();
    updatePlayers();
    updateBallControl();
    updateBall();
    updateCamera();
}

static void updateMenu(void) {
    menuAngle += 0.025f;

    switch (gState) {
    case ST_MENU:
        if (gPressed & PSP_CTRL_UP)   menuSel = (menuSel + 2) % 3;
        if (gPressed & PSP_CTRL_DOWN) menuSel = (menuSel + 1) % 3;
        if (gPressed & PSP_CTRL_CROSS) {
            if (menuSel == 0) gState = ST_SEL_HOME;
            else if (menuSel == 1) gState = ST_CONTROLS;
            else running = 0;
        }
        break;

    case ST_CONTROLS:
        if (gPressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) gState = ST_MENU;
        break;

    case ST_SEL_HOME:
        if (gPressed & PSP_CTRL_LEFT)  selHome = (selHome + 3) & 3;
        if (gPressed & PSP_CTRL_RIGHT) selHome = (selHome + 1) & 3;
        if (gPressed & PSP_CTRL_CROSS) {
            if (selAway == selHome) selAway = (selHome + 1) & 3;
            gState = ST_SEL_AWAY;
        }
        if (gPressed & PSP_CTRL_CIRCLE) gState = ST_MENU;
        break;

    case ST_SEL_AWAY:
        if (gPressed & PSP_CTRL_LEFT) {
            selAway = (selAway + 3) & 3;
            if (selAway == selHome) selAway = (selAway + 3) & 3;
        }
        if (gPressed & PSP_CTRL_RIGHT) {
            selAway = (selAway + 1) & 3;
            if (selAway == selHome) selAway = (selAway + 1) & 3;
        }
        if (gPressed & PSP_CTRL_CROSS) startMatch();
        if (gPressed & PSP_CTRL_CIRCLE) gState = ST_SEL_HOME;
        break;

    default:
        break;
    }
}

static int isMenuState(void) {
    return gState == ST_MENU || gState == ST_CONTROLS ||
           gState == ST_SEL_HOME || gState == ST_SEL_AWAY;
}

/* ---------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------- */
int main(void) {
    SceCtrlData pad;
    unsigned int oldButtons = 0;

    setupCallbacks();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    init3D();
    initBallMesh();

    memset(&pad, 0, sizeof(pad));
    memset(&ball, 0, sizeof(ball));
    ball.owner = -1;
    ball.passTarget = -1;
    setupPlayers();

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);
        gPressed = pad.Buttons & ~oldButtons;
        gReleased = ~pad.Buttons & oldButtons;
        oldButtons = pad.Buttons;
        frameCount++;

        if (isMenuState()) updateMenu();
        else updateGame(&pad);

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFFB0D8F0);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (isMenuState()) drawMenu();
        else drawGame();

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
