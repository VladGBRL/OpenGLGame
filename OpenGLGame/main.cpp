
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <freeglut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const float RX = 10.f;
static const float RZ = 6.f;
static const float ROAD_W = 2.5f;
static const float ROAD_Y = 0.015f;
static const int   SEGS = 48;
static const float OUTER_Z = RZ + ROAD_W;
static const float INNER_Z = RZ - ROAD_W;
static const float FENCE = 18.f;

GLuint tex_ground = 0, tex_road = 0, tex_sky = 0, tex_building = 0;
GLuint tex_bark = 0, tex_leaves = 0, tex_barrier = 0, tex_concrete = 0, tex_seat = 0;

static GLuint makeSolidTex(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char px[3] = { r,g,b };
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
    return t;
}
static GLuint loadTexture(const char* path, bool repeat,
    unsigned char fr, unsigned char fg, unsigned char fb) {
    int w, h, ch; stbi_set_flip_vertically_on_load(1);
    bool hdr = stbi_is_hdr(path);
    void* data = hdr ? (void*)stbi_loadf(path, &w, &h, &ch, 3)
        : (void*)stbi_load(path, &w, &h, &ch, 3);
    if (!data) { printf("[TEX WARN] %s\n", path); return makeSolidTex(fr, fg, fb); }
    if (hdr) { float* f = (float*)data; for (int i = 0; i < w * h * 3; i++)f[i] /= (f[i] + 1.f); }
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP);
    if (hdr) gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_FLOAT, data);
    else    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    printf("[TEX OK] %s (%dx%d)\n", path, w, h);
    return t;
}

void initTextures() {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.15f);
    glEnable(GL_CULL_FACE);
    tex_sky = loadTexture("panorama.hdr", false, 135, 185, 220);
    tex_ground = loadTexture("grass.jpg", true, 72, 120, 48);
    tex_road = loadTexture("asphalt.jpg", true, 55, 55, 58);
    tex_building = loadTexture("building.jpg", true, 170, 160, 145);
    tex_bark = loadTexture("bark.jpeg", true, 90, 60, 30);
    tex_leaves = loadTexture("leaves.jpg", true, 50, 105, 40);
    tex_barrier = loadTexture("fence.jpg", true, 215, 215, 220);
    tex_concrete = loadTexture("concrete.jpg", true, 175, 170, 165);
    tex_seat = loadTexture("seat.jpg", true, 30, 80, 160);
}

int   WIDTH = 800, HEIGHT = 600;
float camX = 0.f, camY = 6.f, camZ = 14.f;
float yaw = 180.f, pitch = -18.f, speed = 0.15f;

static bool  nightMode = false;
static bool  mouseLeft = false, mouseRight = false;
static int   mouseLastX = 0, mouseLastY = 0;
static float mouseSens = 0.25f;
static float mouseYSens = 0.015f;

static GLfloat sunPos[4] = { 15.f,28.f,18.f,0.f };

struct PointLight { float x, y, z; GLenum gl; };
static const int NL = 5;
static PointLight lamps[NL];

void initLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat a0[] = { 0.30f,0.32f,0.36f,1.f }, d0[] = { 1.05f,0.98f,0.88f,1.f }, s0[] = { 0.40f,0.38f,0.30f,1.f };
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, a0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, d0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, s0);
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);

    GLfloat a1[] = { 0.10f,0.12f,0.15f,1.f }, d1[] = { 0.18f,0.20f,0.25f,1.f }, p1[] = { -12.f,8.f,-10.f,0.f };
    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_AMBIENT, a1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, d1);
    glLightfv(GL_LIGHT1, GL_POSITION, p1);

    float lpX[NL] = { -10.f, -5.f,  0.f,  5.f, 10.f };
    for (int i = 0; i < NL; i++) {
        lamps[i] = { lpX[i], 9.0f, 0.f, (GLenum)(GL_LIGHT2 + i) };
        GLenum l = lamps[i].gl;
        GLfloat la[] = { 0.10f,0.08f,0.04f,1.f };
        GLfloat ld[] = { 3.00f,2.80f,2.00f,1.f }; 
        GLfloat ls[] = { 1.00f,0.95f,0.70f,1.f };
        GLfloat lp[] = { lamps[i].x, lamps[i].y, lamps[i].z, 1.f };
        glEnable(l);
        glLightfv(l, GL_AMBIENT, la);
        glLightfv(l, GL_DIFFUSE, ld);
        glLightfv(l, GL_SPECULAR, ls);
        glLightfv(l, GL_POSITION, lp);
        glLightf(l, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(l, GL_LINEAR_ATTENUATION, 0.005f);
        glLightf(l, GL_QUADRATIC_ATTENUATION, 0.0f);
    }

    GLfloat ga[] = { 0.18f,0.20f,0.24f,1.f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    GLfloat ms[] = { 0.12f,0.12f,0.12f,1.f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, ms);
    glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 20);
}

void initFog() {
    GLfloat fc[] = { 0.70f,0.78f,0.86f,1.f };
    glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_EXP2);
    glFogfv(GL_FOG_COLOR, fc); glFogf(GL_FOG_DENSITY, 0.013f); glHint(GL_FOG_HINT, GL_NICEST);
}

static void buildShadowMatrix(float M[16],
    float lx, float ly, float lz, float lw,
    float planeY)
{
    float d = -planeY;
    float dot = ly + d * lw;
    M[0] = dot - lx * 0; M[4] = -lx * 1;      M[8] = -lx * 0; M[12] = -lx * d;
    M[1] = -ly * 0;      M[5] = dot - ly * 1;  M[9] = -ly * 0; M[13] = -ly * d;
    M[2] = -lz * 0;      M[6] = -lz * 1;       M[10] = dot - lz * 0; M[14] = -lz * d;
    M[3] = -lw * 0;      M[7] = -lw * 1;       M[11] = -lw * 0; M[15] = dot - lw * d;
}

static void shadowBox(float ox, float oy, float oz, float sx, float sy, float sz) {
    float x0 = ox, x1 = ox + sx, y0 = oy, y1 = oy + sy, z0 = oz, z1 = oz + sz;
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glEnd();
}
static void shadowCyl(float cx, float cz, float r, float h) {
    shadowBox(cx - r, 0, cz - r, 2 * r, h, 2 * r);
}

static void drawShadowCasters() {
    shadowBox(-24.5f, 0, -24.f, 5.f, 8.f, 4.f);
    shadowBox(19.5f, 0, -24.f, 5.f, 8.f, 4.f);
    shadowBox(-25.f, 0, 20.f, 6.f, 8.f, 4.f);
    shadowBox(19.5f, 0, 20.f, 5.f, 8.f, 4.f);
    shadowBox(-24.f, 0, -2.f, 4.f, 6.f, 3.f);
    shadowBox(20.f, 0, -1.5f, 4.f, 6.f, 3.f);

    shadowBox(-5.5f, 0, OUTER_Z + 1.8f, 11.f, 3.5f, 4.5f);
    shadowBox(-5.5f, 0, -(OUTER_Z + 6.3f), 11.f, 3.5f, 4.5f);

    for (int i = -2; i <= 2; i++) shadowBox((float)i * 3.8f - 1.8f, 0, -1.5f, 3.6f, 2.8f, 3.f);

    shadowCyl(-(ROAD_W + 1.5f), OUTER_Z + 0.4f, 0.22f, 5.5f);
    shadowCyl(ROAD_W + 1.5f, OUTER_Z + 0.4f, 0.22f, 5.5f);

    for (int i = -2; i <= 2; i++) {
        float lx = (float)i * 5.f;
        shadowCyl(lx, OUTER_Z + 1.1f, 0.08f, 4.0f);
        shadowCyl(lx, -OUTER_Z - 1.1f, 0.08f, 4.0f);
    }

    int nB = (int)(2.f * RX / 2.0f) + 1;
    for (int i = 0; i < nB; i++) {
        float bx = -RX + (float)i * 2.f - 1.f;
        shadowBox(bx, 0, OUTER_Z + 0.02f, 2.f, 0.65f, 0.32f);
        shadowBox(bx, 0, INNER_Z - 0.64f, 2.f, 0.65f, 0.32f);
        shadowBox(bx, 0, -OUTER_Z - 0.32f, 2.f, 0.65f, 0.32f);
        shadowBox(bx, 0, -INNER_Z + 0.02f, 2.f, 0.65f, 0.32f);
    }

    shadowBox(-1.65f, ROAD_Y, -0.44f, 3.3f, 0.9f, 0.88f);

    shadowBox(-13.5f, 0, -0.4f, 1.f, 1.5f, 0.8f);
    shadowBox(12.5f, 0, -0.4f, 1.f, 1.5f, 0.8f);
}
void renderShadows() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float Msun[16];
    buildShadowMatrix(Msun, sunPos[0], sunPos[1], sunPos[2], 0.f, 0.01f);
    glColor4f(0.f, 0.f, 0.f, 0.38f);
    glPushMatrix(); glMultMatrixf(Msun); drawShadowCasters(); glPopMatrix();

    glColor4f(0.f, 0.f, 0.f, 0.14f);
    for (int li = 0; li < NL; li++) {
        float M[16];
        buildShadowMatrix(M, lamps[li].x, lamps[li].y, lamps[li].z, 1.f, 0.01f);
        glPushMatrix(); glMultMatrixf(M);
        for (int i = -2; i <= 2; i++) shadowCyl((float)i * 5.f, OUTER_Z + 1.1f, 0.08f, 4.0f);
        shadowBox(-1.65f, ROAD_Y, -0.44f, 3.3f, 0.9f, 0.88f);
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glPopAttrib();
}

