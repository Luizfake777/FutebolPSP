/*
 * FutebolPSP - prototipo original de futebol para PSP
 * Tela inicial: campo 2D visto de cima, controles, bola e placar.
 * Projeto educacional; nao usa assets ou codigo proprietario de PES.
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("FutebolPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_WIDTH 512
#define PI 3.14159265358979323846f

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) fb0[BUF_WIDTH * SCREEN_H];
static unsigned int __attribute__((aligned(16))) fb1[BUF_WIDTH * SCREEN_H];

typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

static float playerX = 120.0f, playerY = 136.0f;
static float opponentX = 350.0f, opponentY = 136.0f;
static float ballX = 240.0f, ballY = 136.0f;
static float ballVX = 0.0f, ballVY = 0.0f;
static int scoreHome = 0, scoreAway = 0;
static int matchFrames = 0;
static int lastGoalFrames = 0;

static unsigned int rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return ((unsigned int)a << 24) | ((unsigned int)b << 16) | ((unsigned int)g << 8) | r;
}

static void drawTriangle(Vertex a, Vertex b, Vertex c) {
    Vertex v[3] = {a,b,c};
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   3, 0, v);
}

static void rect(float x, float y, float w, float h, unsigned int color) {
    Vertex a = {color, (short)x, (short)y, 0};
    Vertex b = {color, (short)(x+w), (short)y, 0};
    Vertex c = {color, (short)(x+w), (short)(y+h), 0};
    Vertex d = {color, (short)x, (short)(y+h), 0};
    drawTriangle(a,b,c);
    drawTriangle(a,c,d);
}

static void circle(float cx, float cy, float radius, unsigned int color) {
    int i;
    for (i = 0; i < 12; i++) {
        float a1 = (2.0f * PI * i) / 12.0f;
        float a2 = (2.0f * PI * (i+1)) / 12.0f;
        Vertex center = {color, (short)cx, (short)cy, 0};
        Vertex p1 = {color, (short)(cx + cosf(a1)*radius), (short)(cy + sinf(a1)*radius), 0};
        Vertex p2 = {color, (short)(cx + cosf(a2)*radius), (short)(cy + sinf(a2)*radius), 0};
        drawTriangle(center,p1,p2);
    }
}

static void line(float x1, float y1, float x2, float y2, unsigned int color, float thickness) {
    float dx = x2-x1, dy = y2-y1;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.1f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;
    Vertex a = {color, (short)(x1+nx), (short)(y1+ny), 0};
    Vertex b = {color, (short)(x2+nx), (short)(y2+ny), 0};
    Vertex c = {color, (short)(x2-nx), (short)(y2-ny), 0};
    Vertex d = {color, (short)(x1-nx), (short)(y1-ny), 0};
    drawTriangle(a,b,c); drawTriangle(a,c,d);
}

static void drawField(void) {
    unsigned int grass1 = rgba(38,132,58,255);
    unsigned int grass2 = rgba(33,119,51,255);
    unsigned int white = rgba(238,245,238,255);
    int i;
    rect(0,0,SCREEN_W,SCREEN_H,rgba(13,24,24,255));
    rect(18,24,444,224,grass1);
    for (i=0; i<8; i++) {
        if (i%2==0) rect(18+i*55.5f,24,55.5f,224,grass2);
    }
    /* Linhas do campo */
    line(18,24,462,24,white,2);
    line(18,248,462,248,white,2);
    line(18,24,18,248,white,2);
    line(462,24,462,248,white,2);
    line(240,24,240,248,white,2);
    /* Circulo central aproximado */
    for (i=0; i<36; i++) {
        float a1 = 2.0f*PI*i/36.0f;
        float a2 = 2.0f*PI*(i+1)/36.0f;
        line(240+32*cosf(a1),136+32*sinf(a1),
             240+32*cosf(a2),136+32*sinf(a2),white,1.5f);
    }
    /* Areas e pequenas traves */
    line(18,82,72,82,white,2); line(72,82,72,190,white,2);
    line(72,190,18,190,white,2);
    line(462,82,408,82,white,2); line(408,82,408,190,white,2);
    line(408,190,462,190,white,2);
    rect(5,112,13,48,rgba(220,220,220,255));
    rect(462,112,13,48,rgba(220,220,220,255));
    circle(240,136,2,white);
}

