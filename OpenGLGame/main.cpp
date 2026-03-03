#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <freeglut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int WIDTH = 800, HEIGHT = 600;

float camX = 0.0f, camY = 2.0f, camZ = 8.0f;
float yaw = 180.0f; 
float pitch = -10.0f;
float speed = 0.2f;

GLuint texGround, texHorizon;

void getLookDir(float& dx, float& dy, float& dz) {
    float yr = yaw * 3.14159f / 180.0f;
    float pr = pitch * 3.14159f / 180.0f;
    dx = cosf(pr) * sinf(yr);
    dy = sinf(pr);
    dz = cosf(pr) * cosf(yr);
}

GLuint loadTexture(const char* path, bool repeat = true) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(1);

    bool isHDR = stbi_is_hdr(path);
    void* data = isHDR
        ? (void*)stbi_loadf(path, &w, &h, &channels, 3)
        : (void*)stbi_load(path, &w, &h, &channels, 3);

    if (!data) {
        printf("[ERROR] Cannot load '%s': %s\n", path, stbi_failure_reason());
        exit(1);
    }

    if (isHDR) {
        float* f = (float*)data;
        for (int i = 0; i < w * h * 3; i++)
            f[i] = f[i] / (f[i] + 1.0f);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP);

    if (isHDR)
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_FLOAT, data);
    else
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    printf("[OK] Loaded '%s' (%dx%d)\n", path, w, h);
    return tex;
}

void initTextures() {
    glEnable(GL_TEXTURE_2D);
    texHorizon = loadTexture("panorama.hdr");
    texGround = loadTexture("grass.jpg");
}

void initFog() {
    GLfloat fogColor[] = { 0.75f, 0.85f, 0.95f, 1.0f };
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, 0.035f);
    glHint(GL_FOG_HINT, GL_NICEST);
}

void drawSkybox() {
    float S = 20.0f;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);

    glBindTexture(GL_TEXTURE_2D, texHorizon);

    // TOP
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.60f); glVertex3f(-S, S, -S);
    glTexCoord2f(1.0f, 0.60f); glVertex3f(S, S, -S);
    glTexCoord2f(1.0f, 1.00f); glVertex3f(S, S, S);
    glTexCoord2f(0.0f, 1.00f); glVertex3f(-S, S, S);
    glEnd();

    // FRONT
    glBegin(GL_QUADS);
    glTexCoord2f(0.00f, 0.30f); glVertex3f(-S, -S, -S);
    glTexCoord2f(0.25f, 0.30f); glVertex3f(S, -S, -S);
    glTexCoord2f(0.25f, 0.72f); glVertex3f(S, S, -S);
    glTexCoord2f(0.00f, 0.72f); glVertex3f(-S, S, -S);
    glEnd();

    // RIGHT 
    glBegin(GL_QUADS);
    glTexCoord2f(0.25f, 0.30f); glVertex3f(S, -S, -S);
    glTexCoord2f(0.50f, 0.30f); glVertex3f(S, -S, S);
    glTexCoord2f(0.50f, 0.72f); glVertex3f(S, S, S);
    glTexCoord2f(0.25f, 0.72f); glVertex3f(S, S, -S);
    glEnd();

    // BACK 
    glBegin(GL_QUADS);
    glTexCoord2f(0.50f, 0.30f); glVertex3f(S, -S, S);
    glTexCoord2f(0.75f, 0.30f); glVertex3f(-S, -S, S);
    glTexCoord2f(0.75f, 0.72f); glVertex3f(-S, S, S);
    glTexCoord2f(0.50f, 0.72f); glVertex3f(S, S, S);
    glEnd();

    // LEFT 
    glBegin(GL_QUADS);
    glTexCoord2f(0.75f, 0.30f); glVertex3f(-S, -S, S);
    glTexCoord2f(1.00f, 0.30f); glVertex3f(-S, -S, -S);
    glTexCoord2f(1.00f, 0.72f); glVertex3f(-S, S, -S);
    glTexCoord2f(0.75f, 0.72f); glVertex3f(-S, S, S);
    glEnd();

    // BOTTOM
    glBindTexture(GL_TEXTURE_2D, texGround);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-S, -S, -S);
    glTexCoord2f(4, 0); glVertex3f(S, -S, -S);
    glTexCoord2f(4, 4); glVertex3f(S, -S, S);
    glTexCoord2f(0, 4); glVertex3f(-S, -S, S);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
}