static void solidColor(float r, float g, float b) {
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glColor3f(r, g, b);
}
static void restoreTexLit() {
    glColor3f(1, 1, 1); glEnable(GL_TEXTURE_2D); glEnable(GL_LIGHTING);
}
static void drawBox(float ox, float oy, float oz,
    float sx, float sy, float sz, float uS = 1.f, float vS = 1.f) {
    float x0 = ox, x1 = ox + sx, y0 = oy, y1 = oy + sy, z0 = oz, z1 = oz + sz;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(x0, y0, z1); glTexCoord2f(uS, 0); glVertex3f(x1, y0, z1); glTexCoord2f(uS, vS); glVertex3f(x1, y1, z1); glTexCoord2f(0, vS); glVertex3f(x0, y1, z1);
    glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(x1, y0, z0); glTexCoord2f(uS, 0); glVertex3f(x0, y0, z0); glTexCoord2f(uS, vS); glVertex3f(x0, y1, z0); glTexCoord2f(0, vS); glVertex3f(x1, y1, z0);
    glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(x0, y0, z0); glTexCoord2f(uS, 0); glVertex3f(x0, y0, z1); glTexCoord2f(uS, vS); glVertex3f(x0, y1, z1); glTexCoord2f(0, vS); glVertex3f(x0, y1, z0);
    glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(x1, y0, z1); glTexCoord2f(uS, 0); glVertex3f(x1, y0, z0); glTexCoord2f(uS, vS); glVertex3f(x1, y1, z0); glTexCoord2f(0, vS); glVertex3f(x1, y1, z1);
    glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(x0, y1, z0); glTexCoord2f(uS, 0); glVertex3f(x1, y1, z0); glTexCoord2f(uS, uS); glVertex3f(x1, y1, z1); glTexCoord2f(0, uS); glVertex3f(x0, y1, z1);
    glEnd();
}
static void drawCylinder(float cx, float by, float cz, float r, float h, int segs = 12, float uS = 1.f) {
    for (int i = 0; i < segs; i++) {
        float a0 = 2.f * (float)M_PI * i / segs, a1 = 2.f * (float)M_PI * (i + 1) / segs;
        float x0 = cx + r * cosf(a0), z0 = cz + r * sinf(a0), x1 = cx + r * cosf(a1), z1 = cz + r * sinf(a1);
        glBegin(GL_QUADS); glNormal3f(cosf(a0), 0, sinf(a0));
        glTexCoord2f((float)i / segs * uS, 0);     glVertex3f(x0, by, z0);
        glTexCoord2f((float)(i + 1) / segs * uS, 0); glVertex3f(x1, by, z1);
        glTexCoord2f((float)(i + 1) / segs * uS, 1); glVertex3f(x1, by + h, z1);
        glTexCoord2f((float)i / segs * uS, 1);     glVertex3f(x0, by + h, z0); glEnd();
    }
    glBegin(GL_TRIANGLE_FAN); glNormal3f(0, 1, 0); glTexCoord2f(.5f, .5f); glVertex3f(cx, by + h, cz);
    for (int i = 0; i <= segs; i++) { float a = 2.f * (float)M_PI * i / segs; glTexCoord2f(.5f + .5f * cosf(a), .5f + .5f * sinf(a)); glVertex3f(cx + r * cosf(a), by + h, cz + r * sinf(a)); }
    glEnd();
}
static void flatQuad(float x0, float z0, float x1, float z1, float x2, float z2, float x3, float z3, float y, float uL, float vL = 1.f) {
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);   glVertex3f(x0, y, z0); glTexCoord2f(uL, 0);  glVertex3f(x1, y, z1);
    glTexCoord2f(uL, vL); glVertex3f(x2, y, z2); glTexCoord2f(0, vL);  glVertex3f(x3, y, z3); glEnd();
}

void drawSkybox() {
    const float S = 50.f;
    glDisable(GL_DEPTH_TEST); glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);

    if (nightMode) {
        glDisable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);
        glColor3f(0.01f, 0.02f, 0.06f);
        glVertex3f(-S, S, -S); glVertex3f(S, S, -S);
        glVertex3f(S, S, S);  glVertex3f(-S, S, S);
        glEnd();

        float sTop[3] = { 0.01f,0.02f,0.06f };
        float sBot[3] = { 0.04f,0.05f,0.12f };
        float sd[4][4][3] = {
            {{-S,-S,-S},{S,-S,-S},{S,S,-S},{-S,S,-S}},
            {{ S,-S,-S},{S,-S, S},{S,S, S},{ S,S,-S}},
            {{ S,-S, S},{-S,-S,S},{-S,S,S},{ S,S, S}},
            {{-S,-S, S},{-S,-S,-S},{-S,S,-S},{-S,S,S}}
        };
        for (int f = 0; f < 4; f++) {
            glBegin(GL_QUADS);
            glColor3fv(sBot); glVertex3fv(sd[f][0]);
            glColor3fv(sBot); glVertex3fv(sd[f][1]);
            glColor3fv(sTop); glVertex3fv(sd[f][2]);
            glColor3fv(sTop); glVertex3fv(sd[f][3]);
            glEnd();
        }

        /* stars as GL_POINTS */
        glPointSize(1.5f);
        glColor3f(0.85f, 0.88f, 0.95f);
        static const float stars[][3] = {
            {-30,45,-20},{10,48,5},{25,42,-15},{-15,46,30},{5,49,-35},
            {-40,43,10},{35,44,-25},{-8,47,40},{20,45,15},{-25,41,-40},
            {15,49,25},{-35,44,-5},{0,48,-45},{30,42,35},{-10,46,-30},
            {40,43,20},{-20,47,10},{8,45,-20},{-45,41,30},{22,48,-10},
            {-5,46,45},{38,44,-35},{-28,43,5},{12,47,-40},{-42,45,25},
            {-18,44,-12},{4,46,28},{33,43,-8},{-6,49,18},{28,41,-30},
            {-38,47,22},{16,44,42},{-22,42,-28},{7,48,-6},{-12,45,38}
        };
        glBegin(GL_POINTS);
        for (auto& s : stars) glVertex3f(s[0], s[1], s[2]);
        glEnd();
        glPointSize(1.0f);

        glColor3f(0.02f, 0.03f, 0.05f);
        glBegin(GL_QUADS);
        glVertex3f(-S, -S, -S); glVertex3f(S, -S, -S);
        glVertex3f(S, -S, S);  glVertex3f(-S, -S, S);
        glEnd();

        glEnable(GL_TEXTURE_2D);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, tex_sky);
        glBegin(GL_QUADS);
        glTexCoord2f(0, .6f); glVertex3f(-S, S, -S); glTexCoord2f(1, .6f); glVertex3f(S, S, -S);
        glTexCoord2f(1, 1.f); glVertex3f(S, S, S);   glTexCoord2f(0, 1.f); glVertex3f(-S, S, S);
        glEnd();
        float sd[4][4][3] = {
            {{-S,-S,-S},{S,-S,-S},{S,S,-S},{-S,S,-S}},
            {{ S,-S,-S},{S,-S, S},{S,S, S},{ S,S,-S}},
            {{ S,-S, S},{-S,-S,S},{-S,S,S},{ S,S, S}},
            {{-S,-S, S},{-S,-S,-S},{-S,S,-S},{-S,S,S}}
        };
        float uo[4] = { 0.f,.25f,.5f,.75f };
        for (int f = 0; f < 4; f++) {
            glBegin(GL_QUADS);
            glTexCoord2f(uo[f], .28f);      glVertex3fv(sd[f][0]);
            glTexCoord2f(uo[f] + .25f, .28f); glVertex3fv(sd[f][1]);
            glTexCoord2f(uo[f] + .25f, .72f); glVertex3fv(sd[f][2]);
            glTexCoord2f(uo[f], .72f);      glVertex3fv(sd[f][3]);
            glEnd();
        }
        glBindTexture(GL_TEXTURE_2D, tex_ground);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-S, -S, -S); glTexCoord2f(6, 0); glVertex3f(S, -S, -S);
        glTexCoord2f(6, 6); glVertex3f(S, -S, S);   glTexCoord2f(0, 6); glVertex3f(-S, -S, S);
        glEnd();
    }

    glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glEnable(GL_FOG); glEnable(GL_LIGHTING);
}

void drawGround() {
    const float S = 40.f; glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, tex_ground);
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);   glVertex3f(-S, 0, -S); glTexCoord2f(20, 0); glVertex3f(S, 0, -S);
    glTexCoord2f(20, 20); glVertex3f(S, 0, S);   glTexCoord2f(0, 20); glVertex3f(-S, 0, S); glEnd();
    glEnable(GL_CULL_FACE);
}

