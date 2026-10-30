#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void resetSimulation();

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Plane mechanics
glm::vec3 planePos = glm::vec3(0.0f, 25.0f, 0.0f); // Starts safely in the air
glm::vec3 planeFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 planeVelocity = glm::vec3(0.0f);
glm::vec3 planeAcceleration = glm::vec3(0.0f);

float planeSpeed = 25.0f;
const float MIN_AIR_SPEED = 10.0f;
const float MAX_AIR_SPEED = 60.0f;

float yaw = -90.0f; // Default facing negative Z
float pitch = 0.0f;
float roll = 0.0f;

// Collision & status state
bool isCrashed = false;
bool isOnGround = false;

// Camera Control Variables - adjusted higher and slightly further back
float camYaw = -90.0f;
float camPitch = 18.0f; // Default elevation pitch angle
bool rightMousePressed = false;
float lastMouseX = 512.0f, lastMouseY = 384.0f;

// Sun light direction vector (normalized)
const glm::vec3 SUN_DIR = glm::normalize(glm::vec3(0.45f, 0.70f, 0.35f));

// Terrain height function - identical on CPU & GPU
inline float getTerrainHeight(float x, float z) {
    float h = 0.0f;
    // Large gentle rolling hills
    h += 14.0f * std::sin(x * 0.012f) * std::cos(z * 0.012f);
    // Medium hills
    h += 8.0f * std::sin(x * 0.025f + 1.3f) * std::sin(z * 0.022f + 0.6f);
    // Small smooth swells
    h += 3.5f * std::cos(x * 0.050f) * std::sin(z * 0.045f + 1.1f);
    return h + 5.0f;
}

void resetSimulation() {
    float currentH = getTerrainHeight(planePos.x, planePos.z);
    planePos.y = currentH + 25.0f;
    pitch = 0.0f;
    roll = 0.0f;
    planeSpeed = 25.0f;
    planeVelocity = planeFront * planeSpeed;
    isCrashed = false;
    isOnGround = false;
    std::cout << "[INFO] Simulation restarted! Plane flying at altitude " << planePos.y << std::endl;
}

