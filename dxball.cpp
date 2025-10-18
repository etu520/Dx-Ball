
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/glut.h>
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <iostream>

#pragma comment(lib, "winmm.lib")
using namespace std;

const int WIN_W = 800;
const int WIN_H = 600;
const char* HS_FILE = "highscores.txt";
const char* BRICK_TEX_FILE = "brick.png";
const float PI = 3.14159265358979323846f;

// ---- Types ----
enum Difficulty { EASY=0, MEDIUM=1, HARD=2 };
struct Brick { float x,y,w,h; bool alive; int colorIdx; GLuint texture; int shapeType; };
struct Reward { float x,y; int type; bool active; float speed; };
struct MissedReward { float x,y; int type; bool active; int timer; };
struct HS { string name; int score; };
struct Particle { float x,y,dx,dy,r,g,b,life; };



// ---- Globals ----

vector<Brick> bricks;
Reward reward = {0,0,0,false,0.012f};
MissedReward missed = {0, -0.92f, 0, false, 0};
vector<HS> highscores;
vector<Particle> particles;

Difficulty difficulty = MEDIUM;
bool playing = false;
bool bgmEnabled = true; 
bool paused = false;
bool showPauseMenu = false;
bool showHelp = false;

int score = 0;
int lives = 3;

float paddleW = 0.4f, paddleH = 0.06f, paddleX = -0.2f;
float ballX=0.0f, ballY=-0.3f, ballR=0.03f;
float ballDX=0.014f, ballDY=0.017f, baseBallSpeed = 0.014f;

Reward emptyReward = {0,0,0,false,0};
bool enteringName = false;
string nameBuffer; int pendingScoreForName = 0;
bool levelCleared=false; int levelClearTicks=0;
bool fireballActive=false; int fireballTicks=0;
int windowWidth=WIN_W, windowHeight=WIN_H;

float animTime = 0.0f;

GLuint brickTextures[3];  // তিনটি texture
bool brickTexLoadedArr[3] = {false, false, false};



bool bgmPlaying = false; // ensure bgm started only once
struct Button {
    float x, y, w, h;
     string label;
    int id;
    Button(float _x, float _y, float _w, float _h, string _label, int _id)
    : x(_x), y(_y), w(_w), h(_h), label(_label), id(_id) {}

};
struct SubButton {
    float x, y, w, h;
    int diff;
};
std::vector<Button> menuButtons;
std::vector<SubButton> diffButtons;
std::vector<Button> pauseButtons;
bool showMenu = true;          // show main menu on startup
bool showStartSubmenu = false; // show Easy/Medium/Hard buttons
bool showHighScoresPage = false;
bool showHelpPage = false;


// ---- Sound helpers ----
void playSoundFile(const char* file, DWORD flags = SND_ASYNC | SND_FILENAME) {
    PlaySoundA(file, NULL, flags);
}
void playBgmLoop(const char* file) {
    // play bgm in loop; track state to avoid restarting repeatedly
    PlaySoundA(file, NULL, SND_ASYNC | SND_LOOP | SND_FILENAME);
    bgmPlaying = true;
}


void stopBgm() {
    PlaySoundA(NULL, NULL, 0);
    bgmPlaying = false;
}


// ---- High scores IO ----
void loadHighScores() {
    highscores.clear();
    ifstream in(HS_FILE);
    if(!in.is_open()) {
        for(int i=0;i<5;i++) highscores.push_back({"---",0});
        return;
    }
    string n; int s;
    while(in >> n >> s) highscores.push_back({n,s});
    in.close();
    while((int)highscores.size() < 5) highscores.push_back({"---",0});
    sort(highscores.begin(), highscores.end(), [](const HS&a,const HS&b){return a.score>b.score;});
    if(highscores.size()>5) highscores.resize(5);
}
void saveHighScores() {
    ofstream out(HS_FILE);
    for(auto &h: highscores) out << h.name << " " << h.score << "\n";
    out.close();
}
bool isTopFive(int s) { return s > highscores.back().score; }
void pushHighScore(const string &n, int s) {
    highscores.push_back({n,s});
    sort(highscores.begin(), highscores.end(), [](const HS&a,const HS&b){return a.score>b.score;});
    if(highscores.size() > 5) highscores.resize(5);
    saveHighScores();
}

// ---- Texture loader (stb) ----
GLuint loadTexture(const char* filename) {
    stbi_set_flip_vertically_on_load(1);
    int w,h,channels;
    unsigned char* data = stbi_load(filename, &w, &h, &channels, 0);
    if(!data) return 0;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum fmt = (channels == 4) ? GL_RGBA : GL_RGB;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return tex;
}