void drawRoadCircuit() {
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, tex_road);
    float uL = 2.f * RX / (2.f * ROAD_W);
    flatQuad(-RX, OUTER_Z, RX, OUTER_Z, RX, INNER_Z, -RX, INNER_Z, ROAD_Y, uL);
    flatQuad(-RX, -OUTER_Z, RX, -OUTER_Z, RX, -INNER_Z, -RX, -INNER_Z, ROAD_Y, uL);
    float arcU = (float)M_PI * RZ / (2.f * ROAD_W);
    for (int side = -1; side <= 1; side += 2) {
        float cx = (float)side * RX, sd = (side > 0) ? -90.f : 90.f, Ri = RZ - ROAD_W, Ro = RZ + ROAD_W;
        for (int i = 0; i < SEGS; i++) {
            float a0 = (sd + (float)i / SEGS * 180.f) * (float)M_PI / 180.f, a1 = (sd + (float)(i + 1) / SEGS * 180.f) * (float)M_PI / 180.f;
            float u0 = (float)i / SEGS * arcU, u1 = (float)(i + 1) / SEGS * arcU;
            glBegin(GL_QUADS); glNormal3f(0, 1, 0);
            glTexCoord2f(u0, 0); glVertex3f(cx + Ri * cosf(a0), ROAD_Y, Ri * sinf(a0));
            glTexCoord2f(u1, 0); glVertex3f(cx + Ri * cosf(a1), ROAD_Y, Ri * sinf(a1));
            glTexCoord2f(u1, 1); glVertex3f(cx + Ro * cosf(a1), ROAD_Y, Ro * sinf(a1));
            glTexCoord2f(u0, 1); glVertex3f(cx + Ro * cosf(a0), ROAD_Y, Ro * sinf(a0)); glEnd();
        }
    }
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    float pitY = ROAD_Y + 0.003f, pitZ0 = INNER_Z - 1.8f, pitZ1 = INNER_Z - 0.3f;
    flatQuad(-RX + 1.f, pitZ0, RX - 1.f, pitZ0, RX - 1.f, pitZ1, -RX + 1.f, pitZ1, pitY, uL);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glColor3f(0.95f, 0.95f, 0.95f);
    float plY = pitY + 0.003f;
    glBegin(GL_QUADS); glVertex3f(-RX + 1.f, plY, pitZ0 + 0.06f); glVertex3f(RX - 1.f, plY, pitZ0 + 0.06f); glVertex3f(RX - 1.f, plY, pitZ0 + 0.12f); glVertex3f(-RX + 1.f, plY, pitZ0 + 0.12f); glEnd();
    glBegin(GL_QUADS); glVertex3f(-RX + 1.f, plY, pitZ1 - 0.12f); glVertex3f(RX - 1.f, plY, pitZ1 - 0.12f); glVertex3f(RX - 1.f, plY, pitZ1 - 0.06f); glVertex3f(-RX + 1.f, plY, pitZ1 - 0.06f); glEnd();
    float kW = 0.30f, kH = ROAD_Y + 0.025f, kS = 1.1f;
    for (float x = -RX; x < RX; x += kS) {
        bool red = ((int)((x + RX) / kS) % 2 == 0); float xe = x + kS * .5f;
        glColor3f(red ? 0.92f : 0.97f, red ? 0.05f : 0.97f, red ? 0.05f : 0.97f);
        auto kerb = [&](float z0, float z1) {glBegin(GL_QUADS); glNormal3f(0, 1, 0); glVertex3f(x, kH, z0); glVertex3f(xe, kH, z0); glVertex3f(xe, kH, z1); glVertex3f(x, kH, z1); glEnd(); };
        kerb(OUTER_Z, OUTER_Z + kW); kerb(INNER_Z - kW, INNER_Z); kerb(-OUTER_Z - kW, -OUTER_Z); kerb(-INNER_Z, -INNER_Z + kW);
    }
    glColor3f(0.96f, 0.96f, 0.96f);
    for (float x = -RX + .2f; x < RX; x += 1.4f) {
        float xe = x + 0.8f; if (xe > RX)xe = RX; float y = ROAD_Y + .004f, dh = 0.055f;
        glBegin(GL_QUADS); glVertex3f(x, y, RZ - dh); glVertex3f(xe, y, RZ - dh); glVertex3f(xe, y, RZ + dh); glVertex3f(x, y, RZ + dh); glEnd();
        glBegin(GL_QUADS); glVertex3f(x, y, -RZ - dh); glVertex3f(xe, y, -RZ - dh); glVertex3f(xe, y, -RZ + dh); glVertex3f(x, y, -RZ + dh); glEnd();
    }
    float sq = (OUTER_Z - INNER_Z) / 8.f;
    for (int i = 0; i < 8; i++) {
        glColor3f((i % 2 == 0) ? 1.f : 0.f, (i % 2 == 0) ? 1.f : 0.f, (i % 2 == 0) ? 1.f : 0.f);
        float z0c = INNER_Z + i * sq, z1c = z0c + sq, y = ROAD_Y + .005f;
        glBegin(GL_QUADS); glVertex3f(-.35f, y, z0c); glVertex3f(.35f, y, z0c); glVertex3f(.35f, y, z1c); glVertex3f(-.35f, y, z1c); glEnd();
    }
    glColor3f(0.1f, 0.3f, 0.9f);
    glBegin(GL_QUADS); glVertex3f(-RX, ROAD_Y + .006f, RZ - 0.05f); glVertex3f(-RX + 2.5f, ROAD_Y + .006f, RZ - 0.05f); glVertex3f(-RX + 2.5f, ROAD_Y + .006f, RZ + 0.05f); glVertex3f(-RX, ROAD_Y + .006f, RZ + 0.05f); glEnd();
    glColor3f(1, 1, 1); glEnable(GL_TEXTURE_2D); glEnable(GL_LIGHTING); glEnable(GL_CULL_FACE);
}

void drawTree(float cx, float cz, float rotY = 0.f, float sc = 1.f) {
    glPushMatrix(); glTranslatef(cx, 0.f, cz); glRotatef(rotY, 0, 1, 0); glScalef(sc, sc, sc);
    glBindTexture(GL_TEXTURE_2D, tex_bark); drawCylinder(0, 0, 0, 0.18f, 1.5f, 10, 2.f);
    glBindTexture(GL_TEXTURE_2D, tex_leaves); glDisable(GL_CULL_FACE); glAlphaFunc(GL_GREATER, .3f);
    float cy = 1.2f, cr = 1.15f;
    for (int k = 0; k < 3; k++) {
        glPushMatrix(); glRotatef(k * 60.f, 0, 1, 0);
        glBegin(GL_QUADS); glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(-cr, cy, 0); glTexCoord2f(1, 0); glVertex3f(cr, cy, 0); glTexCoord2f(1, 1); glVertex3f(cr, cy + 2.2f * cr, 0); glTexCoord2f(0, 1); glVertex3f(-cr, cy + 2.2f * cr, 0); glEnd();
        glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(1, 0); glVertex3f(cr, cy, 0); glTexCoord2f(0, 0); glVertex3f(-cr, cy, 0); glTexCoord2f(0, 1); glVertex3f(-cr, cy + 2.2f * cr, 0); glTexCoord2f(1, 1); glVertex3f(cr, cy + 2.2f * cr, 0); glEnd();
        glPopMatrix();
    }
    glAlphaFunc(GL_GREATER, .15f); glEnable(GL_CULL_FACE); glPopMatrix();
}

