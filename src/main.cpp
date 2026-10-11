// CS 6983 Module 2 Week 3
// Sample OpenGL Program

#include "gl_loader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct App {
    // screen size
    int width = 1920;
    int height = 1080;
    // initial camera position
    float yaw = 0.7f;
    float pitch = 0.4f;
    float distance = 6.0f;
    // dragging state
    bool dragging = false;
    // last mouse position
    double lastX = 0.0;
    double lastY = 0.0;
};

// one object group ("g" line) of the obj file
struct Part {
    std::string name;
    // interleaved vertex data: px py pz nx ny nz per vertex
    std::vector<float> verticesData;
    GLuint verticesCount = 0;
    // filled in after uploading to the GPU
    GLuint VAO = 0;
    glm::vec3 color{1.0f};
};

static App gApp;

// simple shader: used for the grid
static GLuint gProgram = 0;
static GLint gMvpLoc = -1;
static GLint gColorLoc = -1;
static GLuint gGridVao = 0;
static GLuint gGridVertexCount = 0;
static std::vector<Part> gParts;

// Blinn-Phong shader: used for the robot arm
static GLuint gBlinnPhongProgram = 0;
struct BlinnPhongLocs {
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint lightPos = -1;
    GLint viewPos = -1;
    GLint lightColor = -1;
    GLint Ia = -1;
    GLint ka = -1;
    GLint kd = -1;
    GLint ks = -1;
    GLint shininess = -1;
};
static BlinnPhongLocs gBlinnPhongLocs;

// colors
const glm::vec3 LIGHT_GRAY(0.28f, 0.31f, 0.34f); // Gray
const glm::vec3 DARK_GRAY(0.11f, 0.12f, 0.14f); // Dark Gray

// robot arm part colors (kd)
const glm::vec3 JOINT_COLOR(0.25f, 0.27f, 0.30f); // dark gray: base and joints
const glm::vec3 LINK_COLOR(0.85f, 0.86f, 0.88f); // light gray: arms and connectors
const glm::vec3 GRIPPER_COLOR(0.95f, 0.55f, 0.15f); // orange: hand and hook
const glm::vec3 SCREW_COLOR(0.08f, 0.08f, 0.08f); // black: screws

// Blinn-Phong lighting parameters
const glm::vec3 LIGHT_POS(3.0f, 5.0f, 3.0f); // light position in world space
const glm::vec3 LIGHT_COLOR(1.0f, 1.0f, 1.0f); // Il
const glm::vec3 AMBIENT_INTENSITY(1.0f, 1.0f, 1.0f); // Ia
const float KA = 0.15f; // ambient reflectance
const float KS = 0.5f; // specular reflectance
const float SHININESS = 32.0f; // alpha

// point the camera at the middle of the arm (about half its height) instead of the floor
const glm::vec3 CAMERA_TARGET(0.0f, 1.1f, 0.0f);

// ###########################################################################################
// Shaders
// ###########################################################################################

// helper function to read the text from a file
// Thanks Cursor AI
static std::string read_text(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Could not open " + path);
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Thanks Cursor AI
static GLuint compile_shader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << log << "\n";
        throw std::runtime_error("Shader compile failed");
    }
    return shader;
}

// Thanks Cursor AI
static GLuint link_program(const std::string& vertPath, const std::string& fragPath) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, read_text(vertPath));
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, read_text(fragPath));
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << log << "\n";
        throw std::runtime_error("Program link failed");
    }
    return program;
}

// ###########################################################################################
// Meshes
// ###########################################################################################


static GLuint upload_positions(const std::vector<float>& positions) {
    GLuint vao = 0; // vertex array object: how to interpret the vertex data
    GLuint vbo = 0; // vertex buffer object: where to store the vertex data
    // generate the vertex array and buffer
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    // bind the vertex array and buffer
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // upload the data to the buffer
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(float)), positions.data(), GL_STATIC_DRAW);
    // set the vertex attribute pointer
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    // enable the vertex attribute array
    glEnableVertexAttribArray(0);
    // return the vertex array
    return vao;
}