// ---- Particles ----
void spawnParticles(float x, float y, int n=12) {
    for(int i=0;i<n;i++){
        Particle p;
        float a = ((float)(rand()%360)) * (PI/180.0f);
        float s = 0.012f + (rand()%100)/8000.0f;
        p.dx = cosf(a)*s; p.dy = sinf(a)*s;
        p.x = x; p.y = y;
        p.r = 1.0f; p.g = 0.4f + (rand()%50)/100.0f; p.b = 0.1f;
        p.life = 1.0f;
        particles.push_back(p);
    }
}
void updateParticles() {
    for(auto &p: particles) { p.x += p.dx; p.y += p.dy; p.life -= 0.02f; }
    particles.erase(remove_if(particles.begin(), particles.end(), [](const Particle &p){ return p.life <= 0.0f; }), particles.end());
}
void drawParticles() {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for(auto &p: particles) {
        glColor4f(p.r,p.g,p.b,p.life);
        float s = 0.02f * p.life;
        glBegin(GL_QUADS);
        glVertex2f(p.x-s,p.y-s); glVertex2f(p.x+s,p.y-s);
        glVertex2f(p.x+s,p.y+s); glVertex2f(p.x-s,p.y+s);
        glEnd();
    }
    glDisable(GL_BLEND);
}

// ---- Bricks layout (refined) ----
void layoutBricks(Difficulty d) {
    bricks.clear();
    int rows, cols;
    string texFile;

    if (d == EASY) { rows = 4; cols = 7; texFile = "brick.png"; }
    else if (d == MEDIUM) { rows = 5; cols = 9; texFile = "brick2.png"; }
    else { rows = 6; cols = 11; texFile = "brick3.png"; }

    // --- Texture load ---
    GLuint tex = loadTexture(texFile.c_str());
    bool texLoaded = (tex != 0);

    float bw = 1.8f / cols;      // brick width
    float bh = 0.12f;            // brick height
    float leftX = -0.9f;         
    float startY = 0.75f;

    for(int r = 0; r < rows; r++) {
        for(int c = 0; c < cols; c++) {
            Brick b;
            b.w = bw * 0.95f;  // slightly narrower for spacing
            b.h = bh * 0.95f;
            b.alive = true;
            b.texture = texLoaded ? tex : 0;
            b.colorIdx = r % 6;

            
            // --- Position by difficulty ---
            if(d==EASY){
                float shift = (r%2)*bw*0.25f;
                b.x = leftX + c*bw + shift;
                b.y = startY - r*(bh+0.02f);
            }
            else if(d==MEDIUM){
                // 🌊 wave + random offset
                float wave = 0.06f * sinf(c*0.6f + r*0.5f);
                float randOffset = ((rand()%100)/500.0f) - 0.1f; // -0.1..0.1
                b.x = leftX + c*bw + ((r%2)? bw*0.2f : 0.0f) + randOffset;
                b.y = startY - r*(bh+0.03f) + wave;
            }
            else{ // HARD
                // 🌟 circular/star/moon layout
                float angle = ((r*cols+c) * 2*PI) / (rows*cols);
                float radius = 0.5f + 0.25f * sinf(3*angle + r*0.5f);
                b.x = cos(angle)*radius;
                b.y = sin(angle)*radius + 0.25f;
                b.w *= 1.1f; b.h *= 1.1f;
            }
            // Keep inside window bounds
            if(b.x + b.w > 1.0f) b.x = 1.0f - b.w;
            if(b.x < -1.0f) b.x = -1.0f;
            if(b.y + b.h > 1.0f) b.y = 1.0f - b.h;
            if(b.y - b.h < -1.0f) b.y = -1.0f + b.h;

            bricks.push_back(b);
        }
    }
}


// ---- Game start/reset ----
void startGame(Difficulty d) {
    difficulty = d;
    playing = true; paused = false; showHelp = false; enteringName = false;
    score = 0; lives = 3;

    paddleH = 0.06f;
    if(d==EASY){ paddleW = 0.50f; baseBallSpeed = 0.0105f; }
    else if(d==MEDIUM){ paddleW = 0.40f; baseBallSpeed = 0.014f; }
    else { paddleW = 0.32f; baseBallSpeed = 0.018f; }

    paddleX = -paddleW/2.0f;
    ballR = 0.03f; ballX = 0.0f; ballY = -0.3f;
    ballDX = baseBallSpeed * ((rand()%2)?1.0f:-1.0f);
    ballDY = baseBallSpeed * 1.2f;

    // ✅ Load 3 brick textures only once
    for(int i=0; i<3; i++) {
        if(!brickTexLoadedArr[i]) {
            string filename = "brick" + to_string(i+1) + ".png";
            brickTextures[i] = loadTexture(filename.c_str());
            brickTexLoadedArr[i] = (brickTextures[i] != 0);
        }
    }

    // ✅ Layout bricks for selected difficulty
    layoutBricks(d);

    reward = emptyReward;
    missed.active = false;
    missed.timer = 0;
    particles.clear();
    levelCleared=false; fireballActive=false; fireballTicks=0;

    if(!bgmPlaying) playBgmLoop("bgm.wav");
}