void drawBuilding(float cx, float cz, float rotY, float w, float h, float d) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    float hw = w * .5f, hd = d * .5f;
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    drawBox(-hw - 0.20f, 0, -hd - 0.20f, w + 0.40f, 0.50f, d + 0.40f, w / 1.4f, 1.f);
    drawBox(-hw - 0.10f, 0.50f, -hd - 0.10f, w + 0.20f, 0.12f, d + 0.20f, w / 1.4f, 1.f);
    glBindTexture(GL_TEXTURE_2D, tex_building);
    drawBox(-hw, 0.62f, -hd, w, h - 0.62f, d, w / 1.6f, (h - 0.62f) / 2.f);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    for (float band = 0.62f + 2.3f; band < h; band += 2.3f) drawBox(-hw - 0.04f, band, -hd - 0.04f, w + 0.08f, 0.14f, d + 0.08f, w / 1.5f, 1.f);
    drawBox(-hw - 0.22f, h, -hd - 0.22f, w + 0.44f, 0.28f, d + 0.44f, w / 1.4f, 1.f);
    drawBox(-hw, h + 0.28f, -hd, w, 0.38f, 0.13f); drawBox(-hw, h + 0.28f, hd - 0.13f, w, 0.38f, 0.13f);
    drawBox(-hw, h + 0.28f, -hd, 0.13f, 0.38f, d); drawBox(hw - 0.13f, h + 0.28f, -hd, 0.13f, 0.38f, d);
    drawBox(-0.55f, h + 0.66f, -0.45f, 1.1f, 0.75f, 0.90f, 2.f, 2.f);
    drawCylinder(0.3f, h + 1.41f, -0.2f, 0.025f, 1.2f, 6);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    int floors = (int)((h - 0.62f - 0.5f) / 1.15f), cols = (int)((w - 0.5f) / 1.25f); if (cols < 1)cols = 1;
    float wW = 0.54f, wH = 0.70f, wSpX = (w - 0.5f) / (float)cols, startX = -hw + 0.25f;
    int colsZ = (int)((d - 0.5f) / 1.25f); if (colsZ < 1)colsZ = 1;
    float wSpZ = (d - 0.5f) / (float)colsZ, startZ = -hd + 0.25f;
    for (int fl = 0; fl < floors; fl++) {
        float wy = 0.62f + 0.4f + (float)fl * 1.15f;
        for (int col = 0; col < cols; col++) {
            float wx = startX + col * wSpX + (wSpX - wW) * .5f;
            float br = 0.10f + 0.04f * (fl % 3), bg = 0.18f + 0.04f * (col % 3), bb = 0.38f + 0.05f * (fl % 2);
            if ((fl + col) % 7 == 0) { br = 0.55f; bg = 0.48f; bb = 0.22f; }
            solidColor(br, bg, bb);
            glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(wx, wy, -hd - 0.007f); glVertex3f(wx + wW, wy, -hd - 0.007f); glVertex3f(wx + wW, wy + wH, -hd - 0.007f); glVertex3f(wx, wy + wH, -hd - 0.007f); glEnd();
            float fw = 0.044f; solidColor(0.80f, 0.78f, 0.72f);
            glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(wx - fw, wy - fw, -hd - 0.011f); glVertex3f(wx + wW + fw, wy - fw, -hd - 0.011f); glVertex3f(wx + wW + fw, wy, -hd - 0.011f); glVertex3f(wx - fw, wy, -hd - 0.011f); glEnd();
            glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(wx - fw, wy + wH, -hd - 0.011f); glVertex3f(wx + wW + fw, wy + wH, -hd - 0.011f); glVertex3f(wx + wW + fw, wy + wH + fw, -hd - 0.011f); glVertex3f(wx - fw, wy + wH + fw, -hd - 0.011f); glEnd();
            glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(wx - fw, wy, -hd - 0.011f); glVertex3f(wx, wy, -hd - 0.011f); glVertex3f(wx, wy + wH, -hd - 0.011f); glVertex3f(wx - fw, wy + wH, -hd - 0.011f); glEnd();
            glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(wx + wW, wy, -hd - 0.011f); glVertex3f(wx + wW + fw, wy, -hd - 0.011f); glVertex3f(wx + wW + fw, wy + wH, -hd - 0.011f); glVertex3f(wx + wW, wy + wH, -hd - 0.011f); glEnd();
            solidColor(br * 0.8f, bg * 0.8f, bb * 0.8f);
            glBegin(GL_QUADS); glNormal3f(0, 0, 1); glVertex3f(wx + wW, wy, hd + 0.007f); glVertex3f(wx, wy, hd + 0.007f); glVertex3f(wx, wy + wH, hd + 0.007f); glVertex3f(wx + wW, wy + wH, hd + 0.007f); glEnd();
        }
        for (int col = 0; col < colsZ; col++) {
            float wz = startZ + col * wSpZ + (wSpZ - wW) * .5f; solidColor(0.10f, 0.17f, 0.34f);
            glBegin(GL_QUADS); glNormal3f(-1, 0, 0); glVertex3f(-hw - 0.007f, wy, wz + wW); glVertex3f(-hw - 0.007f, wy, wz); glVertex3f(-hw - 0.007f, wy + wH, wz); glVertex3f(-hw - 0.007f, wy + wH, wz + wW); glEnd();
            glBegin(GL_QUADS); glNormal3f(1, 0, 0);  glVertex3f(hw + 0.007f, wy, wz);     glVertex3f(hw + 0.007f, wy, wz + wW); glVertex3f(hw + 0.007f, wy + wH, wz + wW); glVertex3f(hw + 0.007f, wy + wH, wz); glEnd();
        }
    }
    glEnable(GL_TEXTURE_2D); glEnable(GL_LIGHTING); glPopMatrix();
}

void drawHoarding(float cx, float cz, float rotY, float w, float h, float r, float g, float b, float r2, float g2, float b2) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    drawCylinder(-w * .5f + 0.15f, 0, 0, 0.06f, 1.0f, 8); drawCylinder(w * .5f - 0.15f, 0, 0, 0.06f, 1.0f, 8);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    solidColor(r, g, b); glBegin(GL_QUADS); glNormal3f(0, 0, 1); glVertex3f(-w * .5f, 0.95f, 0); glVertex3f(w * .5f, 0.95f, 0); glVertex3f(w * .5f, 0.95f + h, 0); glVertex3f(-w * .5f, 0.95f + h, 0); glEnd();
    solidColor(r2, g2, b2); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(w * .5f, 0.95f, -0.04f); glVertex3f(-w * .5f, 0.95f, -0.04f); glVertex3f(-w * .5f, 0.95f + h, -0.04f); glVertex3f(w * .5f, 0.95f + h, -0.04f); glEnd();
    solidColor(0.97f, 0.97f, 0.97f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(w * .5f, 0.95f + h * 0.38f, -0.05f); glVertex3f(-w * .5f, 0.95f + h * 0.38f, -0.05f); glVertex3f(-w * .5f, 0.95f + h * 0.52f, -0.05f); glVertex3f(w * .5f, 0.95f + h * 0.52f, -0.05f); glEnd();
    restoreTexLit(); glPopMatrix();
}