// upload interleaved vertex data (px py pz nx ny nz per vertex)
// location 0 = position, location 1 = normal
static GLuint upload_mesh(const std::vector<float>& vertices) {
    GLuint vao = 0;
    GLuint vbo = 0;
    // generate the vertex array and buffer
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    // bind the vertex array and buffer
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // upload the data to the buffer
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
    // each vertex is 6 floats, so both attributes step 6 floats to reach the next vertex
    const GLsizei stride = 6 * sizeof(float);
    // position: first 3 floats of each vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    // normal: next 3 floats, starting 3 floats into the vertex
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // return the vertex array
    return vao;
}

// Thanks Cursor AI
static GLuint make_grid() {
    // create a vector to store the vertices
    std::vector<float> positions;
    // create the vertices of the grid
    for (int i = -4; i <= 4; ++i) {
        const float x = static_cast<float>(i);
        positions.insert(positions.end(), {x, 0.0f, -4.0f, x, 0.0f, 4.0f});
        positions.insert(positions.end(), {-4.0f, 0.0f, x, 4.0f, 0.0f, x});
    }
    // set the number of vertices
    gGridVertexCount = static_cast<GLuint>(positions.size() / 3);
    return upload_positions(positions);
}

static void draw_mesh(GLuint vao, GLuint count, GLenum mode, const glm::mat4& mvp, const glm::vec3& color) {
    // set the Model-View-Projection matrix
    // glUniformMatrix4fv(location, count, transpose, value)
    glUniformMatrix4fv(gMvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    // pass color to the fragment shader
    glUniform3f(gColorLoc, color.r, color.g, color.b);
    // bind the vertex array
    glBindVertexArray(vao);
    // draw the vertices
    glDrawArrays(mode, 0, static_cast<GLint>(count));
}

// ###########################################################################################
// OBJ Loader
// ###########################################################################################

// Thanks Claude code
// append one vertex (position followed by normal) to a part
static void push_vertex(Part& part, const glm::vec3& p, const glm::vec3& n) {
    part.verticesData.insert(part.verticesData.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
}

// read an obj file and split it into parts, one per "g"/"o" line
// faces are expanded into plain vertex lists so they can be drawn with glDrawArrays
static std::vector<Part> load_obj(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Could not open " + path);
    }

    // lookup tables for the whole file: face indices are global, not per group,
    // so these must never be cleared when a new group starts
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<Part> parts;

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string type;
        // skip empty lines
        if (!(ss >> type)) {
            continue;
        }

        if (type == "v") {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (type == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (type == "g" || type == "o") {
            std::string name;
            ss >> name;
            // an "o" followed by a "g" names the same object, so reuse the empty part
            if (!parts.empty() && parts.back().verticesData.empty()) {
                parts.back().name = name;
            } else {
                parts.push_back(Part{});
                parts.back().name = name;
            }
        } else if (type == "f") {
            // guard against faces that appear before any group
            if (parts.empty()) {
                parts.push_back(Part{});
                parts.back().name = "default";
            }
            Part& part = parts.back();

            // each corner looks like v/vt/vn or v//vn
            std::vector<glm::vec3> cornerPos;
            std::vector<glm::vec3> cornerNormal;
            std::string corner;
            while (ss >> corner) {
                std::istringstream cs(corner);
                std::string vi, ti, ni;
                std::getline(cs, vi, '/');
                std::getline(cs, ti, '/'); // texture coordinate, unused
                std::getline(cs, ni, '/');
                if (ni.empty()) {
                    throw std::runtime_error("Face without normal in " + path + ", export with Normals checked");
                }
                // obj indices start at 1
                cornerPos.push_back(positions.at(std::stoi(vi) - 1));
                cornerNormal.push_back(normals.at(std::stoi(ni) - 1));
            }

            // triangles pass straight through, larger polygons are split as a fan: (0,1,2), (0,2,3), ...
            for (size_t i = 1; i + 1 < cornerPos.size(); ++i) {
                push_vertex(part, cornerPos[0], cornerNormal[0]);
                push_vertex(part, cornerPos[i], cornerNormal[i]);
                push_vertex(part, cornerPos[i + 1], cornerNormal[i + 1]);
            }
        }
        // vt, s, mtllib, usemtl and comments are ignored
    }

    for (Part& part : parts) {
        part.verticesCount = static_cast<GLuint>(part.verticesData.size() / 6);
    }
    return parts;
}

// print what was loaded so it can be checked against the obj file
static void print_parts_summary(const std::vector<Part>& parts) {
    GLuint total = 0;
    glm::vec3 lo(1e9f);
    glm::vec3 hi(-1e9f);
    for (const Part& part : parts) {
        std::cout << "  " << part.name << ": " << part.verticesCount << " vertices\n";
        total += part.verticesCount;
        for (size_t i = 0; i < part.verticesData.size(); i += 6) {
            const glm::vec3 p(part.verticesData[i], part.verticesData[i + 1], part.verticesData[i + 2]);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    std::cout << parts.size() << " parts, " << total << " vertices\n";
    std::cout << "bounds x[" << lo.x << ", " << hi.x << "] y[" << lo.y << ", " << hi.y
              << "] z[" << lo.z << ", " << hi.z << "]\n";
}

// true if name begins with prefix (std::string::starts_with needs C++20)
static bool starts_with(const std::string& name, const std::string& prefix) {
    return name.rfind(prefix, 0) == 0;
}

// pick a color for each part from the name it was given in Blender
static glm::vec3 part_color(const std::string& name) {
    if (starts_with(name, "Base") || starts_with(name, "J")) return JOINT_COLOR;
    if (starts_with(name, "Arm") || starts_with(name, "Connector")) return LINK_COLOR;
    if (starts_with(name, "Hand") || starts_with(name, "Hook")) return GRIPPER_COLOR;
    if (starts_with(name, "screw")) return SCREW_COLOR;
    return LINK_COLOR;
}

// ###########################################################################################
// Transform
// ###########################################################################################

// Unity Transform: translate, then rotate, then scale. Scale applies first
// to the vertex because the vector sits on the right: M * p = T * R * S * p.
static glm::mat4 model_matrix(const glm::vec3& position, float radians, const glm::vec3& scale) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, radians, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    return model;
}

// ###########################################################################################
// Camera
// ###########################################################################################

// camera position in world space, orbiting around CAMERA_TARGET
// split out of view_matrix because the fragment shader also needs it (viewPos)
static glm::vec3 camera_eye() {
    const float cosPitch = std::cos(gApp.pitch);
    // eye position is calculated based on the pitch and yaw angles
    return CAMERA_TARGET + glm::vec3(
        gApp.distance * cosPitch * std::sin(gApp.yaw), // x coordinate
        gApp.distance * std::sin(gApp.pitch), // y coordinate
        gApp.distance * cosPitch * std::cos(gApp.yaw) // z coordinate
    );
}

// Unity Camera position and orientation
static glm::mat4 view_matrix() {
    // return the view matrix
    // lookAt(camera position, target position, up vector)
    return glm::lookAt(camera_eye(), CAMERA_TARGET, glm::vec3(0.0f, 1.0f, 0.0f));
}

// Unity Camera field of view, aspect, near plane, far plane
static glm::mat4 projection_matrix() {
    // calculate the aspect ratio
    const float aspect = gApp.height > 0
        ? static_cast<float>(gApp.width) / static_cast<float>(gApp.height)
        : 1.0f;

    // 70 degrees is the camera field of view (FOV)
    float fov = 40.0f;
    // set clipping planes
    // near plane must stay above 0. camera-space z = 0 cannot be projected
    float near = 0.1f;
    float far = 15.0f;
    return glm::perspective(glm::radians(fov), aspect, near, far);
}

// ###########################################################################################
// Scene
// ###########################################################################################

// draw every part of the robot arm with the Blinn-Phong shader
static void draw_robot(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection) {
    // uniforms only go to the program in use, so switch first
    glUseProgram(gBlinnPhongProgram);

    // values shared by every part, set once per frame
    glUniformMatrix4fv(gBlinnPhongLocs.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(gBlinnPhongLocs.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(gBlinnPhongLocs.projection, 1, GL_FALSE, glm::value_ptr(projection));
    const glm::vec3 eye = camera_eye();
    glUniform3f(gBlinnPhongLocs.viewPos, eye.x, eye.y, eye.z);
    glUniform3f(gBlinnPhongLocs.lightPos, LIGHT_POS.x, LIGHT_POS.y, LIGHT_POS.z);
    glUniform3f(gBlinnPhongLocs.lightColor, LIGHT_COLOR.r, LIGHT_COLOR.g, LIGHT_COLOR.b);
    glUniform3f(gBlinnPhongLocs.Ia, AMBIENT_INTENSITY.r, AMBIENT_INTENSITY.g, AMBIENT_INTENSITY.b);
    glUniform1f(gBlinnPhongLocs.ka, KA);
    glUniform1f(gBlinnPhongLocs.ks, KS);
    glUniform1f(gBlinnPhongLocs.shininess, SHININESS);

    // only the color (kd) changes from part to part
    for (const Part& part : gParts) {
        glUniform3f(gBlinnPhongLocs.kd, part.color.r, part.color.g, part.color.b);
        glBindVertexArray(part.VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(part.verticesCount));
    }
}

static void draw_scene(float time) {
    const glm::mat4 view = view_matrix();
    const glm::mat4 projection = projection_matrix();

    // draw the grid using lines with the simple shader
    glUseProgram(gProgram);
    draw_mesh(gGridVao, gGridVertexCount, GL_LINES, projection * view, LIGHT_GRAY);

    // the obj is already in world units with its base on the floor, so no offset or scale
    // slowly turn the arm around the y axis so the highlights move across it
    const glm::mat4 model = model_matrix(glm::vec3(0.0f), time * 0.3f, glm::vec3(1.0f));
    draw_robot(model, view, projection);
}

// ###########################################################################################
// Callbacks functions
// ###########################################################################################

static void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    gApp.width = width;
    gApp.height = height;
}

// ###########################################################################################
// Input Callbacks
// ###########################################################################################

// Thanks Cursor AI
static void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT) {
        gApp.yaw -= 0.08f;
    } else if (key == GLFW_KEY_RIGHT) {
        gApp.yaw += 0.08f;
    } else if (key == GLFW_KEY_UP) {
        gApp.pitch += 0.06f;
    } else if (key == GLFW_KEY_DOWN) {
        gApp.pitch -= 0.06f;
    } else if (key == GLFW_KEY_MINUS) {
        gApp.distance += 0.3f;
    } else if (key == GLFW_KEY_EQUAL) {
        gApp.distance -= 0.3f;
    }
}

// Thanks Cursor AI
static void mouse_button_callback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        gApp.dragging = action == GLFW_PRESS;
    }
}