// ---- Drawing helpers ----
void drawText(float x, float y, const std::string &text, 
              void *font = GLUT_BITMAP_HELVETICA_18, 
              bool bold = false, 
              float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    glColor3f(r, g, b); // set text color
    glRasterPos2f(x, y);

    for (char c : text) {
        glutBitmapCharacter(font, c);
        if (bold) {
            // Simulate bold by drawing slightly offset versions
            glRasterPos2f(x + 0.002f, y);
            glutBitmapCharacter(font, c);
            glRasterPos2f(x, y + 0.002f);
            glutBitmapCharacter(font, c);
            glRasterPos2f(x, y); // reset to normal
        }
    }
}

void drawFilledCircle(float cx, float cy, float r, int segs=36) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for(int i=0;i<=segs;i++){
        float a = (float)i/(float)segs * 2.0f * PI;
        glVertex2f(cx + cosf(a)*r, cy + sinf(a)*r);
    }
    glEnd();
}


void drawGradientText(float x, float y, const std::string &text, float scale = 0.0015f, 
                      std::vector<std::vector<float>> colors = {}) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);

    // default colors if empty
    if(colors.empty()) {
        colors = {
            {0.0f, 1.0f, 1.0f},   {0.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f},   {1.0f, 0.5f, 0.0f},
            {1.0f, 0.0f, 0.0f},   {1.0f, 0.0f, 1.0f},
            {0.5f, 0.0f, 1.0f}
        };
    }

    for (int i = 0; i < text.length(); ++i) {
        char c = text[i];
        // outline
        glColor3f(0,0,0);
        float offsets[8][2] = {{5,5},{-5,5},{5,-5},{-5,-5},{0,5},{0,-5},{5,0},{-5,0}};
        for(int j=0;j<8;j++){
            glPushMatrix();
            glTranslatef(offsets[j][0], offsets[j][1], 0.5f);
            glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, c);
            glPopMatrix();
        }
        // gradient fill
        int idx = i % colors.size();
        glColor3f(colors[idx][0], colors[idx][1], colors[idx][2]);
        glutStrokeCharacter(GLUT_STROKE_ROMAN, c);
        float advance = glutStrokeWidth(GLUT_STROKE_ROMAN, c);
        glTranslatef(advance*0.3f,0,0);
    }

    glPopMatrix();
}

void drawHeart(float x, float y, float size) {
    glColor3f(1.0f, 0.0f, 0.0f); // red color
    glBegin(GL_TRIANGLE_FAN);

    // Center point
    glVertex2f(x, y);

    // Heart shape using parametric equation
    for (float t = 0; t <= 2 * M_PI; t += 0.01f) {
        float xt = 16 * pow(sin(t), 3);
        float yt = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t);

        glVertex2f(x + xt * size * 0.05f, y + yt * size * 0.05f);
    }
    glEnd();
}

void setupMenuButtons() {
    float yStart = 0.25f;  // matches where you want buttons to appear
    float yGap = 0.15f;
    float w = 0.7f;
    float h = 0.12f;
    menuButtons =  {
        Button(0.0f, yStart, w, h, "Start", 1),
        Button(0.0f, yStart - yGap, w, h, "High Scores", 2),
        Button(0.0f, yStart - 2*yGap, w, h, "Help", 3),
        Button(0.0f, yStart - 3*yGap, w, h, "Exit", 4),
    
    };
}


void setupDiffButtons() {
    float yStart = 0.25f;
    float yGap = 0.18f;
    float w = 0.6f;
    float h = 0.12f;
    diffButtons = {
        {0.0f, yStart, w, h, 0},           // EASY
        {0.0f, yStart - yGap, w, h, 1},    // MEDIUM
        {0.0f, yStart - 2*yGap, w, h, 2},  // HARD
        {0.0f, yStart - 3*yGap, w, h, 3}   // BACK
    };
}

void setupPauseButtons() {
    pauseButtons.clear();
    float y = 0.2f;
    float gap = 0.25f;
    float bw = 0.5f, bh = 0.15f;

    pauseButtons.push_back({0.0f, y, bw, bh, "Resume", 1});
    pauseButtons.push_back({0.0f, y - gap, bw, bh, "Menu", 2});
    pauseButtons.push_back({0.0f, y - 2 * gap, bw, bh, "Exit", 3});
}