void drawGrandstand(float cx, float cz, float rotY) {
    const float W = 11.0f, riseH = 0.44f, riseD = 0.56f, ROWS = 8, halfW = W * 0.5f, baseH = 0.25f, bwT = 0.28f;
    float totalD = ROWS * riseD, topY = baseH + ROWS * riseH, wallH = topY + 2.4f;
    glPushMatrix(); glTranslatef(cx, 0.f, cz); glRotatef(rotY, 0, 1, 0); glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-halfW, 0, 0); glTexCoord2f(W / 2.f, 0); glVertex3f(halfW, 0, 0); glTexCoord2f(W / 2.f, 1); glVertex3f(halfW, baseH, 0); glTexCoord2f(0, 1); glVertex3f(-halfW, baseH, 0); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-halfW, baseH, 0); glTexCoord2f(W / 2.f, 0); glVertex3f(halfW, baseH, 0); glTexCoord2f(W / 2.f, 1); glVertex3f(halfW, baseH, totalD); glTexCoord2f(0, 1); glVertex3f(-halfW, baseH, totalD); glEnd();
    for (int i = 0; i < (int)ROWS; i++) {
        float y0 = baseH + i * riseH, y1 = baseH + (i + 1) * riseH, z0 = i * riseD, z1 = (i + 1) * riseD;
        glBindTexture(GL_TEXTURE_2D, tex_concrete);
        glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-halfW, y0, z0); glTexCoord2f(1, 0); glVertex3f(halfW, y0, z0); glTexCoord2f(1, 1); glVertex3f(halfW, y1, z0); glTexCoord2f(0, 1); glVertex3f(-halfW, y1, z0); glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_seat);
        glBegin(GL_QUADS); glNormal3f(0, 1, 0); glTexCoord2f(0, 0); glVertex3f(-halfW, y1, z0); glTexCoord2f(W / 0.42f, 0); glVertex3f(halfW, y1, z0); glTexCoord2f(W / 0.42f, 1); glVertex3f(halfW, y1, z1); glTexCoord2f(0, 1); glVertex3f(-halfW, y1, z1); glEnd();
        glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-halfW, y1, z1); glTexCoord2f(1, 0); glVertex3f(halfW, y1, z1); glTexCoord2f(1, 1); glVertex3f(halfW, y1 + riseH * 0.7f, z1); glTexCoord2f(0, 1); glVertex3f(-halfW, y1 + riseH * 0.7f, z1); glEnd();
    }
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    for (int side = -1; side <= 1; side += 2) {
        float sx = side * halfW, nx = (float)side;
        glBegin(GL_QUADS); glNormal3f(nx, 0, 0); glTexCoord2f(0, 0); glVertex3f(sx, 0, 0); glTexCoord2f(1, 0); glVertex3f(sx, 0, totalD); glTexCoord2f(1, 1); glVertex3f(sx, baseH, totalD); glTexCoord2f(0, 1); glVertex3f(sx, baseH, 0); glEnd();
        for (int i = 0; i < (int)ROWS; i++) {
            float y0 = baseH + i * riseH, y1 = baseH + (i + 1) * riseH, z0 = i * riseD, z1 = (i + 1) * riseD;
            glBegin(GL_QUADS); glNormal3f(nx, 0, 0); glTexCoord2f(0, 0); glVertex3f(sx, y0, z0); glTexCoord2f(1, 0); glVertex3f(sx, y1, z0); glTexCoord2f(1, 1); glVertex3f(sx, y1, z1); glTexCoord2f(0, 1); glVertex3f(sx, y0, z1); glEnd();
        }
        glBegin(GL_TRIANGLES); glNormal3f(nx, 0, 0); glVertex3f(sx, baseH, 0); glVertex3f(sx, topY, totalD); glVertex3f(sx, baseH, totalD); glEnd();
    }
    glBindTexture(GL_TEXTURE_2D, tex_building);
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-halfW, 0, totalD); glTexCoord2f(W / 2.f, 0); glVertex3f(halfW, 0, totalD); glTexCoord2f(W / 2.f, wallH / 2.f); glVertex3f(halfW, wallH, totalD); glTexCoord2f(0, wallH / 2.f); glVertex3f(-halfW, wallH, totalD); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(halfW, 0, totalD + bwT); glTexCoord2f(W / 2.f, 0); glVertex3f(-halfW, 0, totalD + bwT); glTexCoord2f(W / 2.f, wallH / 2.f); glVertex3f(-halfW, wallH, totalD + bwT); glTexCoord2f(0, wallH / 2.f); glVertex3f(halfW, wallH, totalD + bwT); glEnd();
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glBegin(GL_QUADS); glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-halfW, 0, totalD); glTexCoord2f(1, 0); glVertex3f(-halfW, 0, totalD + bwT); glTexCoord2f(1, 1); glVertex3f(-halfW, wallH, totalD + bwT); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH, totalD); glEnd();
    glBegin(GL_QUADS); glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(halfW, 0, totalD + bwT); glTexCoord2f(1, 0); glVertex3f(halfW, 0, totalD); glTexCoord2f(1, 1); glVertex3f(halfW, wallH, totalD); glTexCoord2f(0, 1); glVertex3f(halfW, wallH, totalD + bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-halfW, wallH, totalD); glTexCoord2f(1, 0); glVertex3f(halfW, wallH, totalD); glTexCoord2f(1, 1); glVertex3f(halfW, wallH, totalD + bwT); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH, totalD + bwT); glEnd();
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); solidColor(0.85f, 0.08f, 0.08f);
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-halfW, topY + 0.4f, totalD - 0.01f); glVertex3f(halfW, topY + 0.4f, totalD - 0.01f); glVertex3f(halfW, topY + 0.9f, totalD - 0.01f); glVertex3f(-halfW, topY + 0.9f, totalD - 0.01f); glEnd();
    restoreTexLit();
    const float roofOvhg = 1.0f, roofThk = 0.20f;
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-halfW, wallH + roofThk, -roofOvhg); glTexCoord2f(W / 2.f, 0); glVertex3f(halfW, wallH + roofThk, -roofOvhg); glTexCoord2f(W / 2.f, 1); glVertex3f(halfW, wallH + roofThk, totalD + bwT); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH + roofThk, totalD + bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, -1, 0); glTexCoord2f(0, 0); glVertex3f(-halfW, wallH, -roofOvhg); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH, totalD + bwT); glTexCoord2f(W / 2.f, 1); glVertex3f(halfW, wallH, totalD + bwT); glTexCoord2f(W / 2.f, 0); glVertex3f(halfW, wallH, -roofOvhg); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(-halfW, wallH, -roofOvhg); glTexCoord2f(1, 0); glVertex3f(halfW, wallH, -roofOvhg); glTexCoord2f(1, 1); glVertex3f(halfW, wallH + roofThk, -roofOvhg); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH + roofThk, -roofOvhg); glEnd();
    glBegin(GL_QUADS); glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-halfW, wallH, -roofOvhg); glTexCoord2f(0, 1); glVertex3f(-halfW, wallH, totalD + bwT); glTexCoord2f(1, 1); glVertex3f(-halfW, wallH + roofThk, totalD + bwT); glTexCoord2f(1, 0); glVertex3f(-halfW, wallH + roofThk, -roofOvhg); glEnd();
    glBegin(GL_QUADS); glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(halfW, wallH, totalD + bwT); glTexCoord2f(0, 1); glVertex3f(halfW, wallH, -roofOvhg); glTexCoord2f(1, 1); glVertex3f(halfW, wallH + roofThk, -roofOvhg); glTexCoord2f(1, 0); glVertex3f(halfW, wallH + roofThk, totalD + bwT); glEnd();
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); solidColor(0.82f, 0.06f, 0.06f);
    glBegin(GL_QUADS); glNormal3f(0, 1, 0); glVertex3f(-halfW, wallH + roofThk + 0.002f, -roofOvhg); glVertex3f(halfW, wallH + roofThk + 0.002f, -roofOvhg); glVertex3f(halfW, wallH + roofThk + 0.002f, -roofOvhg + W * 0.25f); glVertex3f(-halfW, wallH + roofThk + 0.002f, -roofOvhg + W * 0.25f); glEnd();
    restoreTexLit();
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    float colPos[4] = { -halfW + 0.5f,-halfW * 0.34f,halfW * 0.34f,halfW - 0.5f };
    for (int c = 0; c < 4; c++) drawCylinder(colPos[c], 0.f, -roofOvhg + 0.08f, 0.11f, wallH, 12);
    drawBox(-halfW + 0.4f, wallH - 0.02f, -roofOvhg + 0.03f, W - 0.8f, 0.16f, 0.22f);
    glEnable(GL_CULL_FACE); glPopMatrix();
}

void drawPitGarage(float cx, float cz, float rotY) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_concrete); drawBox(-1.8f, 0, -1.5f, 3.6f, 2.8f, 3.0f, 2.f, 2.f);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    solidColor(0.08f, 0.08f, 0.09f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-1.5f, 0.02f, -1.51f); glVertex3f(1.5f, 0.02f, -1.51f); glVertex3f(1.5f, 2.2f, -1.51f); glVertex3f(-1.5f, 2.2f, -1.51f); glEnd();
    solidColor(0.85f, 0.08f, 0.08f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-1.8f, 2.2f, -1.52f); glVertex3f(1.8f, 2.2f, -1.52f); glVertex3f(1.8f, 2.55f, -1.52f); glVertex3f(-1.8f, 2.55f, -1.52f); glEnd();
    restoreTexLit(); glBindTexture(GL_TEXTURE_2D, tex_concrete); drawBox(-2.0f, 2.8f, -1.8f, 4.0f, 0.15f, 3.4f, 2.f, 1.f); glPopMatrix();
}

void drawStartGantry() {
    float legZ = OUTER_Z + 0.4f, legX = ROAD_W + 1.5f, gantryY = 5.5f;
    glPushMatrix();
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    drawCylinder(-legX, 0, legZ, 0.22f, gantryY, 12); drawCylinder(legX, 0, legZ, 0.22f, gantryY, 12);
    drawBox(-legX - 0.15f, gantryY - 0.3f, legZ - 0.1f, 0.30f, 0.30f, 0.20f); drawBox(legX - 0.15f, gantryY - 0.3f, legZ - 0.1f, 0.30f, 0.30f, 0.20f);
    drawBox(-legX - 0.15f, gantryY, legZ - 0.15f, 2.f * (legX + 0.15f), 0.35f, 0.30f, 4.f, 1.f);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    float lx[5] = { -1.8f,-0.9f,0.f,0.9f,1.8f };
    for (int i = 0; i < 5; i++) { solidColor(0.18f, 0.04f, 0.04f); drawCylinder(lx[i], gantryY - 0.5f, legZ - 0.22f, 0.12f, 0.18f, 12); }
    solidColor(0.95f, 0.08f, 0.05f); drawCylinder(lx[0], gantryY - 0.5f, legZ - 0.22f, 0.10f, 0.16f, 12);
    restoreTexLit(); glBindTexture(GL_TEXTURE_2D, tex_concrete); drawCylinder(legX + 0.5f, 0, legZ + 0.3f, 0.045f, 3.2f, 8);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    float fW = 0.55f, fH = 0.40f, fx = legX + 0.53f, fy = 2.8f, fz = legZ + 0.3f, sqF = fW / 4.f;
    for (int fi = 0; fi < 4; fi++) for (int fj = 0; fj < 3; fj++) {
        glColor3f(((fi + fj) % 2 == 0) ? 0.f : 1.f, ((fi + fj) % 2 == 0) ? 0.f : 1.f, ((fi + fj) % 2 == 0) ? 0.f : 1.f);
        glBegin(GL_QUADS); glNormal3f(0, 0, 1); glVertex3f(fx + fi * sqF, fy + fj * fH / 3.f, fz); glVertex3f(fx + (fi + 1) * sqF, fy + fj * fH / 3.f, fz); glVertex3f(fx + (fi + 1) * sqF, fy + (fj + 1) * fH / 3.f, fz); glVertex3f(fx + fi * sqF, fy + (fj + 1) * fH / 3.f, fz); glEnd();
    }
    restoreTexLit(); glPopMatrix();
}