const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "layout (location = 2) in vec3 aNormal;\n"
    "\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "uniform bool isTerrain;\n"
    "uniform bool isSky;\n"
    "uniform bool isUI;\n"
    "uniform vec2 terrainOffset;\n"
    "\n"
    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "out vec3 VertexColor;\n"
    "out float WorldY;\n"
    "\n"
    "float getTerrainH(float x, float z) {\n"
    "    float h = 0.0;\n"
    "    h += 14.0 * sin(x * 0.012) * cos(z * 0.012);\n"
    "    h += 8.0 * sin(x * 0.025 + 1.3) * sin(z * 0.022 + 0.6);\n"
    "    h += 3.5 * cos(x * 0.050) * sin(z * 0.045 + 1.1);\n"
    "    return h + 5.0;\n"
    "}\n"
    "\n"
    "vec3 calcTerrainNormal(float x, float z) {\n"
    "    float eps = 0.4;\n"
    "    float hL = getTerrainH(x - eps, z);\n"
    "    float hR = getTerrainH(x + eps, z);\n"
    "    float hD = getTerrainH(x, z - eps);\n"
    "    float hU = getTerrainH(x, z + eps);\n"
    "    return normalize(vec3(hL - hR, 2.0 * eps, hD - hU));\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    if (isUI) {\n"
    "        gl_Position = projection * model * vec4(aPos.xy, 0.0, 1.0);\n"
    "        VertexColor = aColor;\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    vec3 pos = aPos;\n"
    "    vec3 norm = aNormal;\n"
    "    vec3 vColor = aColor;\n"
    "\n"
    "    if (isTerrain) {\n"
    "        float wx = aPos.x + terrainOffset.x;\n"
    "        float wz = aPos.z + terrainOffset.y;\n"
    "        float wy = getTerrainH(wx, wz);\n"
    "        pos = vec3(wx, wy, wz);\n"
    "        norm = calcTerrainNormal(wx, wz);\n"
    "        WorldY = wy;\n"
    "    } else {\n"
    "        vec4 worldP = model * vec4(pos, 1.0);\n"
    "        pos = worldP.xyz;\n"
    "        norm = mat3(transpose(inverse(model))) * aNormal;\n"
    "        WorldY = pos.y;\n"
    "    }\n"
    "\n"
    "    FragPos = pos;\n"
    "    Normal = normalize(norm);\n"
    "    VertexColor = aColor;\n"
    "    gl_Position = projection * view * vec4(pos, 1.0);\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "in vec3 VertexColor;\n"
    "in float WorldY;\n"
    "\n"
    "uniform vec4 objectColor;\n"
    "uniform bool useUniformColor;\n"
    "uniform bool isTerrain;\n"
    "uniform bool isSky;\n"
    "uniform bool isUI;\n"
    "uniform vec3 cameraPos;\n"
    "uniform float appTime;\n"
    "\n"
    "uniform vec3 planePos;\n"
    "uniform mat3 planeRotInv;\n"
    "\n"
    "float hash(vec2 p) {\n"
    "    p = fract(p * vec2(123.34, 456.21));\n"
    "    p += dot(p, p + 45.32);\n"
    "    return fract(p.x * p.y);\n"
    "}\n"
    "\n"
    "float smoothNoise(vec2 p) {\n"
    "    vec2 i = floor(p);\n"
    "    vec2 f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    float a = hash(i);\n"
    "    float b = hash(i + vec2(1.0, 0.0));\n"
    "    float c = hash(i + vec2(0.0, 1.0));\n"
    "    float d = hash(i + vec2(1.0, 1.0));\n"
    "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
    "}\n"
    "\n"
    "float cloudFBM(vec2 p) {\n"
    "    float f = 0.0;\n"
    "    f += 0.5000 * smoothNoise(p); p *= 2.02;\n"
    "    f += 0.2500 * smoothNoise(p); p *= 2.03;\n"
    "    f += 0.1250 * smoothNoise(p); p *= 2.01;\n"
    "    f += 0.0625 * smoothNoise(p);\n"
    "    return f;\n"
    "}\n"
    "\n"
    "vec3 getSkyColor(vec3 rayDir, float time) {\n"
    "    float elevation = clamp(rayDir.y, 0.0, 1.0);\n"
    "    vec3 zenithColor = vec3(0.20, 0.48, 0.86);\n"
    "    vec3 horizonColor = vec3(0.55, 0.78, 0.92);\n"
    "    vec3 sky = mix(horizonColor, zenithColor, pow(elevation, 0.65));\n"
    "\n"
    "    vec3 sunDir = normalize(vec3(0.45, 0.70, 0.35));\n"
    "    float sunDot = dot(rayDir, sunDir);\n"
    "    if (sunDot > 0.0) {\n"
    "        float sunDisc = smoothstep(0.9975, 0.9985, sunDot);\n"
    "        vec3 sunColor = vec3(1.0, 0.98, 0.90);\n"
    "        sky = mix(sky, sunColor, sunDisc);\n"
    "        float corona = smoothstep(0.96, 0.998, sunDot) * 0.55;\n"
    "        vec3 coronaColor = vec3(1.0, 0.88, 0.65);\n"
    "        sky += coronaColor * corona;\n"
    "    }\n"
    "\n"
    "    if (rayDir.y > 0.02) {\n"
    "        vec2 cloudUV = (rayDir.xz / rayDir.y) * 0.35 + vec2(time * 0.005, time * 0.003);\n"
    "        float n = cloudFBM(cloudUV);\n"
    "        float cloudDensity = smoothstep(0.50, 0.76, n);\n"
    "        cloudDensity *= 0.38;\n"
    "        cloudDensity *= smoothstep(0.02, 0.20, rayDir.y);\n"
    "\n"
    "        vec3 cloudBase = vec3(0.98, 0.98, 1.0);\n"
    "        vec3 cloudSunlit = cloudBase + vec3(0.12, 0.08, 0.02) * max(sunDot, 0.0);\n"
    "        sky = mix(sky, cloudSunlit, cloudDensity);\n"
    "    }\n"
    "\n"
    "    return sky;\n"
    "}\n"
    "\n"
    "vec3 getTerrainColor(float y, vec3 normal) {\n"
    "    float t = clamp((y + 15.0) / 45.0, 0.0, 1.0);\n"
    "    vec3 valleyGreen = vec3(0.18, 0.52, 0.20);\n"
    "    vec3 meadowGreen = vec3(0.28, 0.68, 0.25);\n"
    "    vec3 ridgeGreen  = vec3(0.42, 0.74, 0.32);\n"
    "    vec3 baseColor;\n"
    "    if (t < 0.5) {\n"
    "        baseColor = mix(valleyGreen, meadowGreen, t * 2.0);\n"
    "    } else {\n"
    "        baseColor = mix(meadowGreen, ridgeGreen, (t - 0.5) * 2.0);\n"
    "    }\n"
    "    float slope = 1.0 - normal.y;\n"
    "    vec3 rockColor = vec3(0.42, 0.38, 0.30);\n"
    "    baseColor = mix(baseColor, rockColor, smoothstep(0.3, 0.7, slope));\n"
    "    return baseColor;\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    if (isUI) {\n"
    "        if (useUniformColor) {\n"
    "            FragColor = objectColor;\n"
    "        } else {\n"
    "            FragColor = vec4(VertexColor, objectColor.a);\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    if (isSky) {\n"
    "        vec3 rayDir = normalize(FragPos - cameraPos);\n"
    "        FragColor = vec4(getSkyColor(rayDir, appTime), 1.0);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    if (useUniformColor) {\n"
    "        FragColor = objectColor;\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    vec3 base;\n"
    "    if (isTerrain) {\n"
    "        base = getTerrainColor(WorldY, Normal);\n"
    "    } else {\n"
    "        base = VertexColor;\n"
    "    }\n"
    "\n"
    "    // Sunlight lighting\n"
    "    vec3 sunDir = normalize(vec3(0.45, 0.70, 0.35));\n"
    "    float diff = max(dot(Normal, sunDir), 0.0);\n"
    "    vec3 ambient = vec3(0.42, 0.48, 0.58) * 0.75;\n"
    "    vec3 diffuse = vec3(1.0, 0.98, 0.90) * diff * 0.65;\n"
    "    vec3 litColor = (ambient + diffuse) * base;\n"
    "\n"
    "    // Dynamic Plane Shadow on Terrain\n"
    "    if (isTerrain) {\n"
    "        vec3 toPlane = planePos - FragPos;\n"
    "        float t = dot(toPlane, sunDir);\n"
    "        if (t > 0.0) {\n"
    "            vec3 rayPoint = FragPos + t * sunDir;\n"
    "            vec3 localOffset = planeRotInv * (rayPoint - planePos);\n"
    "            vec3 sLocal = planeRotInv * sunDir;\n"
    "            float sY = max(sLocal.y, 0.08);\n"
    "            float projX = localOffset.x - localOffset.y * (sLocal.x / sY);\n"
    "            float projZ = localOffset.z - localOffset.y * (sLocal.z / sY);\n"
    "\n"
    "            float zRel = projZ + 2.2;\n"
    "            float wingW = (zRel >= 0.0 && zRel <= 3.25) ? (2.1 * zRel / 3.2) : 0.0;\n"
    "            float bodyW = (zRel >= 0.0 && zRel <= 3.25) ? 0.40 : 0.0;\n"
    "            float halfSpan = max(wingW, bodyW);\n"
    "\n"
    "            float distWing = abs(projX) - halfSpan;\n"
    "            float distZ = max(-projZ - 2.2, projZ - 1.05);\n"
    "            float dWing = max(distWing, distZ);\n"
    "\n"
    "            float distRudderX = abs(projX) - 0.15;\n"
    "            float distRudderZ = max(0.2 - projZ, projZ - 1.25);\n"
    "            float dRudder = max(distRudderX, distRudderZ);\n"
    "\n"
    "            float d = min(dWing, dRudder);\n"
    "            float blur = 0.12 + t * 0.008;\n"
    "            float shadowFactor = 1.0 - smoothstep(-blur, blur, d);\n"
    "            shadowFactor *= clamp(1.0 - t / 350.0, 0.0, 1.0);\n"
    "\n"
    "            litColor *= (1.0 - shadowFactor * 0.65);\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Atmospheric horizon fog\n"
    "    float dist = length(cameraPos - FragPos);\n"
    "    float fogFactor = clamp((dist - 350.0) / 750.0, 0.0, 0.88);\n"
    "    vec3 skyColor = vec3(0.55, 0.78, 0.92);\n"
    "    vec3 finalColor = mix(litColor, skyColor, fogFactor);\n"
    "\n"
    "    FragColor = vec4(finalColor, 1.0);\n"
    "}\n\0";