void drawUI() {
    // 🔄 UPDATED: HUD with High Score + Hearts ❤️
glColor3f(1, 1, 1);
char buf[128];
// Find highest score
int highScore = 0;
for (auto &hs : highscores) 
    if (hs.score > highScore) highScore = hs.score;

// Draw main info
sprintf(buf, "Score: %d   Highest: %d   Level: %s", score, highScore, difficulty == EASY ? "Easy" : (difficulty == MEDIUM ? "Medium" : "Hard"));
drawText(-0.9f, 0.9f, buf);

// Draw hearts for lives
float heartX = 0.5f;
for (int i = 0; i < lives; i++) {
    drawHeart(heartX + i * 0.12f, 0.88f, 0.05f);
}


}

// ---- Animated neon gradient background ----
void drawAnimatedBackground(float t) {
    // two slowly-changing parameters produce a pleasant neon shift
    float a = 0.5f + 0.5f * sinf(t * 0.45f);
    float b = 0.5f + 0.5f * cosf(t * 0.55f + 1.0f);
    float topR = 0.15f + 0.6f * a;
    float topG = 0.05f + 0.45f * b;
    float topB = 0.08f + 0.5f * (1.0f - a);
    float botR = 0.02f + 0.3f * (1.0f - b);
    float botG = 0.04f + 0.3f * a;
    float botB = 0.05f + 0.4f * b;
    glBegin(GL_QUADS);
    glColor3f(topR, topG, topB); glVertex2f(-1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
    glColor3f(botR, botG, botB); glVertex2f(1.0f, -1.0f); glVertex2f(-1.0f, -1.0f);
    glEnd();
}


// ---- Reward spawn ----
void spawnRewardRandomAt(float x, float y) {
    
    reward.active = true;
    reward.x = x; reward.y = y;
    reward.type = rand()%4; // 0 life,1 wide,2 fast,3 fireball
    reward.speed = 0.012f + (rand()%5)*0.002f;
}

// ---- Physics & logic ----
void checkCollisionsAndPhysics() {
    if(!playing || paused || enteringName || levelCleared) return;

    if(fireballActive) { if(--fireballTicks <= 0) fireballActive = false; }

    ballX += ballDX; ballY += ballDY;

    // walls
    if(ballX - ballR < -1.0f) { ballX = -1.0f + ballR; ballDX = -ballDX; }
    if(ballX + ballR > 1.0f)  { ballX = 1.0f - ballR;  ballDX = -ballDX; }
    if(ballY + ballR > 1.0f)  { ballY = 1.0f - ballR;  ballDY = -ballDY; }

    // bottom -> life lost
    if(ballY - ballR < -1.0f) {
        lives--;
        // play life sound for life lost
        playSoundFile("life.wav");
        if(lives <= 0) {
            // game over: stop bgm, play gameover sound and handle highscores
            stopBgm();
            playSoundFile("gameover.wav");
            if(isTopFive(score)) {
                enteringName = true;
                nameBuffer.clear();
                pendingScoreForName = score;
            } else {
                playing = false;
                showMenu = true; 
            }
        } else {
            // reset ball with short pause
            ballX = 0; ballY = -0.3f;
            ballDX = baseBallSpeed * ((rand()%2)?1.0f:-1.0f);
            ballDY = baseBallSpeed * 1.2f;
            paused = true;
            glutTimerFunc(800, [](int){ paused = false; }, 0);
        }
        return;
    }

    // paddle collision
    float paddleTop = -0.95f + paddleH;
    if(ballY - ballR <= paddleTop && ballY - ballR >= paddleTop - 0.03f) {
        if(ballX >= paddleX && ballX <= paddleX + paddleW) {
            float hit = (ballX - (paddleX + paddleW/2.0f)) / (paddleW/2.0f); // -1..1
            float angle = hit * (70.0f * PI / 180.0f);
            float speed = sqrtf(ballDX*ballDX + ballDY*ballDY);
            if(speed < baseBallSpeed*0.8f) speed = baseBallSpeed*0.8f;
            ballDX = speed * sinf(angle);
            ballDY = fabsf(speed * cosf(angle));
            score += 5;
            // intentionally no "hit.wav" on every bounce to avoid noisy repetition
        }
    }

    // bricks collision
    for(auto &b : bricks) {
        if(!b.alive) continue;
        if(ballX > b.x - ballR && ballX < b.x + b.w + ballR &&
           ballY < b.y + ballR && ballY > b.y - b.h - ballR) {
            b.alive = false;
            score += 10;
            spawnParticles(ballX, ballY, 14);
            // occasional reward spawn
            if((rand()%100) < 14) spawnRewardRandomAt(ballX, ballY);
            if(!fireballActive) ballDY = -ballDY;
            // gentle feedback for brick break
            playSoundFile("reward.wav");
            break;
        }
    }

    // reward falling & collection / miss
    if(reward.active) {
        reward.y -= reward.speed;
        if(reward.y < -1.05f) {
            // reward missed --> create bottom indicator that sticks
            missed.active = true;
            missed.x = reward.x;
            missed.type = reward.type;
            missed.y = -0.92f;
            missed.timer = 300; // visible ~5 seconds (at ~60fps)
            reward.active = false;
            // no sound on miss
        } else if(reward.y <= -0.95f + paddleH && reward.x >= paddleX && reward.x <= paddleX + paddleW) {
            // collected: play specific sounds
            if(reward.type == 0) { // life
                if(lives < 9) { lives++; playSoundFile("life.wav"); }
                else { score += 50; playSoundFile("reward.wav"); }
            } else if(reward.type == 1) { paddleW *= 1.25f; if(paddleW > 1.0f) paddleW = 1.0f; playSoundFile("reward.wav"); }
            else if(reward.type == 2) { ballDX *= 1.25f; ballDY *= 1.25f; float maxS = baseBallSpeed * 4.0f;
                if(fabs(ballDX) > maxS) ballDX = (ballDX>0?maxS:-maxS);
                if(fabs(ballDY) > maxS) ballDY = (ballDY>0?maxS:-maxS);
                playSoundFile("reward.wav");
            } else if(reward.type == 3) { fireballActive = true; fireballTicks = 600; playSoundFile("reward.wav"); }
            reward.active = false;
        }
    }

    // update missed timer
    if(missed.active) {
        if(--missed.timer <= 0) missed.active = false;
    }

    // win check
    bool anyAlive = false;
    for(auto &b : bricks) if(b.alive) { anyAlive = true; break; }
    if(!anyAlive) {
        levelCleared = true;
        levelClearTicks = 100;
        playSoundFile("reward.wav");
    }

    updateParticles();
}
void drawCircle(float cx, float cy, float r){
    glBegin(GL_LINE_LOOP);
    for(float t=0;t<=2*M_PI;t+=0.05f)
        glVertex2f(cx + r*cos(t), cy + r*sin(t));
    glEnd();
}

void drawMoonShape(float cx, float cy, float r){
    glBegin(GL_LINE_LOOP);
    for(float t=0;t<=2*M_PI;t+=0.05f)
        glVertex2f(cx + r*cos(t), cy + r*sin(t));
    glEnd();
    glBegin(GL_LINE_LOOP);
    for(float t=0;t<=2*M_PI;t+=0.05f)
        glVertex2f(cx + 0.3*r + 0.9*r*cos(t), cy + 0.0*r + 0.9*r*sin(t));
    glEnd();
}

void drawStarShape(float cx, float cy, float r){
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<10;i++){
        float angle = i*2*M_PI/10;
        float rad = (i%2==0)? r : r*0.5;
        glVertex2f(cx + rad*sin(angle), cy + rad*cos(angle));
    }
    glEnd();
}