void drawPodium(float cx, float cz) {
    glPushMatrix(); glTranslatef(cx, 0, cz);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    drawBox(-0.7f, 0, -0.7f, 1.4f, 1.2f, 1.4f, 1.f, 1.f); drawBox(-2.5f, 0, -0.7f, 1.4f, 0.9f, 1.4f, 1.f, 1.f); drawBox(1.1f, 0, -0.7f, 1.4f, 0.65f, 1.4f, 1.f, 1.f);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    solidColor(0.95f, 0.85f, 0.12f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-0.35f, 0.85f, -0.71f); glVertex3f(0.35f, 0.85f, -0.71f); glVertex3f(0.35f, 1.15f, -0.71f); glVertex3f(-0.35f, 1.15f, -0.71f); glEnd();
    solidColor(0.75f, 0.75f, 0.78f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-2.15f, 0.55f, -0.71f); glVertex3f(-1.45f, 0.55f, -0.71f); glVertex3f(-1.45f, 0.85f, -0.71f); glVertex3f(-2.15f, 0.85f, -0.71f); glEnd();
    solidColor(0.72f, 0.44f, 0.18f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(1.45f, 0.30f, -0.71f); glVertex3f(2.15f, 0.30f, -0.71f); glVertex3f(2.15f, 0.60f, -0.71f); glVertex3f(1.45f, 0.60f, -0.71f); glEnd();
    restoreTexLit(); glPopMatrix();
}

void drawBarrier(float cx, float cz, float rotY = 0.f) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_barrier); glDisable(GL_CULL_FACE);
    const float bh = 0.65f, bwT = 0.16f, bwB = 0.30f, bL = 1.0f;
    glBegin(GL_QUADS); glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(-bL, 0, bwB); glTexCoord2f(2, 0); glVertex3f(bL, 0, bwB); glTexCoord2f(2, 1); glVertex3f(bL, bh, bwT); glTexCoord2f(0, 1); glVertex3f(-bL, bh, bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(bL, 0, -bwB); glTexCoord2f(2, 0); glVertex3f(-bL, 0, -bwB); glTexCoord2f(2, 1); glVertex3f(-bL, bh, -bwT); glTexCoord2f(0, 1); glVertex3f(bL, bh, -bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-bL, bh, bwT); glTexCoord2f(2, 0); glVertex3f(bL, bh, bwT); glTexCoord2f(2, 1); glVertex3f(bL, bh, -bwT); glTexCoord2f(0, 1); glVertex3f(-bL, bh, -bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(bL, 0, bwB); glTexCoord2f(1, 0); glVertex3f(bL, 0, -bwB); glTexCoord2f(1, 1); glVertex3f(bL, bh, -bwT); glTexCoord2f(0, 1); glVertex3f(bL, bh, bwT); glEnd();
    glBegin(GL_QUADS); glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-bL, 0, -bwB); glTexCoord2f(1, 0); glVertex3f(-bL, 0, bwB); glTexCoord2f(1, 1); glVertex3f(-bL, bh, bwT); glTexCoord2f(0, 1); glVertex3f(-bL, bh, -bwT); glEnd();
    glEnable(GL_CULL_FACE); glPopMatrix();
}

void drawLamppost(float cx, float cz, float rotY = 0.f) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    drawCylinder(0, 0, 0, 0.07f, 0.5f, 8); drawCylinder(0, 0.5f, 0, 0.055f, 3.5f, 8);
    drawBox(-0.04f, 3.8f, 0.f, 0.08f, 0.08f, 0.85f); drawBox(-0.18f, 3.68f, 0.68f, 0.36f, 0.12f, 0.24f);
    solidColor(1.f, 0.97f, 0.82f);
    glBegin(GL_QUADS); glNormal3f(0, -1, 0); glVertex3f(-.14f, 3.68f, .70f); glVertex3f(.14f, 3.68f, .70f); glVertex3f(.14f, 3.68f, .90f); glVertex3f(-.14f, 3.68f, .90f); glEnd();
    glDisable(GL_LIGHTING); glColor3f(1.f, 0.96f, 0.75f);
    glPushMatrix(); glTranslatef(0, 3.74f, 0.80f);
    GLUquadric* q = gluNewQuadric(); gluSphere(q, 0.10f, 10, 8); gluDeleteQuadric(q);
    glPopMatrix(); restoreTexLit(); glPopMatrix();
}

void drawFencePanel(float cx, float cz, float rotY = 0.f) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_concrete); glDisable(GL_CULL_FACE);
    drawCylinder(-1.f, 0, 0, 0.04f, 1.8f, 6); drawCylinder(1.f, 0, 0, 0.04f, 1.8f, 6); drawBox(-1.f, 1.72f, -.03f, 2.f, .05f, .06f);
    glBegin(GL_QUADS); glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(-1, 0, 0); glTexCoord2f(2, 0); glVertex3f(1, 0, 0); glTexCoord2f(2, 1.8f); glVertex3f(1, 1.8f, 0); glTexCoord2f(0, 1.8f); glVertex3f(-1, 1.8f, 0); glEnd();
    glBegin(GL_QUADS); glNormal3f(0, 0, -1); glTexCoord2f(2, 0); glVertex3f(1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-1, 0, 0); glTexCoord2f(0, 1.8f); glVertex3f(-1, 1.8f, 0); glTexCoord2f(2, 1.8f); glVertex3f(1, 1.8f, 0); glEnd();
    glEnable(GL_CULL_FACE); glPopMatrix();
}

void drawCar(float cx, float cz, float rotY = 0.f, float cr = 0.82f, float cg = 0.04f, float cb = 0.04f) {
    glPushMatrix(); glTranslatef(cx, ROAD_Y, cz); glRotatef(rotY, 0, 1, 0);
    solidColor(cr, cg, cb);
    drawBox(-1.65f, .06f, -.44f, 3.3f, .10f, .88f); drawBox(-1.20f, .16f, -.43f, 1.4f, .22f, .40f); drawBox(-1.20f, .16f, .03f, 1.4f, .22f, .40f); drawBox(-1.60f, .12f, -.17f, .42f, .11f, .34f);
    solidColor(cr * 0.18f, cg * 0.18f, cb * 0.18f); drawBox(-.38f, .16f, -.28f, .95f, .36f, .56f);
    solidColor(cr, cg, cb); drawBox(-.06f, .52f, -.15f, .70f, .26f, .30f);
    solidColor(.78f, .68f, .08f); drawBox(-.09f, .50f, -.06f, .05f, .34f, .12f); drawBox(-.09f, .82f, -.18f, .46f, .055f, .36f);
    solidColor(cr, cg, cb);
    drawBox(-1.65f, .07f, -.55f, .20f, .055f, 1.10f); drawBox(-1.65f, .125f, -.55f, .20f, .045f, 1.10f); drawBox(-1.65f, .07f, -.57f, .20f, .17f, .05f); drawBox(-1.65f, .07f, .52f, .20f, .17f, .05f);
    drawBox(1.18f, .60f, -.47f, .15f, .065f, .94f); drawBox(1.18f, .665f, -.47f, .15f, .05f, .94f); drawBox(1.18f, .36f, -.49f, .15f, .30f, .055f); drawBox(1.18f, .36f, .43f, .15f, .30f, .055f); drawBox(1.24f, .16f, -.07f, .09f, .46f, .14f);
    solidColor(0.95f, 0.95f, 0.95f); drawBox(-0.55f, .33f, -.285f, .35f, .22f, .01f);
    restoreTexLit(); solidColor(.12f, .12f, .12f);
    struct { float wx, wz; }W[4] = { {-1.22f,-.54f},{-1.22f,.54f},{1.04f,-.54f},{1.04f,.54f} };
    for (auto& w : W) { glPushMatrix(); glTranslatef(w.wx, .22f, w.wz); glRotatef(90, 1, 0, 0); drawCylinder(0, -.15f, 0, .22f, .30f, 16, 2.f); solidColor(0.65f, 0.65f, 0.68f); drawCylinder(0, -.13f, 0, .13f, .26f, 16, 1.f); glPopMatrix(); }
    restoreTexLit(); glPopMatrix();
}

void drawMarshalPost(float cx, float cz, float rotY = 0.f) {
    glPushMatrix(); glTranslatef(cx, 0, cz); glRotatef(rotY, 0, 1, 0);
    glBindTexture(GL_TEXTURE_2D, tex_concrete); drawBox(-0.5f, 0, -0.4f, 1.0f, 1.5f, 0.8f, 1.f, 1.f);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING);
    solidColor(0.85f, 0.08f, 0.08f); glBegin(GL_QUADS); glNormal3f(0, 1, 0); glVertex3f(-0.6f, 1.5f, -0.5f); glVertex3f(0.6f, 1.5f, -0.5f); glVertex3f(0.6f, 1.5f, 0.9f); glVertex3f(-0.6f, 1.5f, 0.9f); glEnd();
    solidColor(0.5f, 0.7f, 0.9f); glBegin(GL_QUADS); glNormal3f(0, 0, -1); glVertex3f(-0.25f, 0.6f, -0.41f); glVertex3f(0.25f, 0.6f, -0.41f); glVertex3f(0.25f, 1.1f, -0.41f); glVertex3f(-0.25f, 1.1f, -0.41f); glEnd();
    restoreTexLit(); glBindTexture(GL_TEXTURE_2D, tex_concrete); drawCylinder(0.6f, 0, 0, 0.03f, 2.2f, 6);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); solidColor(0.95f, 0.85f, 0.05f);
    glBegin(GL_QUADS); glNormal3f(0, 0, 1); glVertex3f(0.63f, 1.6f, 0); glVertex3f(1.1f, 1.6f, 0); glVertex3f(1.1f, 2.1f, 0); glVertex3f(0.63f, 2.1f, 0); glEnd();
    restoreTexLit(); glPopMatrix();
}