void drawGround() {
    float S = 20.0f;
    glBindTexture(GL_TEXTURE_2D, texGround);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(-S, 0, -S);
    glTexCoord2f(8, 0); glVertex3f(S, 0, -S);
    glTexCoord2f(8, 8); glVertex3f(S, 0, S);
    glTexCoord2f(0, 8); glVertex3f(-S, 0, S);
    glEnd();
}

void drawTerrain() {
    int GRID = 40;
    float SIZE = 8.0f, step = SIZE * 2.0f / GRID;
    glBindTexture(GL_TEXTURE_2D, texGround);

    for (int i = 0; i < GRID; i++)
        for (int j = 0; j < GRID; j++) {
            float x0 = -SIZE + j * step, z0 = -SIZE + i * step;
            float x1 = x0 + step, z1 = z0 + step;

            auto H = [](float x, float z) -> float {
                return 2.5f * expf(-(x * x + z * z) / 8.0f)
                    + 1.2f * expf(-((x - 3) * (x - 3) + (z + 2) * (z + 2)) / 3.0f);
                };

            float u0 = (float)j / GRID * 4, v0 = (float)i / GRID * 4;
            float u1 = (float)(j + 1) / GRID * 4, v1 = (float)(i + 1) / GRID * 4;

            glBegin(GL_QUADS);
            glTexCoord2f(u0, v0); glVertex3f(x0, H(x0, z0), z0);
            glTexCoord2f(u1, v0); glVertex3f(x1, H(x1, z0), z0);
            glTexCoord2f(u1, v1); glVertex3f(x1, H(x1, z1), z1);
            glTexCoord2f(u0, v1); glVertex3f(x0, H(x0, z1), z1);
            glEnd();
        }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float dx, dy, dz;
    getLookDir(dx, dy, dz);
    gluLookAt(camX, camY, camZ,
        camX + dx, camY + dy, camZ + dz,
        0, 1, 0);

    glColor3f(1, 1, 1);
    drawSkybox();
    drawGround();
    drawTerrain();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0, (double)w / h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}
void keyboard(unsigned char key, int x, int y) {
    float dx, dy, dz;
    getLookDir(dx, dy, dz);

    float rx = dz, rz = -dx;

    switch (key) {
    case 'w': case 'W':
        camX += dx * speed;
        camZ += dz * speed;
        break;
    case 's': case 'S':
        camX -= dx * speed;
        camZ -= dz * speed;
        break;
    case 'd': case 'D':
        camX -= rx * speed;
        camZ -= rz * speed;
        break;
    case 'a': case 'A':
        camX += rx * speed;
        camZ += rz * speed;
        break;
    case ' ':     
        camY += speed;
        break;
    case 'c': case 'C':
        camY -= speed;
        break;
    case 27: exit(0);  
    }

    float LIMIT = 18.0f;
    if (camX > LIMIT) camX = LIMIT;
    if (camX < -LIMIT) camX = -LIMIT;
    if (camZ > LIMIT) camZ = LIMIT;
    if (camZ < -LIMIT) camZ = -LIMIT;
    if (camY < 0.5f)   camY = 0.5f;
    if (camY > 18.0f)  camY = 18.0f;

    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_LEFT:  yaw += 3.0f; break; 
    case GLUT_KEY_RIGHT: yaw -= 3.0f; break; 
    case GLUT_KEY_UP:    pitch += 2.0f; break; 
    case GLUT_KEY_DOWN:  pitch -= 2.0f; break;
    }

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Game");
    glClearColor(0.75f, 0.85f, 0.95f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    initTextures();
    initFog();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMainLoop();
    return 0;
}