void drawPauseScreen() {
    // --- Gradient background ---
    glBegin(GL_QUADS);
    // top color
    glColor3f(0.1f, 0.1f, 0.3f);  // dark blue
    glVertex2f(-1, 1);
    glVertex2f(1, 1);
    // bottom color
    glColor3f(0.0f, 0.0f, 0.0f);  // black
    glVertex2f(1, -1);
    glVertex2f(-1, -1);
    glEnd();

    // --- Buttons ---
    for (auto &b : pauseButtons) {
        // Button background
        glColor3f(0.2f, 0.2f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(b.x - b.w/2, b.y - b.h/2);
        glVertex2f(b.x + b.w/2, b.y - b.h/2);
        glVertex2f(b.x + b.w/2, b.y + b.h/2);
        glVertex2f(b.x - b.w/2, b.y + b.h/2);
        glEnd();

        // Button text
        glColor3f(1,1,1);
        glRasterPos2f(b.x - (b.label.size() * 0.03f), b.y - 0.02f);
        for (char c : b.label) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }

    // --- Title ---
    glColor3f(1, 0.9f, 0.3f);
    glRasterPos2f(-0.15f, 0.7f);
    string title = "PAUSED";
    for (char c : title) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}



// ---- Rendering ----



void display() {
    glClear(GL_COLOR_BUFFER_BIT); 
    // animated background
    animTime += 0.016f;
    drawAnimatedBackground(animTime);

    // name entry
    if(enteringName) {
        // clean centered UI
        glColor3f(1,1,0);
        drawText(-0.35f, 0.12f, "CONGRATULATIONS - TOP 5!");
        drawText(-0.35f, 0.02f, "Enter your name , Enter to confirm:");
        drawText(-0.25f, -0.08f, nameBuffer.empty() ? "_" : nameBuffer);
        glutSwapBuffers();
        return;
    }

    
    // --- Menu / Help / High Scores ---
    if(!playing) {
    if(!bgmPlaying) playBgmLoop("bgm.wav");
    drawAnimatedBackground(animTime); // neon gradient in background

   if(showMenu) {
    drawAnimatedBackground(animTime);
    drawGradientText(-0.25f, 0.55f, "DX-BALL", 0.0012f);

    std::vector<std::vector<float>> btnColors = {
        {0.6f, 0.8f, 1.0f}, 
        {0.4f, 1.0f, 0.4f}, 
        {1.0f, 0.8f, 0.4f}, 
        {1.0f, 0.5f, 0.7f}
    };

    for(int i=0;i<menuButtons.size();i++){
        auto &b = menuButtons[i];

        // Draw button background
        glColor3f(btnColors[i][0], btnColors[i][1], btnColors[i][2]);
        glBegin(GL_QUADS);
            glVertex2f(b.x - b.w/2, b.y - b.h/2);
            glVertex2f(b.x + b.w/2, b.y - b.h/2);
            glVertex2f(b.x + b.w/2, b.y + b.h/2);
            glVertex2f(b.x - b.w/2, b.y + b.h/2);
        glEnd();

        // Draw button label
        std::string label;
        if(b.id==1) label="START GAME";
        else if(b.id==2) label="HIGH SCORES";
        else if(b.id==3) label="HELP";
        else label="EXIT";

        glColor3f(1,1,1);
        drawText(b.x - 0.08f, b.y - 0.02f, label); // adjust -0.08f to center text
    }

    glutSwapBuffers();
    return;
}



   else if(showStartSubmenu) {
        drawGradientText(-0.25f, 0.5f, "SELECT DIFFICULTY", 0.0007f);

        for (auto &d : diffButtons) {
            if(d.diff==3) glColor3f(0.8f,0.3f,0.3f); // BACK
            else glColor3f(0.3f,0.6f,1.0f); // EASY/MEDIUM/HARD

            glBegin(GL_QUADS);
            glVertex2f(d.x - d.w/2, d.y - d.h/2);
            glVertex2f(d.x + d.w/2, d.y - d.h/2);
            glVertex2f(d.x + d.w/2, d.y + d.h/2);
            glVertex2f(d.x - d.w/2, d.y + d.h/2);
            glEnd();

            std::string label;
            if(d.diff==0) label="EASY";
            else if(d.diff==1) label="MEDIUM";
            else if(d.diff==2) label="HARD";
            else label="BACK";
            drawGradientText(d.x - 0.05f, d.y - 0.02f, label, 0.0005f);
        }

        drawText(-0.4f, -0.85f, "Press ESC to return to Main Menu", GLUT_BITMAP_HELVETICA_12);
        glutSwapBuffers();
        return;
    
   }
    else if(showHighScoresPage) {
    drawAnimatedBackground(animTime);

    drawGradientText(-0.25f, 0.5f, "HIGH SCORES", 0.0009f);

    for(int i=0;i<5;i++){
        char buf[64]; sprintf(buf, "%d. %s %d", i+1, highscores[i].name.c_str(), highscores[i].score);
        drawText(-0.25f, 0.3f - i*0.1f, buf);
    }
    drawText(-0.4f, -0.8f, "Press ESC to return to Menu");
    glutSwapBuffers();
    return;
}

    else if(showHelpPage) {
    drawAnimatedBackground(animTime);

    drawGradientText(-0.25f, 0.5f, "HELP", 0.0012f);

    drawText(-0.8f, 0.35f, "Mouse controls paddle left/right");
    drawText(-0.8f, 0.25f, "Click P to pause/resume");
    drawText(-0.8f, 0.15f, "Start -> Choose difficulty to begin game");
    drawText(-0.8f, 0.05f, "Rewards: life, wider paddle, faster ball, fireball");
    drawText(-0.4f, -0.8f, "Press ESC to return to Menu");
    glutSwapBuffers();
    return;
}




    glutSwapBuffers();
    return;
}


    // draw paddle
    glColor3f(0.12f, 0.68f, 0.92f);
    glBegin(GL_QUADS);
    glVertex2f(paddleX, -0.95f); glVertex2f(paddleX + paddleW, -0.95f);
    glVertex2f(paddleX + paddleW, -0.95f + paddleH); glVertex2f(paddleX, -0.95f + paddleH);
    glEnd();

    // draw ball
    if(fireballActive) glColor3f(1.0f, 0.6f, 0.1f); else glColor3f(1.0f, 0.33f, 0.33f);
    drawFilledCircle(ballX, ballY, ballR);

    
    // draw bricks
for(auto &b : bricks){
    if(!b.alive) continue;
    if(b.texture){
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, b.texture);
    } else glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex2f(b.x, b.y);
        glTexCoord2f(1,1); glVertex2f(b.x + b.w, b.y);
        glTexCoord2f(1,0); glVertex2f(b.x + b.w, b.y - b.h);
        glTexCoord2f(0,0); glVertex2f(b.x, b.y - b.h);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    float cx = b.x + b.w/2;
    float cy = b.y - b.h/2;
    float r = b.w/2;
    if(b.shapeType==1) drawCircle(cx, cy, r);
    else if(b.shapeType==2) drawMoonShape(cx, cy, r);
    else if(b.shapeType==3) drawStarShape(cx, cy, r);

    // draw border
    glColor3f(1,1,1);
    glBegin(GL_LINE_LOOP);
        glVertex2f(b.x + 0.002f, b.y - 0.002f);
        glVertex2f(b.x + b.w - 0.002f, b.y - 0.002f);
        glVertex2f(b.x + b.w - 0.002f, b.y - b.h + 0.002f);
        glVertex2f(b.x + 0.002f, b.y - b.h + 0.002f);
    glEnd();
}



    // reward falling
    if(reward.active) {
        if(reward.type==0) glColor3f(0.0f,0.8f,0.2f);
        else if(reward.type==1) glColor3f(1.0f,0.85f,0.2f);
        else if(reward.type==2) glColor3f(0.6f,0.2f,0.7f);
        else glColor3f(1.0f,0.3f,0.0f);
        glBegin(GL_QUADS);
        glVertex2f(reward.x - 0.03f, reward.y);
        glVertex2f(reward.x + 0.03f, reward.y);
        glVertex2f(reward.x + 0.03f, reward.y - 0.05f);
        glVertex2f(reward.x - 0.03f, reward.y - 0.05f);
        glEnd();
    }

    // missed reward indicator
    if(missed.active) {
        if(missed.type==0) glColor3f(0.0f,0.7f,0.2f);
        else if(missed.type==1) glColor3f(1.0f,0.85f,0.2f);
        else if(missed.type==2) glColor3f(0.6f,0.2f,0.7f);
        else glColor3f(1.0f,0.3f,0.0f);
        glBegin(GL_QUADS);
        glVertex2f(missed.x - 0.04f, missed.y);
        glVertex2f(missed.x + 0.04f, missed.y);
        glVertex2f(missed.x + 0.04f, missed.y - 0.045f);
        glVertex2f(missed.x - 0.04f, missed.y - 0.045f);
        glEnd();
        glColor3f(1,0.6f,0.6f); drawText(missed.x - 0.06f, missed.y + 0.05f, "MISSED");
    }

    drawParticles();
    drawUI();

    if(levelCleared) { glColor3f(0.2f,1.0f,0.2f); drawText(-0.12f, 0.0f, "LEVEL CLEARED!"); }
    // --- Pause Handling ---
if (paused) {
    // Ensure pause menu always shows when paused
    showPauseMenu = true;

    // Draw pause screen instead of normal game
    drawPauseScreen();
    glutSwapBuffers();
    return;  // Stop all other game rendering
}


    glutSwapBuffers();
}