void drawCone(float cx, float cz) {
    glPushMatrix(); glTranslatef(cx, ROAD_Y, cz); solidColor(1.f, .42f, 0.f);
    int s = 12; float r = .085f, h = .26f;
    glBegin(GL_TRIANGLE_FAN); glVertex3f(0, h, 0);
    for (int i = 0; i <= s; i++) { float a = 2.f * (float)M_PI * i / s; glNormal3f(cosf(a) * h, r, sinf(a) * h); glVertex3f(r * cosf(a), 0, r * sinf(a)); }glEnd();
    glColor3f(1, 1, 1); float sh = h * .42f, sr = r * (1 - .42f) + .005f, sw = h * .10f;
    glBegin(GL_QUAD_STRIP); for (int i = 0; i <= s; i++) { float a = 2.f * (float)M_PI * i / s; glVertex3f(sr * cosf(a), sh, sr * sinf(a)); glVertex3f(sr * cosf(a), sh + sw, sr * sinf(a)); }glEnd();
    glColor3f(.15f, .15f, .15f); glBegin(GL_TRIANGLE_FAN); glNormal3f(0, -1, 0); glVertex3f(0, 0, 0);
    for (int i = s; i >= 0; i--) { float a = 2.f * (float)M_PI * i / s; glVertex3f(r * 1.1f * cosf(a), 0, r * 1.1f * sinf(a)); }glEnd();
    restoreTexLit(); glPopMatrix();
}

void drawTyreStack(float cx, float cz) {
    glPushMatrix(); glTranslatef(cx, 0, cz); solidColor(.10f, .10f, .10f);
    for (int i = 0; i < 4; i++) { glPushMatrix(); glTranslatef(0, (float)i * .22f, 0); glRotatef(90, 1, 0, 0); drawCylinder(0, -.18f, 0, .22f, .18f, 16, 2.f); glPopMatrix(); }
    solidColor(.88f, .78f, 0.f); glPushMatrix(); glTranslatef(0, .26f, 0); glRotatef(90, 1, 0, 0); drawCylinder(0, -.09f, 0, .235f, .04f, 16, 1.f); glPopMatrix();
    restoreTexLit(); glPopMatrix();
}

void drawStaticObjects() {
    drawBuilding(-22.f, -22.f, 0.f, 5.f, 12.f, 4.f); drawBuilding(22.f, -22.f, 90.f, 5.f, 10.f, 4.f);
    drawBuilding(-22.f, 22.f, 270.f, 6.f, 14.f, 4.f); drawBuilding(22.f, 22.f, 180.f, 5.f, 11.f, 4.f);
    drawBuilding(-22.f, 0.f, 90.f, 4.f, 8.f, 3.f); drawBuilding(22.f, 0.f, 270.f, 4.f, 9.f, 3.f);
    drawBuilding(-20.f, -15.f, 30.f, 3.5f, 6.f, 3.f); drawBuilding(20.f, 15.f, 200.f, 3.5f, 7.f, 3.f);
    drawGrandstand(0.f, OUTER_Z + 1.8f, 0.f); drawGrandstand(0.f, -(OUTER_Z + 1.8f), 180.f);
    for (int i = -2; i <= 2; i++) drawPitGarage((float)i * 3.8f, 0.0f, 0.f);
    drawStartGantry(); drawPodium(-5.0f, OUTER_Z + 1.2f);
    drawHoarding(-7.f, OUTER_Z + 0.8f, 0.f, 2.5f, 1.1f, 0.85f, 0.06f, 0.06f, 0.96f, 0.96f, 0.96f);
    drawHoarding(-3.f, OUTER_Z + 0.8f, 0.f, 2.5f, 1.1f, 0.04f, 0.18f, 0.60f, 0.96f, 0.96f, 0.96f);
    drawHoarding(2.f, OUTER_Z + 0.8f, 0.f, 2.5f, 1.1f, 0.06f, 0.50f, 0.14f, 0.96f, 0.96f, 0.96f);
    drawHoarding(6.f, OUTER_Z + 0.8f, 0.f, 2.5f, 1.1f, 0.88f, 0.62f, 0.02f, 0.12f, 0.12f, 0.12f);
    drawHoarding(-5.f, -OUTER_Z - 0.8f, 180.f, 2.0f, 0.9f, 0.04f, 0.18f, 0.60f, 0.96f, 0.96f, 0.96f);
    drawHoarding(5.f, -OUTER_Z - 0.8f, 180.f, 2.0f, 0.9f, 0.85f, 0.06f, 0.06f, 0.96f, 0.96f, 0.96f);
    drawHoarding(-5.f, 1.5f, 180.f, 2.0f, 0.9f, 0.04f, 0.18f, 0.60f, 0.96f, 0.96f, 0.96f);
    drawHoarding(5.f, 1.5f, 180.f, 2.0f, 0.9f, 0.85f, 0.06f, 0.06f, 0.96f, 0.96f, 0.96f);
    drawMarshalPost(-13.f, 0.f, 270.f); drawMarshalPost(13.f, 0.f, 90.f);
    drawMarshalPost(-7.f, OUTER_Z + 0.7f, 0.f); drawMarshalPost(7.f, OUTER_Z + 0.7f, 0.f);
    drawMarshalPost(-7.f, -OUTER_Z - 0.7f, 0.f); drawMarshalPost(7.f, -OUTER_Z - 0.7f, 0.f);
    struct T { float x, z, r, s; } trees[] = {
        {-12,19,20,1.0f},{-6,19.5f,70,1.1f},{0,19,140,1.2f},{6,19.5f,200,.9f},{12,19,300,1.0f},{-9,20.5f,50,1.0f},{-3,20.5f,110,1.1f},{3,20.5f,250,.9f},{9,20.5f,330,1.0f},
        {-12,-19,80,1.0f},{-6,-19.5f,130,1.1f},{0,-19,200,1.2f},{6,-19.5f,260,.9f},{12,-19,350,1.0f},{-9,-20.5f,160,1.0f},{-3,-20.5f,210,1.1f},{3,-20.5f,290,.9f},{9,-20.5f,40,1.0f},
        {-18.8f,-11,115,1.0f},{-18.8f,-9,255,1.1f},{-18.8f,-7,175,.9f},{-18.8f,-5,45,1.0f},{-18.8f,5,330,1.0f},{-18.8f,7,200,1.1f},{-18.8f,9,60,.9f},{-18.8f,11,310,1.0f},
        {18.8f,-11,225,1.0f},{18.8f,-9,95,1.1f},{18.8f,-7,335,.9f},{18.8f,-5,155,1.0f},{18.8f,5,10,1.0f},{18.8f,7,180,1.1f},{18.8f,9,70,.9f},{18.8f,11,280,1.0f}
    };
    for (auto& t : trees) drawTree(t.x, t.z, t.r, t.s);
    int nB = (int)(2.f * RX / 2.0f) + 1;
    for (int i = 0; i < nB; i++) {
        float bx = -RX + (float)i * 2.f;
        drawBarrier(bx, OUTER_Z + 0.32f, 0.f); drawBarrier(bx, INNER_Z - 0.32f, 0.f);
        drawBarrier(bx, -OUTER_Z - 0.32f, 0.f); drawBarrier(bx, -INNER_Z + 0.32f, 0.f);
    }
    for (int i = -2; i <= 2; i++) { float lx = (float)i * 5.f; drawLamppost(lx, OUTER_Z + 1.1f, 180.f); drawLamppost(lx, -OUTER_Z - 1.1f, 0.f); }
    int fN = (int)(2.f * FENCE / 2.f); float fS = 2.f * FENCE / (float)fN;
    for (int i = 0; i < fN; i++) { float t = -FENCE + fS * i + fS * .5f; drawFencePanel(t, -FENCE, 0.f); drawFencePanel(t, FENCE, 180.f); drawFencePanel(-FENCE, t, 90.f); drawFencePanel(FENCE, t, 270.f); }
    for (int i = 0; i < 6; i++) { float a = (float)M_PI * (-50.f + (float)i * 20.f) / 180.f, tr = INNER_Z - 0.7f; drawTyreStack(RX + tr * cosf(a), tr * sinf(a)); drawTyreStack(-RX + tr * cosf(a), tr * sinf(a)); }
    for (int i = -4; i <= 4; i++) drawCone((float)i * .75f, 1.5f);
    drawCar(0.0f, RZ, 90.f, 0.82f, 0.04f, 0.04f);
}

