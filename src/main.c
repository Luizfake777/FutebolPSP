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
    int stripes;
} Kit;

typedef struct {
    const char *name;
    const char *abbr;
    Kit kit;
} Team;

typedef struct {
    float x, z, vx, vz;
    float dvx, dvz;
    float face, anim, speed;
    float cool, diveT, diveDir, slideT, slx, slz;
    float homeX, homeZ, think, stamina;
    float aimX, aimZ;
    int team, role, skin;
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

/* ---------------------------------------------------------------------
 * Dados estaticos
 * ------------------------------------------------------------------- */
static const Team teams[4] = {
    { "BRASIL",    "BRA", { RGB(250, 214, 0),   RGB(250, 214, 0),   RGB(0, 70, 170),    RGB(240, 240, 240), 0 } },
    { "ARGENTINA", "ARG", { RGB(120, 190, 240), RGB(245, 245, 245), RGB(25, 25, 25),    RGB(25, 25, 25),    1 } },
    { "FRANCA",    "FRA", { RGB(25, 55, 155),   RGB(25, 55, 155),   RGB(245, 245, 245), RGB(200, 30, 45),   0 } },
    { "ESPANHA",   "ESP", { RGB(205, 25, 35),   RGB(205, 25, 35),   RGB(20, 40, 125),   RGB(20, 40, 125),   0 } }
};

static const Kit gkKits[2] = {
    { RGB(30, 170, 60),  RGB(30, 170, 60),  RGB(20, 20, 20), RGB(20, 20, 20), 0 },
    { RGB(245, 130, 20), RGB(245, 130, 20), RGB(20, 20, 20), RGB(20, 20, 20), 0 }
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
static void boxRaw(float hx, float hy, float hz, unsigned int c) {
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
            v[n].x = (float)cc[id][0] * hx;
            v[n].y = (float)cc[id][1] * hy;
            v[n].z = (float)cc[id][2] * hz;
            n++;
        }
    }
    sceGumDrawArray(GU_TRIANGLES, VTX_3D, 36, 0, v);
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
    sceGumPushMatrix();
    tr(x, y, z);
    boxRaw(hx, hy, hz, c);
    sceGumPopMatrix();
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
static void drawPlayer(const Player *p, const Kit *k) {
    float sp = clampf(p->speed / 6.0f, 0.0f, 1.0f);
    float sw = sinf(p->anim) * 0.95f * sp;
    float lean = 0.18f * sp;
    unsigned int skin = skinTones[p->skin & 3];
    unsigned int hair = hairTones[p->skin & 3];
    float s;
    int j;
    ScePspFVector3 sc;

    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();
    tr(p->x, 0.0f, p->z);
    sceGumRotateY(p->face);
    sc.x = 1.2f; sc.y = 1.2f; sc.z = 1.2f;
    sceGumScale(&sc);

    if (p->diveT > 0.0f) {
        float ld = p->diveDir * cosf(p->face);
        tr(0.0f, 0.4f, 0.0f);
        sceGumRotateZ(-ld * 1.3f);
    }
    if (p->slideT > 0.0f) {
        tr(0.0f, 0.12f, 0.0f);
        sceGumRotateX(-1.2f);
    }

    /* pernas */
    for (s = -1.0f; s <= 1.0f; s += 2.0f) {
        sceGumPushMatrix();
        tr(s * 0.11f, 0.5f, 0.0f);
        sceGumRotateX(s * sw);
        partBox(0.0f, -0.085f, 0.0f, 0.095f, 0.085f, 0.10f, k->shorts);
        partBox(0.0f, -0.34f, 0.0f, 0.07f, 0.165f, 0.075f, k->socks);
        partBox(0.0f, -0.47f, 0.035f, 0.08f, 0.03f, 0.12f, RGB(25, 25, 25));
        sceGumPopMatrix();
    }

    /* tronco, bracos e cabeca */
    sceGumPushMatrix();
    tr(0.0f, 0.5f, 0.0f);
    sceGumRotateX(lean);

    partBox(0.0f, 0.04f, 0.0f, 0.20f, 0.06f, 0.11f, k->shorts);
    for (j = 0; j < 3; j++) {
        unsigned int c = (k->stripes && (j & 1)) ? k->shirt2 : k->shirt;
        partBox(-0.1533f + (float)j * 0.1533f, 0.27f, 0.0f, 0.0767f, 0.17f, 0.12f, c);
    }
    for (s = -1.0f; s <= 1.0f; s += 2.0f) {
        sceGumPushMatrix();
        tr(s * 0.30f, 0.40f, 0.0f);
        sceGumRotateX(-s * sw);
        partBox(0.0f, -0.07f, 0.0f, 0.05f, 0.07f, 0.055f, k->shirt);
        partBox(0.0f, -0.22f, 0.0f, 0.04f, 0.09f, 0.045f, skin);
        sceGumPopMatrix();
    }
    partBox(0.0f, 0.58f, 0.0f, 0.11f, 0.12f, 0.11f, skin);
    partBox(0.0f, 0.69f, -0.01f, 0.115f, 0.04f, 0.115f, hair);
    partBox(-0.045f, 0.60f, 0.112f, 0.02f, 0.02f, 0.01f, RGB(20, 20, 20));
    partBox( 0.045f, 0.60f, 0.112f, 0.02f, 0.02f, 0.01f, RGB(20, 20, 20));

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
static void drawHUD(void) {
    char buf[48];
    char tb[8];
    const Team *ta = &teams[teamOf[0]];
    const Team *tbm = &teams[teamOf[1]];

    begin2D();
    rect2d(150.0f, 4.0f, 180.0f, 26.0f, RGBA(0, 0, 0, 170));
    rect2d(150.0f, 4.0f, 6.0f, 26.0f, ta->kit.shirt);
    rect2d(324.0f, 4.0f, 6.0f, 26.0f, tbm->kit.shirt);

    strcpy(buf, ta->abbr);
    strcat(buf, " ");
    appendInt(buf, score[0]);
    strcat(buf, "-");
    appendInt(buf, score[1]);
    strcat(buf, " ");
    strcat(buf, tbm->abbr);
    drawTextS(240.0f - textW(buf, 2.0f) * 0.5f, 10.0f, 2.0f, RGB(255, 255, 255), buf);

    rect2d(205.0f, 32.0f, 70.0f, 20.0f, RGBA(0, 0, 0, 170));
    fmtTime(tb, (int)ceilf(timeLeft));
    drawTextS(210.0f, 35.0f, 2.0f, RGB(255, 235, 120), tb);

    if (gState == ST_PLAY) {
        rect2d(10.0f, 252.0f, 64.0f, 8.0f, RGBA(0, 0, 0, 150));
        rect2d(11.0f, 253.0f, 62.0f * clampf(pl[ctrl].stamina, 0.0f, 1.0f), 6.0f, RGB(80, 200, 255));
        drawTextS(10.0f, 240.0f, 1.0f, RGB(255, 255, 255), "FOLEGO");
        if (charge > 0.02f) {
            rect2d(190.0f, 250.0f, 100.0f, 10.0f, RGBA(0, 0, 0, 150));
            rect2d(192.0f, 252.0f, 96.0f * charge, 6.0f,
                   RGB((int)(80 + 175 * charge), (int)(220 - 150 * charge), 40));
            drawTextS(212.0f, 238.0f, 1.0f, RGB(255, 255, 255), "FORCA");
        }
    }
    end2D();
}

static void drawGameOverlays(void) {
    char buf[48];
    if (gState == ST_INTRO) {
        begin2D();
        rect2d(0.0f, 96.0f, (float)SW, 70.0f, RGBA(0, 0, 0, 150));
        strcpy(buf, teams[teamOf[0]].name);
        strcat(buf, " X ");
        strcat(buf, teams[teamOf[1]].name);
        drawTextC(106.0f, 3.0f, RGB(255, 255, 255), buf);
        drawTextC(138.0f, 2.0f, RGB(255, 235, 120), "PREPARE-SE!");
        end2D();
    } else if (gState == ST_GOAL) {
        begin2D();
        drawTextC(70.0f, 9.0f, RGB(255, 235, 40), "GOL!");
        drawTextC(150.0f, 3.0f, RGB(255, 255, 255), teams[teamOf[scorerTeam]].name);
        end2D();
    } else if (gState == ST_PAUSE) {
        begin2D();
        rect2d(0.0f, 0.0f, (float)SW, (float)SH, RGBA(0, 0, 0, 150));
        drawTextC(80.0f, 5.0f, RGB(255, 255, 255), "PAUSE");
        drawTextC(150.0f, 2.0f, RGB(255, 235, 120), "START: CONTINUAR");
        drawTextC(180.0f, 2.0f, RGB(255, 235, 120), "SELECT: SAIR DA PARTIDA");
        end2D();
    } else if (gState == ST_END) {
        begin2D();
        rect2d(0.0f, 60.0f, (float)SW, 150.0f, RGBA(0, 0, 0, 170));
        drawTextC(70.0f, 4.0f, RGB(255, 235, 40), "FIM DE JOGO");
        strcpy(buf, teams[teamOf[0]].abbr);
        strcat(buf, " ");
        appendInt(buf, score[0]);
        strcat(buf, "-");
        appendInt(buf, score[1]);
        strcat(buf, " ");
        strcat(buf, teams[teamOf[1]].abbr);
        drawTextC(115.0f, 4.0f, RGB(255, 255, 255), buf);
        if (score[0] == score[1]) {
            drawTextC(160.0f, 3.0f, RGB(255, 255, 255), "EMPATE");
        } else {
            strcpy(buf, "VITORIA: ");
            strcat(buf, teams[teamOf[score[0] > score[1] ? 0 : 1]].name);
            drawTextC(160.0f, 3.0f, RGB(255, 255, 255), buf);
        }
        drawTextC(188.0f, 2.0f, RGB(255, 235, 120), "X: MENU");
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
static void drawMenuModel(const Kit *k, float ox, int skin) {
    Player mp;
    ScePspFVector3 eye, ctr, up;
    memset(&mp, 0, sizeof(mp));
    mp.x = ox;
    mp.face = menuAngle;
    mp.speed = 3.2f;
    mp.anim = (float)frameCount * 0.17f;
    mp.skin = skin;

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(50.0f, 16.0f / 9.0f, 0.5f, 50.0f);

    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    eye.x = 0.0f; eye.y = 1.0f; eye.z = 2.7f;
    ctr.x = 0.0f; ctr.y = 0.65f; ctr.z = 0.0f;
    up.x = 0.0f; up.y = 1.0f; up.z = 0.0f;
    sceGumLookAt(&eye, &ctr, &up);

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuEnable(GU_DEPTH_TEST);
    drawPlayer(&mp, k);
}

static void drawMenu(void) {
    static const char *items[3] = { "JOGAR", "CONTROLES", "SAIR" };
    static const char *ctl[7] = {
        "ANALOGICO OU DIRECIONAL: MOVER",
        "QUADRADO OU R: CORRER",
        "X: SEGURE E SOLTE PARA CHUTAR",
        "O: PASSE OU DESARME",
        "TRIANGULO: PASSE ALTO OU TROCAR",
        "L: TROCAR JOGADOR",
        "START: PAUSAR"
    };
    int i;
    char buf[32];

    begin2D();
    rectGrad(0.0f, 0.0f, (float)SW, (float)SH, RGB(8, 70, 30), RGB(2, 22, 10));
    for (i = 0; i < 6; i++)
        rect2d(0.0f, 40.0f + (float)i * 46.0f, (float)SW, 2.0f, RGBA(255, 255, 255, 18));
    end2D();

    switch (gState) {
    case ST_MENU:
        drawMenuModel(&teams[(frameCount / 150) & 3].kit, 1.15f, (frameCount / 150) & 3);
        begin2D();
        drawTextS(30.0f, 30.0f, 5.0f, RGB(255, 235, 40), "FUTEBOL PSP");
        for (i = 0; i < 3; i++) {
            unsigned int col = (i == menuSel) ? RGB(255, 235, 40) : RGB(230, 230, 230);
            if (i == menuSel) drawTextS(30.0f, 120.0f + (float)i * 34.0f, 3.0f, col, ">");
            drawTextS(60.0f, 120.0f + (float)i * 34.0f, 3.0f, col, items[i]);
        }
        drawTextS(30.0f, 250.0f, 1.0f, RGB(200, 220, 200), "CIMA BAIXO: ESCOLHER   X: CONFIRMAR");
        drawTextS(320.0f, 232.0f, 2.0f, RGB(255, 255, 255), teams[(frameCount / 150) & 3].abbr);
        end2D();
        break;

    case ST_CONTROLS:
        begin2D();
        drawTextC(16.0f, 4.0f, RGB(255, 235, 40), "CONTROLES");
        for (i = 0; i < 7; i++)
            drawTextS(40.0f, 70.0f + (float)i * 26.0f, 2.0f, RGB(255, 255, 255), ctl[i]);
        drawTextC(252.0f, 1.0f, RGB(200, 220, 200), "X OU O: VOLTAR");
        end2D();
        break;

    case ST_SEL_HOME:
    case ST_SEL_AWAY: {
        int sel = (gState == ST_SEL_HOME) ? selHome : selAway;
        drawMenuModel(&teams[sel].kit, 0.0f, sel);
        begin2D();
        drawTextC(14.0f, 4.0f, RGB(255, 235, 40), gState == ST_SEL_HOME ? "SEU TIME" : "ADVERSARIO");
        drawTextS(30.0f, 100.0f, 6.0f, RGB(255, 255, 255), "<");
        drawTextS((float)SW - 66.0f, 100.0f, 6.0f, RGB(255, 255, 255), ">");
        drawTextC(200.0f, 4.0f, RGB(255, 255, 255), teams[sel].name);
        if (gState == ST_SEL_AWAY) {
            strcpy(buf, teams[selHome].abbr);
            strcat(buf, " X ");
            strcat(buf, teams[sel].abbr);
            drawTextC(236.0f, 2.0f, RGB(255, 235, 120), buf);
        }
        drawTextS(10.0f, 258.0f, 1.0f, RGB(200, 220, 200), "ESQ DIR: ESCOLHER   X: CONFIRMAR   O: VOLTAR");
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
    float lineZ = own - sgn * 1.3f;
    float tx = clampf(ball.x * 0.45f, -2.0f, 2.0f);
    float spd = 4.6f;

    if (ball.owner == i) {
        p->dvx = 0.0f; p->dvz = 0.0f;
        gkHold -= DT;
        if (gkHold <= 0.0f) {
            int m = bestMate(i, 0.0f, 0.0f, 0);
            if (m >= 0) doPass(i, m, 1);
        }
        return;
    }

    if (ball.owner < 0 && ball.vz * sgn > 1.5f &&
        sqrtf(ball.vx * ball.vx + ball.vz * ball.vz) > 6.0f) {
        float th = (own - ball.z) / ball.vz;
        if (th > 0.0f && th < 1.2f) {
            float px;
            if (p->aimZ == 0.0f) {
                p->aimX = rndRange(-0.7f, 0.7f);
                p->aimZ = 1.0f;
            }
            px = ball.x + ball.vx * th + p->aimX;
            tx = clampf(px, -2.3f, 2.3f);
            spd = 5.5f;
            if (fabsf(tx - p->x) > 0.9f && th < 0.35f && p->diveT <= 0.0f && p->cool <= 0.0f) {
                p->diveT = 0.55f;
                p->diveDir = signf(tx - p->x);
            }
        }
    } else {
        p->aimZ = 0.0f;
    }
    moveToward(p, tx, lineZ, spd);
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

static void updateUser(const SceCtrlData *pad) {
    Player *c = &pl[ctrl];
    float mx = 0.0f, mz = 0.0f, mag, spd = 4.4f;
    float ax = ((float)((int)pad->Lx - 128)) / 128.0f;
    float ay = ((float)((int)pad->Ly - 128)) / 128.0f;
    int wantSprint;

    if (pad->Buttons & PSP_CTRL_LEFT)  mx -= 1.0f;
    if (pad->Buttons & PSP_CTRL_RIGHT) mx += 1.0f;
    if (pad->Buttons & PSP_CTRL_UP)    mz -= 1.0f;
    if (pad->Buttons & PSP_CTRL_DOWN)  mz += 1.0f;
    if (fabsf(ax) > 0.22f) mx += ax;
    if (fabsf(ay) > 0.22f) mz += ay;

    mag = sqrtf(mx * mx + mz * mz);
    if (mag > 1.0f) { mx /= mag; mz /= mag; mag = 1.0f; }

    wantSprint = (pad->Buttons & (PSP_CTRL_SQUARE | PSP_CTRL_RTRIGGER)) != 0;
    if (wantSprint && c->stamina > 0.02f && mag > 0.3f) {
        spd = 6.3f;
        c->stamina -= 0.28f * DT;
    } else {
        c->stamina += 0.12f * DT;
    }
    c->stamina = clampf(c->stamina, 0.0f, 1.0f);

    if (c->slideT <= 0.0f) {
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

    if (pad->Buttons & PSP_CTRL_CROSS) charge = clampf(charge + DT / 0.85f, 0.0f, 1.0f);
    if (gReleased & PSP_CTRL_CROSS) {
        if (canKick(ctrl)) doShoot(ctrl, charge, mx);
        charge = 0.0f;
    } else if (!(pad->Buttons & PSP_CTRL_CROSS)) {
        charge = 0.0f;
    }

    if (gPressed & PSP_CTRL_CIRCLE) {
        if (canKick(ctrl)) {
            int m = bestMate(ctrl, c->aimX, c->aimZ, 1);
            if (m >= 0) doPass(ctrl, m, 0);
        } else if (c->think <= 0.0f && c->slideT <= 0.0f) {
            c->slideT = 0.4f;
            c->slx = c->aimX;
            c->slz = c->aimZ;
            c->think = 1.2f;
        }
    }
    if (gPressed & PSP_CTRL_TRIANGLE) {
        if (canKick(ctrl)) {
            int m = bestMate(ctrl, c->aimX, c->aimZ, 1);
            if (m >= 0) doPass(ctrl, m, 1);
        } else {
            switchPlayer();
        }
    }
    if (gPressed & PSP_CTRL_LTRIGGER) switchPlayer();
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
        if (p->diveT > 0.0f) {
            p->diveT -= DT;
            dvx += p->diveDir * 5.5f;
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
                if (rnd01() < chance) {
                    int prev = ball.owner;
                    takeBall(i);
                    pl[prev].cool = 0.6f;
                }
            }
        }
    }

    /* goleiros: defesa ou agarrar */
    for (i = 0; i < 8; i += 4) {
        Player *p = &pl[i];
        float reach, hmax;
        if (ball.owner != -1 || p->cool > 0.0f) continue;
        reach = (p->diveT > 0.0f) ? 1.5f : 0.95f;
        hmax = (p->diveT > 0.0f) ? 1.9f : 1.5f;
        if (distf(p->x, p->z, ball.x, ball.z) < reach && ball.y < hmax) {
            if (bs > 12.5f) {
                ball.vz = -ball.vz * 0.45f;
                ball.vx = ball.vx * 0.5f + rndRange(-3.0f, 3.0f);
                ball.vy = 2.0f;
                ball.lastTeam = p->team;
                ball.lastTouch = i;
                p->cool = 0.5f;
            } else {
                takeBall(i);
                gkHold = 1.2f;
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
    score[team]++;
    scorerTeam = team;
    gState = ST_GOAL;
    stateT = 3.0f;
    ball.owner = -1;
    ball.passTarget = -1;
    charge = 0.0f;
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
        kits[i] = (p->role == 0) ? &gkKits[p->team] : &teams[teamOf[p->team]].kit;
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
        p->face = (p->team == 0) ? PI : 0.0f;
    }
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
    camX = 0.0f;
    camZ = 0.0f;
    gState = ST_INTRO;
    stateT = 2.5f;
}

static void updateGame(const SceCtrlData *pad) {
    int i;

    switch (gState) {
    case ST_INTRO:
        stateT -= DT;
        for (i = 0; i < 8; i++) { pl[i].dvx = 0.0f; pl[i].dvz = 0.0f; }
        updatePlayers();
        updateBall();
        updateCamera();
        if (stateT <= 0.0f) gState = ST_PLAY;
        return;

    case ST_PAUSE:
        if (gPressed & PSP_CTRL_START) gState = ST_PLAY;
        if (gPressed & PSP_CTRL_SELECT) gState = ST_MENU;
        return;

    case ST_END:
        if (gPressed & PSP_CTRL_CROSS) gState = ST_MENU;
        return;

    case ST_GOAL:
        stateT -= DT;
        for (i = 0; i < 8; i++) { pl[i].dvx = 0.0f; pl[i].dvz = 0.0f; }
        updatePlayers();
        updateBall();
        updateCamera();
        if (stateT <= 0.0f && gState == ST_GOAL) {
            kickoff(1 - scorerTeam);
            gState = ST_PLAY;
        }
        return;

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