// ---- Input ----
void motionMouse(int x, int y) {
    if(!playing || enteringName) return;
    float nx = (float)x / (float)windowWidth;
    float worldX = nx * 2.0f - 1.0f;
    paddleX = worldX - paddleW/2.0f;
    if(paddleX < -1.0f) paddleX = -1.0f;
    if(paddleX + paddleW > 1.0f) paddleX = 1.0f - paddleW;
}
void mouseClick(int button, int state, int mx, int my) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    float nx = (float)mx / (float)windowWidth;
    float ny = 1.0f - (float)my / (float)windowHeight; // flip Y
    float worldX = nx * 2.0f - 1.0f;
    float worldY = ny * 2.0f - 1.0f;

    // --- MAIN MENU ---
    if (showMenu) {
        for (auto &b : menuButtons) {
            if (worldX >= b.x - b.w / 2 && worldX <= b.x + b.w / 2 &&
                worldY >= b.y - b.h / 2 && worldY <= b.y + b.h / 2) {
                switch (b.id) {
                    case 1: // Start
                        showMenu = false;
                        showStartSubmenu = true;
                        break;
                    case 2: // High Scores
                        showMenu = false;
                        showHighScoresPage = true;
                        break;
                    case 3: // Help
                        showMenu = false;
                        showHelpPage = true;
                        break;
                    case 4: // Exit
                        exit(0);
                        break;
                }
            }
        }
        return;
    }

    // --- DIFFICULTY MENU ---
    if (showStartSubmenu) {
        for (auto &d : diffButtons) {
            if (worldX >= d.x - d.w / 2 && worldX <= d.x + d.w / 2 &&
                worldY >= d.y - d.h / 2 && worldY <= d.y + d.h / 2) {
                if (d.diff == 3) { // BACK button
                    showStartSubmenu = false;
                    showMenu = true;
                } else {
                    startGame((Difficulty)d.diff);
                    showStartSubmenu = false;
                    showMenu = false;

                    stopBgm(); 
                }
                
                glutPostRedisplay(); // 🔥 force immediate update
                return;
            }
        }
        return;
    }

    // --- HIGHSCORE PAGE BACK DETECTION (optional) ---
    if (showHighScoresPage) {
        
    }

    // --- HELP PAGE BACK DETECTION (optional) ---
    if (showHelpPage) {
        
    }

    // --- PAUSE MENU ---