void getLookDir(float& dx, float& dy, float& dz) {
    float yr = yaw * (float)M_PI / 180.f, pr = pitch * (float)M_PI / 180.f;
    dx = cosf(pr) * sinf(yr); dy = sinf(pr); dz = cosf(pr) * cosf(yr);
}

static void applyDayNight() {
    if (!nightMode) {
        glClearColor(.70f, .78f, .88f, 1.f);
        GLfloat a0[] = { 0.30f,0.32f,0.36f,1.f }, d0[] = { 1.05f,0.98f,0.88f,1.f }, s0[] = { 0.40f,0.38f,0.30f,1.f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, a0); glLightfv(GL_LIGHT0, GL_DIFFUSE, d0); glLightfv(GL_LIGHT0, GL_SPECULAR, s0);
        glEnable(GL_LIGHT0);
        GLfloat a1[] = { 0.10f,0.12f,0.15f,1.f }, d1[] = { 0.18f,0.20f,0.25f,1.f };
        glLightfv(GL_LIGHT1, GL_AMBIENT, a1); glLightfv(GL_LIGHT1, GL_DIFFUSE, d1);
        glEnable(GL_LIGHT1);
        for (int i = 0; i < NL; i++) {
            GLenum l = lamps[i].gl;
            GLfloat ld[] = { 0.30f,0.28f,0.22f,1.f }, la[] = { 0.00f,0.00f,0.00f,1.f };
            glLightfv(l, GL_DIFFUSE, ld); glLightfv(l, GL_AMBIENT, la); glEnable(l);
        }
        GLfloat ga[] = { 0.18f,0.20f,0.24f,1.f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
        GLfloat fc[] = { 0.70f,0.78f,0.86f,1.f };
        glFogfv(GL_FOG_COLOR, fc); glFogf(GL_FOG_DENSITY, 0.013f);
    }
    else {
     
        glClearColor(0.02f, 0.03f, 0.08f, 1.f);

        GLfloat overPos[] = { 0.f,1.f,0.f,0.f }; 
        GLfloat overAmb[] = { 0.05f,0.06f,0.10f,1.f };
        GLfloat overDif[] = { 0.22f,0.20f,0.15f,1.f };
        GLfloat overSpc[] = { 0.f,0.f,0.f,1.f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, overAmb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, overDif);
        glLightfv(GL_LIGHT0, GL_SPECULAR, overSpc);
        glLightfv(GL_LIGHT0, GL_POSITION, overPos);
        glEnable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        for (int i = 0; i < NL; i++) glDisable(lamps[i].gl);

        GLfloat ga[] = { 0.38f,0.30f,0.12f,1.f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

        GLfloat fc[] = { 0.02f,0.03f,0.08f,1.f };
        glFogfv(GL_FOG_COLOR, fc); glFogf(GL_FOG_DENSITY, 0.015f);
    }
}


static void drawLampGlow(float cx, float cz) {
    const int S = 24;
    float rx = 5.0f, rz = 4.0f;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.92f, 0.72f, 0.18f, 0.55f); glVertex3f(cx, ROAD_Y + 0.004f, cz);
    glColor4f(0.85f, 0.55f, 0.05f, 0.0f);
    for (int i = 0; i <= S; i++) {
        float a = 2.f * (float)M_PI * i / S;
        glVertex3f(cx + rx * cosf(a), ROAD_Y + 0.004f, cz + rz * sinf(a));
    }
    glEnd();
    float hr = 1.4f;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.f, 0.95f, 0.65f, 0.70f); glVertex3f(cx, ROAD_Y + 0.005f, cz);
    glColor4f(1.f, 0.80f, 0.20f, 0.0f);
    for (int i = 0; i <= S; i++) {
        float a = 2.f * (float)M_PI * i / S;
        glVertex3f(cx + hr * cosf(a), ROAD_Y + 0.005f, cz + hr * sinf(a));
    }
    glEnd();
}
static void drawNightGlow() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);      glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = -2; i <= 2; i++) {
        float lx = (float)i * 5.f;
        drawLampGlow(lx, OUTER_Z - 0.8f);
        drawLampGlow(lx, -OUTER_Z + 0.8f);
    }
    glDepthMask(GL_TRUE);
    glPopAttrib();
}
void display() {
    applyDayNight();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float dx, dy, dz; getLookDir(dx, dy, dz);
    gluLookAt(camX, camY, camZ, camX + dx, camY + dy, camZ + dz, 0, 1, 0);
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);
    for (int i = 0; i < NL; i++) { GLfloat lp[4] = { lamps[i].x,lamps[i].y,lamps[i].z,1.f }; glLightfv(lamps[i].gl, GL_POSITION, lp); }
    glColor3f(1, 1, 1);
    drawSkybox();
    glEnable(GL_LIGHTING);
    drawGround(); drawRoadCircuit(); drawStaticObjects();
    if (nightMode) drawNightGlow();
    if (!nightMode) {
        renderShadows();
    }
    else {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_FOG);
        glDisable(GL_CULL_FACE); glDepthMask(GL_FALSE);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.f, 0.f, 0.f, 0.22f);
        float sLights[3][3] = { {-8.f,9.f,0.f},{0.f,9.f,0.f},{8.f,9.f,0.f} };
        for (int li = 0; li < 3; li++) {
            float M[16];
            buildShadowMatrix(M, sLights[li][0], sLights[li][1], sLights[li][2], 1.f, 0.01f);
            glPushMatrix(); glMultMatrixf(M);
            for (int i = -2; i <= 2; i++) {
                shadowCyl((float)i * 5.f, OUTER_Z + 1.1f, 0.09f, 4.2f);
                shadowCyl((float)i * 5.f, -OUTER_Z - 1.1f, 0.09f, 4.2f);
            }
            shadowBox(-1.65f, ROAD_Y, -0.44f, 3.3f, 0.9f, 0.88f);
            glPopMatrix();
        }
        glDepthMask(GL_TRUE);
        glPopAttrib();
    }
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(62.0, (double)w / h, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int, int) {
    float dx, dy, dz; getLookDir(dx, dy, dz); float rx = dz, rz = -dx;
    switch (key) {
    case'w':case'W':camX += dx * speed; camZ += dz * speed; break;
    case's':case'S':camX -= dx * speed; camZ -= dz * speed; break;
    case'd':case'D':camX -= rx * speed; camZ -= rz * speed; break;
    case'a':case'A':camX += rx * speed; camZ += rz * speed; break;
    case' ':camY += speed; break;
    case'c':case'C':camY -= speed; break;
    case'+':speed *= 1.3f; break;
    case'-':speed /= 1.3f; break;
    case 27:exit(0);
    case'n':case'N':nightMode = !nightMode; printf("Mode: %s\n", nightMode ? "NIGHT" : "DAY"); break;
    }
    const float L = 35.f;
    if (camX > L)camX = L; if (camX < -L)camX = -L;
    if (camZ > L)camZ = L; if (camZ < -L)camZ = -L;
    if (camY < .4f)camY = .4f; if (camY > 30.f)camY = 30.f;
    glutPostRedisplay();
}

void specialKeys(int key, int, int) {
    switch (key) {
    case GLUT_KEY_LEFT: yaw += 3.f; break;
    case GLUT_KEY_RIGHT:yaw -= 3.f; break;
    case GLUT_KEY_UP:   pitch += 2.f; break;
    case GLUT_KEY_DOWN: pitch -= 2.f; break;
    }
    if (pitch > 89.f)pitch = 89.f; if (pitch < -89.f)pitch = -89.f;
    glutPostRedisplay();
}

void mouseButton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) { mouseLeft = (state == GLUT_DOWN); mouseLastX = x; mouseLastY = y; }
    if (button == GLUT_RIGHT_BUTTON) { mouseRight = (state == GLUT_DOWN); mouseLastX = x; mouseLastY = y; }
}

void mouseMotion(int x, int y) {
    int dx = x - mouseLastX, dy = y - mouseLastY; mouseLastX = x; mouseLastY = y;
    if (mouseLeft) { yaw -= (float)dx * mouseSens; pitch += (float)dy * mouseSens; if (pitch > 89.f)pitch = 89.f; if (pitch < -89.f)pitch = -89.f; glutPostRedisplay(); }
    if (mouseRight) { camY -= (float)dy * mouseYSens; if (camY < .4f)camY = .4f; if (camY > 30.f)camY = 30.f; glutPostRedisplay(); }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("F1 Street Circuit – P3: Camera/Lighting/Shadows");
    glClearColor(.70f, .78f, .88f, 1.f);
    glEnable(GL_DEPTH_TEST); glEnable(GL_NORMALIZE);
    initTextures(); initLighting(); initFog();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard); glutSpecialFunc(specialKeys);
    glutMouseFunc(mouseButton); glutMotionFunc(mouseMotion);
    printf("\nControls: WASD=move  Space/C=up/down  Arrows=look\n");
    printf("Left-drag=mouselook  Right-drag=vert-pan  +/-=speed  ESC=exit\n");
    printf("N = toggle Night / Day mode\n");
    printf("Lights: LIGHT0=sun  LIGHT1=fill  LIGHT2-4=front straight  LIGHT5-6=back straight\n\n");
    glutMainLoop();
    return 0;
}