static void drawScene(void) {
    drawField();
    /* Jogador local: camisa amarela; adversario: camisa vermelha */
    circle(playerX,playerY,8,rgba(16,34,70,255));
    circle(playerX,playerY-1,6,rgba(245,210,48,255));
    circle(opponentX,opponentY,8,rgba(90,10,20,255));
    circle(opponentX,opponentY-1,6,rgba(230,45,48,255));
    circle(ballX,ballY,4,rgba(248,248,245,255));
    circle(ballX-1,ballY-1,1.5f,rgba(20,20,20,255));
}

static void resetAfterGoal(int homeScored) {
    if (homeScored) scoreHome++; else scoreAway++;
    playerX=120; playerY=136;
    opponentX=350; opponentY=136;
    ballX=240; ballY=136;
    ballVX=ballVY=0;
    lastGoalFrames=90;
}

static void updateGame(SceCtrlData pad) {
    const float speed = 2.2f;
    float oldX=playerX, oldY=playerY;
    if (pad.Buttons & PSP_CTRL_LEFT) playerX -= speed;
    if (pad.Buttons & PSP_CTRL_RIGHT) playerX += speed;
    if (pad.Buttons & PSP_CTRL_UP) playerY -= speed;
    if (pad.Buttons & PSP_CTRL_DOWN) playerY += speed;

    if (playerX < 26) playerX=26;
    if (playerX > 454) playerX=454;
    if (playerY < 32) playerY=32;
    if (playerY > 240) playerY=240;

    /* Bola perto do jogador: pode ser conduzida. X chuta para a direita. */
    {
        float dx=ballX-playerX, dy=ballY-playerY;
        float dist=sqrtf(dx*dx+dy*dy);
        if (dist < 18.0f && (pad.Buttons & PSP_CTRL_CROSS)) {
            ballVX = 4.2f;
            ballVY = (ballY-playerY)*0.18f;
        } else if (dist < 15.0f && (pad.Buttons & PSP_CTRL_CIRCLE)) {
            ballVX = -3.0f;
            ballVY = (ballY-playerY)*0.2f;
        } else if (dist < 15.0f && (fabsf(playerX-oldX)>0.0f || fabsf(playerY-oldY)>0.0f)) {
            ballX += (playerX-oldX)*0.85f;
            ballY += (playerY-oldY)*0.85f;
        }
    }

    /* Adversario simples que persegue a bola. */
    if (opponentX < ballX-3) opponentX += 0.65f;
    if (opponentX > ballX+3) opponentX -= 0.65f;
    if (opponentY < ballY-3) opponentY += 0.55f;
    if (opponentY > ballY+3) opponentY -= 0.55f;
    {
        float dx=ballX-opponentX, dy=ballY-opponentY;
        if (sqrtf(dx*dx+dy*dy)<13.0f && (matchFrames%50==0)) {
            ballVX = -2.4f;
            ballVY = (136.0f-ballY)*0.025f;
        }
    }

    ballX += ballVX; ballY += ballVY;
    ballVX *= 0.985f; ballVY *= 0.985f;

    if (ballY < 29 || ballY > 243) { ballY = ballY < 29 ? 29 : 243; ballVY *= -0.65f; }
    if (ballX < 14) {
        if (ballY > 110 && ballY < 162) resetAfterGoal(0);
        else { ballX=14; ballVX *= -0.65f; }
    }
    if (ballX > 466) {
        if (ballY > 110 && ballY < 162) resetAfterGoal(1);
        else { ballX=466; ballVX *= -0.65f; }
    }
    if (lastGoalFrames>0) lastGoalFrames--;
    matchFrames++;
}

int main(void) {
    SceCtrlData pad;
    int running=1;
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void*)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
    sceGuOffset(2048-(SCREEN_W/2), 2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    while (running) {
        sceCtrlReadBufferPositive(&pad,1);
        if (pad.Buttons & PSP_CTRL_START) running=0;

        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(rgba(0,0,0,255));
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        drawScene();
        sceGuFinish();
        sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        /* Texto sobreposto usando o console de depuracao. */
        pspDebugScreenSetXY(1,0);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf(" FUTEBOL PSP - PROTOTIPO | CASA %d x %d VISITANTE ",scoreHome,scoreAway);
        pspDebugScreenSetXY(1,1);
        pspDebugScreenPrintf("D-PAD: mover | X: chutar direita | O: chutar esquerda | START: sair");
        if (lastGoalFrames>0) {
            pspDebugScreenSetXY(17,16);
            pspDebugScreenPrintf(" GOOOOOL! ");
        }
        sceKernelDelayThread(16667);
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