if (showPauseMenu) {
    for (auto &b : pauseButtons) {
        if (worldX >= b.x - b.w/2 && worldX <= b.x + b.w/2 &&
            worldY >= b.y - b.h/2 && worldY <= b.y + b.h/2) {
            switch (b.id) {
                case 1: // Resume
                    paused = false;
                    showPauseMenu = false;
                    break;
                case 2: // Main Menu
                    paused = false;
                    playing = false;
                    showPauseMenu = false;
                    showMenu = true;
                    break;
                case 3: // Exit
                    exit(0);
                    break;
            }
        }
    }
    return;
}
      if (playing && !enteringName) {
        paused = !paused;
        showPauseMenu = paused; // ✅ showPauseMenu sync
    }
}



void keyboard(unsigned char key, int, int) {
    if (enteringName) {
        if (key == 13) { // Enter
            if (nameBuffer.empty()) nameBuffer = "ANON";
            pushHighScore(nameBuffer, pendingScoreForName);
            enteringName = false;
            playing = false;
            showMenu = true;
            stopBgm();
        } else if (key == 8 || key == 127) {
            if (!nameBuffer.empty()) nameBuffer.pop_back();
        } else if (isalnum((unsigned char)key) && nameBuffer.size() < 12) {
            nameBuffer.push_back(key);
        }
        return; // 🔹 ensure we exit here
    }

    // ESC handling
    if (key == 27) {
        if (showStartSubmenu || showHighScoresPage || showHelpPage) {
            showMenu = true;
            showStartSubmenu = false;
            showHighScoresPage = false;
            showHelpPage = false;
        } else if (playing) {
            playing = false;
            paused = false;
        }
        stopBgm();
        return;
    }

    // Pause
    if ((key == 'p' || key == 'P') && playing) paused = !paused;
}