void addTriangle(std::vector<float>& buf, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 col) {
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    glm::vec3 verts[3] = {v0, v1, v2};
    for (int i = 0; i < 3; ++i) {
        buf.push_back(verts[i].x);
        buf.push_back(verts[i].y);
        buf.push_back(verts[i].z);
        buf.push_back(col.x);
        buf.push_back(col.y);
        buf.push_back(col.z);
        buf.push_back(normal.x);
        buf.push_back(normal.y);
        buf.push_back(normal.z);
    }
}

void addLineSegment(std::vector<float>& buf, glm::vec3 v0, glm::vec3 v1) {
    glm::vec3 dummyNormal(0.0f, 1.0f, 0.0f);
    glm::vec3 dummyColor(0.0f, 0.0f, 0.0f);
    glm::vec3 pts[2] = {v0, v1};
    for(int i = 0; i < 2; ++i) {
        buf.push_back(pts[i].x);
        buf.push_back(pts[i].y);
        buf.push_back(pts[i].z);
        buf.push_back(dummyColor.x);
        buf.push_back(dummyColor.y);
        buf.push_back(dummyColor.z);
        buf.push_back(dummyNormal.x);
        buf.push_back(dummyNormal.y);
        buf.push_back(dummyNormal.z);
    }
}

// ==========================================
// Vector Font & 2D UI Drawing Helpers
// ==========================================
void addUIQuad(std::vector<float>& buf, float x, float y, float w, float h, glm::vec3 col) {
    glm::vec3 norm(0.0f, 0.0f, 1.0f);
    float x2 = x + w;
    float y2 = y + h;
    glm::vec3 p0(x,  y,  0.0f);
    glm::vec3 p1(x2, y,  0.0f);
    glm::vec3 p2(x2, y2, 0.0f);
    glm::vec3 p3(x,  y2, 0.0f);

    glm::vec3 quad[6] = { p0, p1, p2,  p0, p2, p3 };
    for (int i = 0; i < 6; ++i) {
        buf.push_back(quad[i].x);
        buf.push_back(quad[i].y);
        buf.push_back(quad[i].z);
        buf.push_back(col.r);
        buf.push_back(col.g);
        buf.push_back(col.b);
        buf.push_back(norm.x);
        buf.push_back(norm.y);
        buf.push_back(norm.z);
    }
}

void addUILine(std::vector<float>& buf, float x1, float y1, float x2, float y2, glm::vec3 col) {
    glm::vec3 norm(0.0f, 0.0f, 1.0f);
    buf.push_back(x1); buf.push_back(y1); buf.push_back(0.0f);
    buf.push_back(col.r); buf.push_back(col.g); buf.push_back(col.b);
    buf.push_back(norm.x); buf.push_back(norm.y); buf.push_back(norm.z);

    buf.push_back(x2); buf.push_back(y2); buf.push_back(0.0f);
    buf.push_back(col.r); buf.push_back(col.g); buf.push_back(col.b);
    buf.push_back(norm.x); buf.push_back(norm.y); buf.push_back(norm.z);
}

void addUICharStrokes(std::vector<float>& buf, char c, float x, float y, float w, float h, glm::vec3 col) {
    auto seg = [&](float ax, float ay, float bx, float by) {
        addUILine(buf, x + ax * w, y + (1.0f - ay) * h, x + bx * w, y + (1.0f - by) * h, col);
    };

    switch (c) {
        case 'A':
            seg(0.0f, 0.0f, 0.5f, 1.0f); seg(0.5f, 1.0f, 1.0f, 0.0f); seg(0.2f, 0.45f, 0.8f, 0.45f);
            break;
        case 'B':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.8f, 1.0f); seg(0.8f, 1.0f, 0.8f, 0.5f);
            seg(0.8f, 0.5f, 0.0f, 0.5f); seg(0.0f, 0.5f, 0.8f, 0.5f); seg(0.8f, 0.5f, 0.8f, 0.0f);
            seg(0.8f, 0.0f, 0.0f, 0.0f);
            break;
        case 'C':
            seg(1.0f, 1.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.0f, 0.0f); seg(0.0f, 0.0f, 1.0f, 0.0f);
            break;
        case 'D':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.65f, 1.0f); seg(0.65f, 1.0f, 1.0f, 0.65f);
            seg(1.0f, 0.65f, 1.0f, 0.35f); seg(1.0f, 0.35f, 0.65f, 0.0f); seg(0.65f, 0.0f, 0.0f, 0.0f);
            break;
        case 'E':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.95f, 1.0f);
            seg(0.0f, 0.5f, 0.70f, 0.5f); seg(0.0f, 0.0f, 0.95f, 0.0f);
            break;
        case 'H':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(1.0f, 0.0f, 1.0f, 1.0f); seg(0.0f, 0.5f, 1.0f, 0.5f);
            break;
        case 'I':
            seg(0.5f, 0.0f, 0.5f, 1.0f); seg(0.15f, 1.0f, 0.85f, 1.0f); seg(0.15f, 0.0f, 0.85f, 0.0f);
            break;
        case 'K':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(1.0f, 1.0f, 0.0f, 0.5f); seg(0.0f, 0.5f, 1.0f, 0.0f);
            break;
        case 'L':
            seg(0.0f, 1.0f, 0.0f, 0.0f); seg(0.0f, 0.0f, 0.9f, 0.0f);
            break;
        case 'N':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 1.0f, 0.0f); seg(1.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 'O':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 1.0f, 1.0f);
            seg(1.0f, 1.0f, 1.0f, 0.0f); seg(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        case 'P':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.9f, 1.0f);
            seg(0.9f, 1.0f, 0.9f, 0.5f); seg(0.9f, 0.5f, 0.0f, 0.5f);
            break;
        case 'R':
            seg(0.0f, 0.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.9f, 1.0f);
            seg(0.9f, 1.0f, 0.9f, 0.55f); seg(0.9f, 0.55f, 0.0f, 0.55f);
            seg(0.35f, 0.55f, 0.95f, 0.0f);
            break;
        case 'S':
            seg(1.0f, 1.0f, 0.0f, 1.0f); seg(0.0f, 1.0f, 0.0f, 0.52f);
            seg(0.0f, 0.52f, 1.0f, 0.52f); seg(1.0f, 0.52f, 1.0f, 0.0f);
            seg(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        case 'T':
            seg(0.0f, 1.0f, 1.0f, 1.0f); seg(0.5f, 1.0f, 0.5f, 0.0f);
            break;
        case 'U':
            seg(0.0f, 1.0f, 0.0f, 0.0f); seg(0.0f, 0.0f, 1.0f, 0.0f); seg(1.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 'Y':
            seg(0.0f, 1.0f, 0.5f, 0.5f); seg(1.0f, 1.0f, 0.5f, 0.5f); seg(0.5f, 0.5f, 0.5f, 0.0f);
            break;
        case '[':
            seg(0.8f, 1.0f, 0.2f, 1.0f); seg(0.2f, 1.0f, 0.2f, 0.0f); seg(0.2f, 0.0f, 0.8f, 0.0f);
            break;
        case ']':
            seg(0.2f, 1.0f, 0.8f, 1.0f); seg(0.8f, 1.0f, 0.8f, 0.0f); seg(0.8f, 0.0f, 0.2f, 0.0f);
            break;
        case '-':
            seg(0.15f, 0.5f, 0.85f, 0.5f);
            break;
        default:
            break;
    }
}