// Thanks Cursor AI
static void cursor_callback(GLFWwindow*, double x, double y) {
    if (gApp.dragging) {
        gApp.yaw += static_cast<float>(x - gApp.lastX) * 0.008f;
        gApp.pitch += static_cast<float>(y - gApp.lastY) * 0.008f;
    }
    gApp.lastX = x;
    gApp.lastY = y;
}

// Thanks Cursor AI
static void scroll_callback(GLFWwindow*, double, double yOffset) {
    gApp.distance -= static_cast<float>(yOffset) * 0.4f;
}

// ###########################################################################################
// Camera Clamping
// ###########################################################################################

// Thanks Cursor AI
static void clamp_camera() {
    if (gApp.pitch > 1.15f) gApp.pitch = 1.15f;
    if (gApp.pitch < -1.15f) gApp.pitch = -1.15f;
    if (gApp.distance < 2.0f) gApp.distance = 2.0f;
    if (gApp.distance > 18.0f) gApp.distance = 18.0f;
}

// ###########################################################################################
// Main
// ###########################################################################################

int main() {
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW " << code << ": " << description << "\n";
    });
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // create the window
    GLFWwindow* window = glfwCreateWindow(gApp.width, gApp.height, "CS 6983 OpenGL", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    // make the window the current context so we can draw to it
    glfwMakeContextCurrent(window);
    // wait for the display to refresh before swapping the buffers
    glfwSwapInterval(1);
    // set the callback functions for the window
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwGetFramebufferSize(window, &gApp.width, &gApp.height);

    // run the driver
    if (!load_gl()) {
        glfwTerminate();
        return 1;
    }

    // link the shaders
    try {
        gProgram = link_program(std::string(SHADER_DIR) + "/simple_vert.glsl",
                                std::string(SHADER_DIR) + "/simple_frag.glsl");
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        glfwTerminate();
        return 1;
    }

    // link the Blinn-Phong shaders
    try {
        gBlinnPhongProgram = link_program(std::string(SHADER_DIR) + "/blinn_phong_vert.glsl",
                                          std::string(SHADER_DIR) + "/blinn_phong_frag.glsl");
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        glfwTerminate();
        return 1;
    }

    // load the robot arm model
    try {
        gParts = load_obj(std::string(MODELS_DIR) + "/assignment2.obj");
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        glfwTerminate();
        return 1;
    }
    print_parts_summary(gParts);

    // get the location of the uniform variables
    gMvpLoc = glGetUniformLocation(gProgram, "uMVP");
    gColorLoc = glGetUniformLocation(gProgram, "uColor");

    // locations belong to a program, so the Blinn-Phong ones are looked up separately
    // names must match the shader exactly; -1 means a typo or a uniform the shader never uses
    gBlinnPhongLocs.model = glGetUniformLocation(gBlinnPhongProgram, "model");
    gBlinnPhongLocs.view = glGetUniformLocation(gBlinnPhongProgram, "view");
    gBlinnPhongLocs.projection = glGetUniformLocation(gBlinnPhongProgram, "projection");
    gBlinnPhongLocs.lightPos = glGetUniformLocation(gBlinnPhongProgram, "lightPos");
    gBlinnPhongLocs.viewPos = glGetUniformLocation(gBlinnPhongProgram, "viewPos");
    gBlinnPhongLocs.lightColor = glGetUniformLocation(gBlinnPhongProgram, "lightColor");
    gBlinnPhongLocs.Ia = glGetUniformLocation(gBlinnPhongProgram, "Ia");
    gBlinnPhongLocs.ka = glGetUniformLocation(gBlinnPhongProgram, "ka");
    gBlinnPhongLocs.kd = glGetUniformLocation(gBlinnPhongProgram, "kd");
    gBlinnPhongLocs.ks = glGetUniformLocation(gBlinnPhongProgram, "ks");
    gBlinnPhongLocs.shininess = glGetUniformLocation(gBlinnPhongProgram, "shininess");
    for (GLint loc : {gBlinnPhongLocs.model, gBlinnPhongLocs.view, gBlinnPhongLocs.projection,
                      gBlinnPhongLocs.lightPos, gBlinnPhongLocs.viewPos, gBlinnPhongLocs.lightColor,
                      gBlinnPhongLocs.Ia, gBlinnPhongLocs.ka, gBlinnPhongLocs.kd, gBlinnPhongLocs.ks,
                      gBlinnPhongLocs.shininess}) {
        if (loc == -1) {
            std::cerr << "warning: a Blinn-Phong uniform was not found in the shader\n";
        }
    }

    // make the meshes
    gGridVao = make_grid();

    // upload every part and give it a color based on its name
    for (auto& part : gParts) {
        part.VAO = upload_mesh(part.verticesData);
        part.color = part_color(part.name);
    }

    // enable the depth test
    glEnable(GL_DEPTH_TEST);
    // set the background color
    glClearColor(DARK_GRAY.r, DARK_GRAY.g, DARK_GRAY.b, 1.0f);

    std::cout << "drag or arrows to orbit, wheel or -/= to zoom, Esc quits\n";
    // main loop
    while (!glfwWindowShouldClose(window)) {
        // poll for events (inputs)
        glfwPollEvents();
        // get the time
        const float time = static_cast<float>(glfwGetTime());
        // clamp the camera to prevent it from going too far
        clamp_camera();

        // set the viewport and clear the color and depth buffers to prepare for the next frame
        glViewport(0, 0, gApp.width, gApp.height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw the scene
        draw_scene(time);
        // swap the buffers to display the new frame
        glfwSwapBuffers(window);
    }

    // terminate the window
    glfwTerminate();
    return 0;
}