// ---- Menu ----
void startLevel(int v) {
    if(v==1) startGame(EASY);
    else if(v==2) startGame(MEDIUM);
    else if(v==3) startGame(HARD);
}
void mainMenu(int v) {
    if(v==1) { /* Start game handled by submenu */ }
    else if(v==2) { // High Scores
        showMenu = false;
        showHighScoresPage = true;
        showHelp = false;
        playing = false;
        if(!bgmPlaying) playBgmLoop("bgm.wav");
    }
    else if(v==3) { // Help
        showMenu = false;
        showHelp = true;
        showHighScoresPage = false;
        playing = false;
        if(!bgmPlaying) playBgmLoop("bgm.wav");
    }
    else if(v==4) { // Exit
        saveHighScores(); exit(0);
    }
}


// ---- Timer / Update ----
void timerFunc(int) {
    // level cleared progression
    if(levelCleared && --levelClearTicks <= 0) {
        levelCleared = false;
        if(difficulty == EASY) startGame(MEDIUM);
        else if(difficulty == MEDIUM) startGame(HARD);
        else {
            stopBgm();
            if(isTopFive(score)) {
                enteringName = true; nameBuffer.clear(); pendingScoreForName = score;
            } else playing = false;
        }
    }

    if(!paused) checkCollisionsAndPhysics();
    if(missed.active && --missed.timer <= 0) missed.active = false;

    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

// ---- Reshape & Init ----
void reshape(int w, int h) {
    windowWidth = w; windowHeight = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float aspect = (float)w / (float)h;
    if(aspect >= 1.0f) gluOrtho2D(-aspect, aspect, -1.2, 1.2);
    else gluOrtho2D(-1.2, 1.2, -1.2/aspect, 1.2/aspect);
    glMatrixMode(GL_MODELVIEW);
}
void initGL() { glClearColor(0,0,0,1); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }

// ---- Main ----
int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    loadHighScores();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("DX-Ball");

    initGL();
    setupMenuButtons(); // <-- 🔥 Make sure this is here
    setupDiffButtons();
    setupPauseButtons();

    PlaySound(TEXT("bgm.wav"), NULL, SND_LOOP | SND_ASYNC);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutPassiveMotionFunc(motionMouse);
    glutMotionFunc(motionMouse);
    glutMouseFunc(mouseClick);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timerFunc, 0);

    playing = false; paused = false; showHelp = false;
    glutMainLoop();
    PlaySound(NULL, NULL, 0);
    return 0;
}