void addUIText(std::vector<float>& buf, const std::string& text, float startX, float startY, float charW, float charH, float spacing, glm::vec3 col) {
    float curX = startX;
    for (char c : text) {
        if (c != ' ') {
            addUICharStrokes(buf, c, curX, startY, charW, charH, col);
        }
        curX += charW + spacing;
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Flight Simulator", NULL, NULL);
    if(window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if(!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Compile Shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex shader compile error:\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment shader compile error:\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader program link error:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ==========================================
    // 1. Build Airplane Model with Grey Body & Yellow Tail
    // ==========================================
    std::vector<float> planeVertices;
    
    // Geometry coordinates
    glm::vec3 nose(0.0f, 0.0f, -2.2f);           // Pointed nozzle / nose tip
    glm::vec3 tailTop(0.0f, 0.95f, 0.95f);       // Upper spine
    glm::vec3 wingLeft(-2.1f, 0.0f, 0.95f);      // Left wingtip
    glm::vec3 wingRight(2.1f, 0.0f, 0.95f);      // Right wingtip
    glm::vec3 tailBase(0.0f, 0.05f, 0.95f);      // Rear base

    // Color Palette: Body Grey + Existing Yellow Tail
    glm::vec3 colorBodyGrey(0.66f, 0.68f, 0.72f);        // Tactical Aircraft Grey Body
    glm::vec3 colorBellyGrey(0.50f, 0.52f, 0.56f);       // Slightly deeper grey underbelly
    glm::vec3 colorCanopyGrey(0.32f, 0.36f, 0.42f);      // Dark tinted canopy glass
    glm::vec3 colorTailYellow(0.96f, 0.76f, 0.10f);      // Existing Yellow Tail Rudder
    glm::vec3 colorExhaustCharcoal(0.24f, 0.24f, 0.26f); // Exhaust Base
    glm::vec3 colorThrusterOrange(1.0f, 0.55f, 0.12f);   // Glowing Thruster Core

    // Facet 1: Left Upper Wing & Fuselage (Grey Body)
    addTriangle(planeVertices, nose, tailTop, wingLeft, colorBodyGrey);

    // Facet 2: Right Upper Wing & Fuselage (Grey Body)
    addTriangle(planeVertices, nose, wingRight, tailTop, colorBodyGrey);

    // Facet 3: Ventral Underbelly (Deeper Grey)
    addTriangle(planeVertices, nose, wingLeft, wingRight, colorBellyGrey);

    // Facet 4: Aft Exhaust Base (Charcoal Grey)
    addTriangle(planeVertices, wingLeft, tailTop, wingRight, colorExhaustCharcoal);

    // Facet 5 & 6: Cockpit Canopy (Tinted Dark Glass)
    glm::vec3 canopyFront(0.0f, 0.16f, -1.35f);
    glm::vec3 canopyPeak(0.0f, 0.52f, -0.35f);
    glm::vec3 canopyRear(0.0f, 0.42f, 0.35f);
    glm::vec3 canopyLeft(-0.25f, 0.18f, -0.35f);
    glm::vec3 canopyRight(0.25f, 0.18f, -0.35f);

    addTriangle(planeVertices, canopyFront, canopyPeak, canopyLeft, colorCanopyGrey);
    addTriangle(planeVertices, canopyLeft, canopyPeak, canopyRear, colorCanopyGrey);
    addTriangle(planeVertices, canopyFront, canopyRight, canopyPeak, colorCanopyGrey);
    addTriangle(planeVertices, canopyRight, canopyRear, canopyPeak, colorCanopyGrey);

    // Facet 7 & 8: Vertical Stabilizer / Rudder (Existing Yellow Tail!)
    glm::vec3 rudderBaseFront(0.0f, 0.68f, 0.35f);
    glm::vec3 rudderTip(0.0f, 1.55f, 1.05f);
    glm::vec3 rudderTipRear(0.0f, 1.55f, 1.15f);
    glm::vec3 rudderBaseRear(0.0f, 0.90f, 1.15f);

    addTriangle(planeVertices, rudderBaseFront + glm::vec3(0.02f, 0.0f, 0.0f),
                            rudderTip + glm::vec3(0.02f, 0.0f, 0.0f),
                            rudderBaseRear + glm::vec3(0.02f, 0.0f, 0.0f), colorTailYellow);
    addTriangle(planeVertices, rudderBaseFront - glm::vec3(0.02f, 0.0f, 0.0f),
                            rudderBaseRear - glm::vec3(0.02f, 0.0f, 0.0f),
                            rudderTip - glm::vec3(0.02f, 0.0f, 0.0f), colorTailYellow);

    // Facet 9: Jet Thruster Core (Glowing Orange)
    addTriangle(planeVertices, glm::vec3(-0.35f, 0.25f, 0.96f),
                            glm::vec3(0.35f, 0.25f, 0.96f),
                            glm::vec3(0.0f, 0.65f, 0.96f), colorThrusterOrange);

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeVertices.size() * sizeof(float), planeVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    int planeVertexCount = planeVertices.size() / 9;

    // ==========================================
    // 1b. Build Explicit Black Edges & Corners
    // ==========================================
    std::vector<float> edgeVertices;
    addLineSegment(edgeVertices, nose, wingLeft);
    addLineSegment(edgeVertices, nose, wingRight);
    addLineSegment(edgeVertices, wingLeft, tailBase);
    addLineSegment(edgeVertices, wingRight, tailBase);
    addLineSegment(edgeVertices, nose, canopyFront);
    addLineSegment(edgeVertices, canopyFront, canopyPeak);
    addLineSegment(edgeVertices, canopyPeak, canopyRear);
    addLineSegment(edgeVertices, canopyFront, canopyLeft);
    addLineSegment(edgeVertices, canopyFront, canopyRight);
    addLineSegment(edgeVertices, canopyPeak, canopyLeft);
    addLineSegment(edgeVertices, canopyPeak, canopyRight);
    addLineSegment(edgeVertices, canopyLeft, canopyRear);
    addLineSegment(edgeVertices, canopyRight, canopyRear);
    addLineSegment(edgeVertices, canopyRear, tailTop);
    addLineSegment(edgeVertices, canopyLeft, wingLeft);
    addLineSegment(edgeVertices, canopyRight, wingRight);
    addLineSegment(edgeVertices, rudderBaseFront, rudderTip);
    addLineSegment(edgeVertices, rudderTip, rudderTipRear);
    addLineSegment(edgeVertices, rudderTipRear, rudderBaseRear);
    addLineSegment(edgeVertices, rudderBaseRear, rudderBaseFront);
    addLineSegment(edgeVertices, wingLeft, tailTop);
    addLineSegment(edgeVertices, wingRight, tailTop);
    addLineSegment(edgeVertices, wingLeft, wingRight);

    unsigned int edgeVAO, edgeVBO;
    glGenVertexArrays(1, &edgeVAO);
    glGenBuffers(1, &edgeVBO);
    glBindVertexArray(edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    glBufferData(GL_ARRAY_BUFFER, edgeVertices.size() * sizeof(float), edgeVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    int edgeVertexCount = edgeVertices.size() / 9;

    // Corner Vertices
    std::vector<float> cornerVertices;
    std::vector<glm::vec3> corners = {
        nose, wingLeft, wingRight, tailTop, tailBase,
        canopyFront, canopyPeak, canopyRear, canopyLeft, canopyRight,
        rudderTip, rudderTipRear, rudderBaseFront, rudderBaseRear
    };
    for(const auto& pt : corners) {
        cornerVertices.push_back(pt.x);
        cornerVertices.push_back(pt.y);
        cornerVertices.push_back(pt.z);
        cornerVertices.push_back(0.0f); cornerVertices.push_back(0.0f); cornerVertices.push_back(0.0f);
        cornerVertices.push_back(0.0f); cornerVertices.push_back(1.0f); cornerVertices.push_back(0.0f);
    }
    unsigned int cornerVAO, cornerVBO;
    glGenVertexArrays(1, &cornerVAO);
    glGenBuffers(1, &cornerVBO);
    glBindVertexArray(cornerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glBufferData(GL_ARRAY_BUFFER, cornerVertices.size() * sizeof(float), cornerVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    int cornerVertexCount = cornerVertices.size() / 9;

    // ==========================================
    // 2. Build Smooth Hills Terrain Grid
    // ==========================================
    const int GRID_CELLS = 140;
    const float GRID_SIZE = 2400.0f;
    const float CELL_SIZE = GRID_SIZE / (float)GRID_CELLS; // ~17.14f

    std::vector<float> terrainVertices;
    std::vector<unsigned int> terrainIndices;

    for (int i = 0; i <= GRID_CELLS; ++i) {
        for (int j = 0; j <= GRID_CELLS; ++j) {
            float x = -GRID_SIZE * 0.5f + i * CELL_SIZE;
            float z = -GRID_SIZE * 0.5f + j * CELL_SIZE;

            terrainVertices.push_back(x);
            terrainVertices.push_back(0.0f);
            terrainVertices.push_back(z);
            terrainVertices.push_back(0.3f);
            terrainVertices.push_back(0.7f);
            terrainVertices.push_back(0.3f);
            terrainVertices.push_back(0.0f);
            terrainVertices.push_back(1.0f);
            terrainVertices.push_back(0.0f);
        }
    }

    for (int i = 0; i < GRID_CELLS; ++i) {
        for (int j = 0; j < GRID_CELLS; ++j) {
            unsigned int tl = i * (GRID_CELLS + 1) + j;
            unsigned int tr = tl + 1;
            unsigned int bl = (i + 1) * (GRID_CELLS + 1) + j;
            unsigned int br = bl + 1;

            terrainIndices.push_back(tl);
            terrainIndices.push_back(bl);
            terrainIndices.push_back(tr);

            terrainIndices.push_back(tr);
            terrainIndices.push_back(bl);
            terrainIndices.push_back(br);
        }
    }

    unsigned int terrainVAO, terrainVBO, terrainEBO;
    glGenVertexArrays(1, &terrainVAO);
    glGenBuffers(1, &terrainVBO);
    glGenBuffers(1, &terrainEBO);

    glBindVertexArray(terrainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, terrainVertices.size() * sizeof(float), terrainVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, terrainIndices.size() * sizeof(unsigned int), terrainIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    int terrainIndexCount = terrainIndices.size();

    // ==========================================
    // 3. Build Sky Dome Mesh (Hemisphere)
    // ==========================================
    std::vector<float> skyVertices;
    std::vector<unsigned int> skyIndices;
    const int SKY_RINGS = 20;
    const int SKY_SECTORS = 36;
    const float SKY_RADIUS = 1500.0f;

    for (int i = 0; i <= SKY_RINGS; ++i) {
        float phi = (float)i / (float)SKY_RINGS * (glm::pi<float>() * 0.5f);
        float y = SKY_RADIUS * std::sin(phi);
        float r = SKY_RADIUS * std::cos(phi);

        for (int j = 0; j <= SKY_SECTORS; ++j) {
            float theta = (float)j / (float)SKY_SECTORS * (glm::pi<float>() * 2.0f);
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            skyVertices.push_back(x);
            skyVertices.push_back(y);
            skyVertices.push_back(z);
            skyVertices.push_back(1.0f); skyVertices.push_back(1.0f); skyVertices.push_back(1.0f);
            skyVertices.push_back(0.0f); skyVertices.push_back(1.0f); skyVertices.push_back(0.0f);
        }
    }

    for (int i = 0; i < SKY_RINGS; ++i) {
        for (int j = 0; j < SKY_SECTORS; ++j) {
            unsigned int cur = i * (SKY_SECTORS + 1) + j;
            unsigned int next = cur + SKY_SECTORS + 1;

            skyIndices.push_back(cur);
            skyIndices.push_back(next);
            skyIndices.push_back(cur + 1);

            skyIndices.push_back(cur + 1);
            skyIndices.push_back(next);
            skyIndices.push_back(next + 1);
        }
    }

    unsigned int skyVAO, skyVBO, skyEBO;
    glGenVertexArrays(1, &skyVAO);
    glGenBuffers(1, &skyVBO);
    glGenBuffers(1, &skyEBO);

    glBindVertexArray(skyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, skyVertices.size() * sizeof(float), skyVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, skyIndices.size() * sizeof(unsigned int), skyIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int skyIndexCount = skyIndices.size();

    // ==========================================
    // 4. Dynamic UI VAO/VBO for Retry Button & Overlays
    // ==========================================
    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Uniform locations
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    unsigned int useUniformColorLoc = glGetUniformLocation(shaderProgram, "useUniformColor");
    unsigned int isTerrainLoc = glGetUniformLocation(shaderProgram, "isTerrain");
    unsigned int isSkyLoc = glGetUniformLocation(shaderProgram, "isSky");
    unsigned int isUILoc = glGetUniformLocation(shaderProgram, "isUI");
    unsigned int terrainOffsetLoc = glGetUniformLocation(shaderProgram, "terrainOffset");
    unsigned int cameraPosLoc = glGetUniformLocation(shaderProgram, "cameraPos");
    unsigned int appTimeLoc = glGetUniformLocation(shaderProgram, "appTime");
    unsigned int planePosLoc = glGetUniformLocation(shaderProgram, "planePos");
    unsigned int planeRotInvLoc = glGetUniformLocation(shaderProgram, "planeRotInv");

    float titleUpdateTimer = 0.0f;
    bool lastLeftMouseDown = false;

    while(!glfwWindowShouldClose(window))
    {
        // 1. Time calculations
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        processInput(window);

        // 2. Flight Dynamics & Acceleration Vector
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        planeFront = glm::normalize(front);

        glm::vec3 targetVelocity = planeFront * planeSpeed;
        planeAcceleration = (targetVelocity - planeVelocity) * 10.0f;
        planeVelocity += planeAcceleration * deltaTime;

        if (!isCrashed) {
            planePos += planeVelocity * deltaTime;
        }

        // ==========================================
        // 3. Ground Collision Detection & Response
        // ==========================================
        float groundH = getTerrainHeight(planePos.x, planePos.z);
        float groundClearance = 0.9f;
        float minSafeAltitude = groundH + groundClearance;

        if (planePos.y <= minSafeAltitude) {
            planePos.y = minSafeAltitude;

            // Trigger crash on impact
            if (!isCrashed) {
                isCrashed = true;
                planeSpeed = 0.0f;
                planeVelocity = glm::vec3(0.0f);
                // Reveal mouse cursor so player can click Retry button
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "[COLLISION] Plane hit the ground! Click RETRY button to restart." << std::endl;
            }
            isOnGround = true;
        } else {
            isOnGround = false;
        }

        // Update Window Title with Speed Control Guide
        titleUpdateTimer += deltaTime;
        if (titleUpdateTimer >= 0.1f) {
            titleUpdateTimer = 0.0f;
            char titleBuffer[256];
            if (isCrashed) {
                snprintf(titleBuffer, sizeof(titleBuffer),
                    "Flight Sim | [CRASHED] Plane Hit Ground! Click RETRY Button or Press [R]");
            } else {
                float agl = planePos.y - groundH;
                snprintf(titleBuffer, sizeof(titleBuffer),
                    "Flight Sim | Alt: %.1fm | Spd: %.1f | Pitch: %.0f° | [SPACE: Accel | CTRL: Decel | WASD: Steer]",
                    planePos.y, planeSpeed, pitch);
            }
            glfwSetWindowTitle(window, titleBuffer);
        }

        // 4. Clear Screen
        glClearColor(0.55f, 0.78f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 5. Activate Shader Program
        glUseProgram(shaderProgram);
        glUniform1f(appTimeLoc, currentFrame);
        glUniform1i(isUILoc, 0);

        // ==========================================
        // 6. Camera Setup (Slightly higher & slightly further back)
        // ==========================================
        glm::vec3 camFront;
        camFront.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        camFront.y = sin(glm::radians(camPitch));
        camFront.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        camFront = glm::normalize(camFront);

        // Camera distance: 16.5f (further away) + 3.2f height elevation
        glm::vec3 cameraPos = planePos - (camFront * 16.5f) + glm::vec3(0.0f, 3.2f, 0.0f);
        float camGroundH = getTerrainHeight(cameraPos.x, cameraPos.z);
        if (cameraPos.y < camGroundH + 1.5f) {
            cameraPos.y = camGroundH + 1.5f;
        }

        glm::vec3 lookTarget = planePos + planeFront * 1.5f + glm::vec3(0.0f, 0.6f, 0.0f);
        glm::mat4 view = glm::lookAt(cameraPos, lookTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 3000.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPosLoc, 1, glm::value_ptr(cameraPos));

        // ==========================================
        // 7. Render Sky Dome with Sun and Vague Clouds
        // ==========================================
        glDepthMask(GL_FALSE);
        glUniform1i(isSkyLoc, 1);
        glUniform1i(isTerrainLoc, 0);
        glUniform1i(useUniformColorLoc, 0);

        glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), cameraPos);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(skyModel));

        glBindVertexArray(skyVAO);
        glDrawElements(GL_TRIANGLES, skyIndexCount, GL_UNSIGNED_INT, 0);
        glDepthMask(GL_TRUE);

        // ==========================================
        // 8. Plane Orientation Matrix Setup
        // ==========================================
        glm::vec3 f = planeFront;
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(f, worldUp)) > 0.98f) {
            worldUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        glm::vec3 r = glm::normalize(glm::cross(f, worldUp));
        glm::vec3 u = glm::normalize(glm::cross(r, f));

        float cosR = std::cos(glm::radians(roll));
        float sinR = std::sin(glm::radians(roll));
        glm::vec3 r_rolled = r * cosR + u * sinR;
        glm::vec3 u_rolled = -r * sinR + u * cosR;

        glm::mat4 planeRot(1.0f);
        planeRot[0] = glm::vec4(r_rolled, 0.0f);
        planeRot[1] = glm::vec4(u_rolled, 0.0f);
        planeRot[2] = glm::vec4(-f, 0.0f);
        planeRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        glm::mat3 rot3 = glm::mat3(planeRot);
        glm::mat3 rotInv = glm::transpose(rot3);

        glUniform3fv(planePosLoc, 1, glm::value_ptr(planePos));
        glUniformMatrix3fv(planeRotInvLoc, 1, GL_FALSE, glm::value_ptr(rotInv));

        // ==========================================
        // 9. Render Smooth Hills Terrain (With Plane Shadow)
        // ==========================================
        glUniform1i(isSkyLoc, 0);
        glUniform1i(isTerrainLoc, 1);
        glUniform1i(useUniformColorLoc, 0);

        float offsetX = std::floor(planePos.x / CELL_SIZE) * CELL_SIZE;
        float offsetZ = std::floor(planePos.z / CELL_SIZE) * CELL_SIZE;
        glUniform2f(terrainOffsetLoc, offsetX, offsetZ);

        glm::mat4 terrainModel = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(terrainModel));

        glBindVertexArray(terrainVAO);
        glDrawElements(GL_TRIANGLES, terrainIndexCount, GL_UNSIGNED_INT, 0);

        // ==========================================
        // 10. Render Visual Reference Markers on Hills
        // ==========================================
        glUniform1i(isTerrainLoc, 0);
        glUniform1i(useUniformColorLoc, 1);
        glUniform4f(colorLoc, 0.95f, 0.45f, 0.10f, 1.0f);

        glBindVertexArray(planeVAO);
        for(int x = -5; x <= 5; x++) {
            for(int z = -5; z <= 5; z++) {
                float mx = offsetX + x * 180.0f;
                float mz = offsetZ + z * 180.0f;
                float my = getTerrainHeight(mx, mz);
                glm::mat4 markerModel = glm::mat4(1.0f);
                markerModel = glm::translate(markerModel, glm::vec3(mx, my + 0.8f, mz));
                markerModel = glm::scale(markerModel, glm::vec3(0.6f, 0.6f, 0.6f));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(markerModel));
                glDrawArrays(GL_TRIANGLES, 0, planeVertexCount);
            }
        }

        // ==========================================
        // 11. Render Plane (Grey Body, Yellow Tail, Bold Black Borders & Visible Edges/Corners)
        // ==========================================
        glm::mat4 planeModel = glm::mat4(1.0f);
        planeModel = glm::translate(planeModel, planePos);
        planeModel = planeModel * planeRot;

        // A. Draw Filled Plane with Defining Colors
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 1.5f);

        glUniform1i(isTerrainLoc, 0);
        glUniform1i(useUniformColorLoc, 0);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planeModel));

        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, planeVertexCount);

        glDisable(GL_POLYGON_OFFSET_FILL);

        // B. Draw Bold Black Outer Silhouette (Inverted hull)
        glUniform1i(useUniformColorLoc, 1);
        glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glm::mat4 borderModel = glm::scale(planeModel, glm::vec3(1.08f, 1.08f, 1.04f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(borderModel));
        glDrawArrays(GL_TRIANGLES, 0, planeVertexCount);
        glDisable(GL_CULL_FACE);

        // C. Draw Black Wireframe Creases
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.5f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(planeModel));
        glDrawArrays(GL_TRIANGLES, 0, planeVertexCount);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // D. Draw Explicit Structural Black Edges
        glLineWidth(3.5f);
        glBindVertexArray(edgeVAO);
        glDrawArrays(GL_LINES, 0, edgeVertexCount);

        // E. Draw Highlighted Black Corners / Joints
        glPointSize(7.0f);
        glBindVertexArray(cornerVAO);
        glDrawArrays(GL_POINTS, 0, cornerVertexCount);

        // ==========================================
        // 12. Render Retry Button & Crash Dialog (When Collided)
        // ==========================================
        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // Button dimensions and positions (centered)
        float btnW = 240.0f;
        float btnH = 58.0f;
        float btnX = (winW - btnW) * 0.5f;
        float btnY = (winH * 0.5f) + 15.0f;

        bool isHovered = (mouseX >= btnX && mouseX <= btnX + btnW &&
                          mouseY >= btnY && mouseY <= btnY + btnH);

        bool leftMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if (isCrashed && isHovered && leftMouseDown && !lastLeftMouseDown) {
            // Button clicked: Restart simulation with plane flying!
            resetSimulation();
        }
        lastLeftMouseDown = leftMouseDown;

        if (isCrashed) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUniform1i(isUILoc, 1);
            glUniform1i(isSkyLoc, 0);
            glUniform1i(isTerrainLoc, 0);

            glm::mat4 uiProj = glm::ortho(0.0f, (float)winW, (float)winH, 0.0f, -1.0f, 1.0f);
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(uiProj));
            glm::mat4 uiModel = glm::mat4(1.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(uiModel));

            // Card Dialog Geometry
            float cardW = 440.0f;
            float cardH = 220.0f;
            float cardX = (winW - cardW) * 0.5f;
            float cardY = (winH - cardH) * 0.5f;

            // 1. Draw UI Quads (Card background & button fill)
            std::vector<float> uiQuadVerts;
            // Backdrop dimming
            addUIQuad(uiQuadVerts, 0.0f, 0.0f, (float)winW, (float)winH, glm::vec3(0.02f, 0.03f, 0.05f));
            // Card panel
            addUIQuad(uiQuadVerts, cardX, cardY, cardW, cardH, glm::vec3(0.10f, 0.12f, 0.16f));
            // Card top alert header banner
            addUIQuad(uiQuadVerts, cardX, cardY, cardW, 40.0f, glm::vec3(0.85f, 0.18f, 0.15f));

            // Button background fill (Hovered vs Normal)
            glm::vec3 btnFillColor = isHovered ? glm::vec3(0.18f, 0.72f, 0.35f) : glm::vec3(0.15f, 0.28f, 0.22f);
            addUIQuad(uiQuadVerts, btnX, btnY, btnW, btnH, btnFillColor);

            glBindVertexArray(uiVAO);
            glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, uiQuadVerts.size() * sizeof(float), uiQuadVerts.data());

            glUniform1i(useUniformColorLoc, 0);
            glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 0.85f);
            glDrawArrays(GL_TRIANGLES, 0, uiQuadVerts.size() / 9);

            // 2. Draw UI Lines & Borders
            std::vector<float> uiLineVerts;
            // Card outer borders
            glm::vec3 cardBorderCol(0.95f, 0.35f, 0.25f);
            addUILine(uiLineVerts, cardX, cardY, cardX + cardW, cardY, cardBorderCol);
            addUILine(uiLineVerts, cardX + cardW, cardY, cardX + cardW, cardY + cardH, cardBorderCol);
            addUILine(uiLineVerts, cardX + cardW, cardY + cardH, cardX, cardY + cardH, cardBorderCol);
            addUILine(uiLineVerts, cardX, cardY + cardH, cardX, cardY, cardBorderCol);

            // Button borders (Thick neon border when hovered)
            glm::vec3 btnBorderCol = isHovered ? glm::vec3(0.40f, 0.98f, 0.55f) : glm::vec3(0.30f, 0.75f, 0.45f);
            addUILine(uiLineVerts, btnX, btnY, btnX + btnW, btnY, btnBorderCol);
            addUILine(uiLineVerts, btnX + btnW, btnY, btnX + btnW, btnY + btnH, btnBorderCol);
            addUILine(uiLineVerts, btnX + btnW, btnY + btnH, btnX, btnY + btnH, btnBorderCol);
            addUILine(uiLineVerts, btnX, btnY + btnH, btnX, btnY, btnBorderCol);

            // Inner button accent border
            addUILine(uiLineVerts, btnX + 3.0f, btnY + 3.0f, btnX + btnW - 3.0f, btnY + 3.0f, btnBorderCol);
            addUILine(uiLineVerts, btnX + btnW - 3.0f, btnY + 3.0f, btnX + btnW - 3.0f, btnY + btnH - 3.0f, btnBorderCol);
            addUILine(uiLineVerts, btnX + btnW - 3.0f, btnY + btnH - 3.0f, btnX + 3.0f, btnY + btnH - 3.0f, btnBorderCol);
            addUILine(uiLineVerts, btnX + 3.0f, btnY + btnH - 3.0f, btnX + 3.0f, btnY + 3.0f, btnBorderCol);

            // Text: "PLANE CRASHED" in header banner
            addUIText(uiLineVerts, "PLANE CRASHED", cardX + 115.0f, cardY + 12.0f, 12.0f, 18.0f, 6.0f, glm::vec3(1.0f, 1.0f, 1.0f));

            // Subtitle text: "GROUND COLLISION"
            addUIText(uiLineVerts, "COLLISION", cardX + 106.0f, cardY + 58.0f, 10.0f, 14.0f, 5.0f, glm::vec3(0.9f, 0.7f, 0.7f));

            // Button Text: "RETRY" (Large, centered inside the button)
            glm::vec3 btnTextColor = isHovered ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(0.85f, 1.0f, 0.90f);
            addUIText(uiLineVerts, "RETRY", btnX + 68.0f, btnY + 16.0f, 16.0f, 26.0f, 8.0f, btnTextColor);

            // Hint text below button: "CLICK TO RESTART OR PRESS R"
            addUIText(uiLineVerts, "CLICK OR PRESS [R] TO FLY", cardX + 90.0f, cardY + 188.0f, 8.0f, 12.0f, 4.0f, glm::vec3(0.75f, 0.80f, 0.85f));

            glBufferSubData(GL_ARRAY_BUFFER, 0, uiLineVerts.size() * sizeof(float), uiLineVerts.data());
            glLineWidth(3.0f);
            glDrawArrays(GL_LINES, 0, uiLineVerts.size() / 9);

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        // 13. Swap buffers & poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &edgeVAO);
    glDeleteBuffers(1, &edgeVBO);
    glDeleteVertexArrays(1, &cornerVAO);
    glDeleteBuffers(1, &cornerVBO);
    glDeleteVertexArrays(1, &terrainVAO);
    glDeleteBuffers(1, &terrainVBO);
    glDeleteBuffers(1, &terrainEBO);
    glDeleteVertexArrays(1, &skyVAO);
    glDeleteBuffers(1, &skyVBO);
    glDeleteBuffers(1, &skyEBO);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // -- RETRY / RESTART KEYBINDS (R or ENTER or SPACE when crashed) --
    if (isCrashed) {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            resetSimulation();
        }
        return; // Freeze flight controls while crashed until retry is triggered
    }

    // -- SPEED CONTROLS (Spacebar: Increase velocity | Control: Decrease velocity) --
    // Spacebar - Increase the velocity
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        planeSpeed += 28.0f * deltaTime;
        if (planeSpeed > MAX_AIR_SPEED) planeSpeed = MAX_AIR_SPEED;
    }
    // Control key (Left Ctrl or Right Ctrl) - Decrease the velocity
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        planeSpeed -= 28.0f * deltaTime;
        float minSpeed = isOnGround ? 0.0f : MIN_AIR_SPEED;
        if (planeSpeed < minSpeed) planeSpeed = minSpeed;
    }

    // -- PLANE STEERING CONTROLS (WASD) --
    float turnSpeed = 60.0f * deltaTime;
    float pitchSpeed = 50.0f * deltaTime;
    float targetRoll = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        yaw -= turnSpeed;
        targetRoll = 45.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        yaw += turnSpeed;
        targetRoll = -45.0f;
    }
    roll += (targetRoll - roll) * deltaTime * 5.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) pitch += pitchSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) pitch -= pitchSpeed;
    if (pitch > 85.0f) pitch = 85.0f;
    if (pitch < -85.0f) pitch = -85.0f;

    // -- CAMERA CONTROLS (Right Mouse Drag) --
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!rightMousePressed) {
            lastMouseX = xpos;
            lastMouseY = ypos;
            rightMousePressed = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        float xoffset = xpos - lastMouseX;
        float yoffset = lastMouseY - ypos;
        lastMouseX = xpos;
        lastMouseY = ypos;

        camYaw += xoffset * 0.15f;
        camPitch += yoffset * 0.15f;

        if (camPitch > 85.0f) camPitch = 85.0f;
        if (camPitch < -15.0f) camPitch = -15.0f;
    } else {
        if (rightMousePressed) {
            rightMousePressed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        // Auto-center camera behind plane at slightly elevated third-person height
        camYaw += (yaw - camYaw) * deltaTime * 5.0f;
        camPitch += (pitch + 18.0f - camPitch) * deltaTime * 5.0f;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}