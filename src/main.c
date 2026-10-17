
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>

PSP_MODULE_INFO("FutebolPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define BW 512

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

static void quad(float x1, float z1, float x2, float z2,
                 float y, unsigned int color) {
    Vertex *v = (Vertex *)sceGuGetMemory(6 * sizeof(Vertex));

    v[0] = (Vertex){color, x1, y, z1};
    v[1] = (Vertex){color, x2, y, z1};
    v[2] = (Vertex){color, x2, y, z2};
    v[3] = (Vertex){color, x1, y, z1};
    v[4] = (Vertex){color, x2, y, z2};
    v[5] = (Vertex){color, x1, y, z2};

    sceGumDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        6, 0, v);
}

static void player(float x, float z, unsigned int shirt) {
    /* Corpo e cabeça simplificados */
    quad(x - 0.32f, z - 0.25f, x + 0.32f, z + 0.25f,
         0.18f, shirt);
    quad(x - 0.19f, z - 0.15f, x + 0.19f, z + 0.15f,
         0.72f, 0xFFCCAA88);
}

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
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    SceCtrlData pad;

    float px = 0.0f, pz = 3.0f;
    float bx = 0.0f, bz = 2.0f;
    float vx = 0.0f, vz = 0.0f;
    float ex = 2.0f, ez = -5.0f;
    int oldButtons = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    init3D();

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_START)
            break;

        float dx = 0.0f, dz = 0.0f;

        if (pad.Buttons & PSP_CTRL_LEFT)  dx -= 1.0f;
        if (pad.Buttons & PSP_CTRL_RIGHT) dx += 1.0f;
        if (pad.Buttons & PSP_CTRL_UP)    dz -= 1.0f;
        if (pad.Buttons & PSP_CTRL_DOWN)  dz += 1.0f;

        if (pad.Lx < 90)  dx -= (128 - pad.Lx) / 38.0f;
        if (pad.Lx > 166) dx += (pad.Lx - 128) / 38.0f;
        if (pad.Ly < 90)  dz -= (128 - pad.Ly) / 38.0f;
        if (pad.Ly > 166) dz += (pad.Ly - 128) / 38.0f;

        px += dx * 0.075f;
        pz += dz * 0.075f;

        if (px < -9.0f) px = -9.0f;
        if (px >  9.0f) px =  9.0f;
        if (pz < -17.0f) pz = -17.0f;
        if (pz >  17.0f) pz =  17.0f;

        /* Bola acompanha o jogador quando está perto */
        float ddx = bx - px;
        float ddz = bz - pz;

        if (ddx * ddx + ddz * ddz < 1.5f) {
            bx = px;
            bz = pz - 0.55f;
        }

        /* X chuta a bola na direção do ataque */
        if ((pad.Buttons & PSP_CTRL_CROSS) &&
            !(oldButtons & PSP_CTRL_CROSS)) {
            if (ddx * ddx + ddz * ddz < 2.5f) {
                vx = dx * 0.20f;
                vz = -0.65f;
                if (dx == 0.0f) vx = 0.0f;
            }
        }

        oldButtons = pad.Buttons;

        bx += vx;
        bz += vz;
        vx *= 0.94f;
        vz *= 0.94f;

        if (bx < -9.0f || bx > 9.0f) vx = -vx * 0.6f;
        if (bz < -17.0f || bz > 17.0f) vz = -vz * 0.6f;

        if (bx < -9.0f) bx = -9.0f;
        if (bx > 9.0f) bx = 9.0f;
        if (bz < -17.0f) bz = -17.0f;
        if (bz > 17.0f) bz = 17.0f;

        /* Adversário simples que persegue a bola */
        if (ex < bx) ex += 0.025f;
        if (ex > bx) ex -= 0.025f;
        if (ez < bz) ez += 0.025f;
        if (ez > bz) ez -= 0.025f;

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFFB0D8F0);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(50.0f, 16.0f / 9.0f, 0.5f, 100.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();

        ScePspFVector3 camera = {px, 11.0f, pz + 13.0f};
        ScePspFVector3 target = {px, 0.0f, pz - 3.0f};
        ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
        sceGumLookAt(&camera, &target, &up);

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        /* Gramado */
        quad(-10, -19, 10, 19, 0.0f, 0xFF267C28);

        /* Faixas do gramado */
        for (int z = -18; z < 18; z += 4)
            quad(-9.8f, (float)z, 9.8f, (float)z + 2.0f,
                 0.015f, 0xFF308A32);

        /* Linhas do campo */
        quad(-9.1f, -17.1f, 9.1f, -16.9f, 0.03f, 0xFFFFFFFF);
        quad(-9.1f, 16.9f, 9.1f, 17.1f, 0.03f, 0xFFFFFFFF);
        quad(-9.1f, -17.0f, -8.9f, 17.0f, 0.03f, 0xFFFFFFFF);
        quad(8.9f, -17.0f, 9.1f, 17.0f, 0.03f, 0xFFFFFFFF);
        quad(-9.0f, -0.10f, 9.0f, 0.10f, 0.03f, 0xFFFFFFFF);

        /* Jogadores */
        player(px, pz, 0xFFDD4422);
        player(-3.0f, -5.0f, 0xFFDD4422);
        player(3.0f, -9.0f, 0xFFDD4422);
        player(ex, ez, 0xFF3333DD);
        player(-4.0f, 8.0f, 0xFF3333DD);

        /* Bola */
        quad(bx - 0.20f, bz - 0.20f,
             bx + 0.20f, bz + 0.20f,
             0.09f, 0xFFFFFFFF);

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
