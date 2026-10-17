
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("FutebolPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define PI 3.14159265f

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned short __attribute__((aligned(16))) indices[] = {
    0, 1, 2, 0, 2, 3
};

typedef struct {
    float x, y, z;
    unsigned int color;
} Vertex;

static Vertex quad[4];

static void drawQuad(float x1, float z1, float x2, float z2,
                     float y, unsigned int color) {
    quad[0] = (Vertex){x1, y, z1, color};
    quad[1] = (Vertex){x2, y, z1, color};
    quad[2] = (Vertex){x2, y, z2, color};
    quad[3] = (Vertex){x1, y, z2, color};

    sceGuColor(color);
    sceGuDrawArray(GU_TRIANGLES,
        GU_VERTEX_32BITF | GU_COLOR_8888 |
        GU_TRANSFORM_3D,
        6, indices, quad);
}

static void drawLine(float x1, float z1, float x2, float z2,
                     unsigned int color) {
    Vertex v[2] = {
        {x1, 0.025f, z1, color},
        {x2, 0.025f, z2, color}
    };
    sceGuColor(color);
    sceGuDrawArray(GU_LINES,
        GU_VERTEX_32BITF | GU_COLOR_8888 |
        GU_TRANSFORM_3D, 2, 0, v);
}

static void drawPlayer(float x, float z, unsigned int color) {
    drawQuad(x - 0.35f, z - 0.35f,
             x + 0.35f, z + 0.35f, 0.12f, color);
    drawQuad(x - 0.22f, z - 0.22f,
             x + 0.22f, z + 0.22f, 0.42f,
             0xFFFFCCAA);
}

int main(void) {
    pspDebugScreenInit();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);

    sceGuDrawBuffer(GU_PSM_8888,
        (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT,
        (void *)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void *)0x110000, BUF_WIDTH);

    sceGuOffset(2048 - SCR_WIDTH / 2,
                2048 - SCR_HEIGHT / 2);
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0, 0);

    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;
    float ballX = 0.0f, ballZ = 0.0f;
    float camX = 0.0f, camZ = 14.0f;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        if (pad.Buttons & PSP_CTRL_LEFT)  ballX -= 0.08f;
        if (pad.Buttons & PSP_CTRL_RIGHT) ballX += 0.08f;
        if (pad.Buttons & PSP_CTRL_UP)    ballZ -= 0.08f;
        if (pad.Buttons & PSP_CTRL_DOWN)  ballZ += 0.08f;

        if (ballX < -9.0f) ballX = -9.0f;
        if (ballX >  9.0f) ballX =  9.0f;
        if (ballZ < -17.0f) ballZ = -17.0f;
        if (ballZ >  17.0f) ballZ =  17.0f;

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF66AADD);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT |
                   GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(45.0f, 16.0f / 9.0f,
                          0.5f, 100.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        ScePspFVector3 camera = {0.0f, 16.0f, 22.0f};
        ScePspFVector3 target = {0.0f, 0.0f, 0.0f};
        ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
        sceGumLookAt(&camera, &target, &up);

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        sceGuDisable(GU_CULL_FACE);

        drawQuad(-10, -18, 10, 18, 0.0f, 0xFF268A32);

        for (int i = -8; i <= 8; i += 4)
            drawQuad((float)i, -18, (float)i + 2, 18,
                     0.01f, 0xFF2D9639);

        drawLine(-9, -17, 9, -17, 0xFFFFFFFF);
        drawLine(-9, 17, 9, 17, 0xFFFFFFFF);
        drawLine(-9, -17, -9, 17, 0xFFFFFFFF);
        drawLine(9, -17, 9, 17, 0xFFFFFFFF);
        drawLine(-9, 0, 9, 0, 0xFFFFFFFF);
        drawLine(-9, -8, 9, -8, 0xFFFFFFFF);
        drawLine(-9, 8, 9, 8, 0xFFFFFFFF);

        drawPlayer(-3, -5, 0xFF3333DD);
        drawPlayer( 2, -3, 0xFF3333DD);
        drawPlayer( 0,  5, 0xFFDD3333);
        drawPlayer( 4,  8, 0xFFDD3333);

        drawQuad(ballX - 0.22f, ballZ - 0.22f,
                 ballX + 0.22f, ballZ + 0.22f,
                 0.18f, 0xFFFFFFFF);

        sceGuEnable(GU_CULL_FACE);
        sceGuFinish();
        sceGuSync(0, 0);
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
