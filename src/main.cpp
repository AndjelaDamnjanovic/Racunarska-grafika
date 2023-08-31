#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebufferSizeCallback(GLFWwindow *window, int width, int height);

void mouseCallback(GLFWwindow *window, double xpos, double ypos);

void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(char const *path);

void setOurLights(Shader shader);

// settings
 unsigned int SCR_WIDTH = 800;
 unsigned int SCR_HEIGHT = 600;
float exposure = 0.5f;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// postavljenje naseg osvetljenja na scenu
bool spotLightOn = false;
bool blinn = true;
glm::vec3 lightPosition(-5.0f, 4.3f, 2.6f);

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//! Svetlo i njegove komponente predstavljamo kao strukturu.
struct PointLight {
        glm::vec3 position;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;

        float constant;
        float linear;
        float quadratic;
};

//! Stanje programa pamtimo kao strukturu
struct ProgramState {
        glm::vec3 clearColor = glm::vec3(0);
        bool ImGuiEnabled = false;
        Camera camera;
        bool CameraMouseMovementUpdateEnabled = true;
        glm::vec3 skullPosition = glm::vec3(1.0f);
        float skullScale = 0.1f;
        PointLight pointLight;
        ProgramState() : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

        void saveToFile(std::string filename);

        void loadFromFile(std::string filename);
};

//! Cuvamo dobijene rezultate u fajl.
void ProgramState::saveToFile(std::string filename)
{
        std::ofstream out(filename);
        out << clearColor.r << '\n'
            << clearColor.g << '\n'
            << clearColor.b << '\n'
            << ImGuiEnabled << '\n'
            << camera.Position.x << '\n'
            << camera.Position.y << '\n'
            << camera.Position.z << '\n'
            << camera.Front.x << '\n'
            << camera.Front.y << '\n'
            << camera.Front.z << '\n';
}

//! Ucitavamo stanje iz fajla.
void ProgramState::loadFromFile(std::string filename)
{
        std::ifstream in(filename);
        if (in) {
                in >> clearColor.r >> clearColor.g >> clearColor.b >> ImGuiEnabled >>
                    camera.Position.x >> camera.Position.y >> camera.Position.z >>
                    camera.Front.x >> camera.Front.y >> camera.Front.z;
        }
}

ProgramState *programState;

void drawImGui(ProgramState *programState);

unsigned int loadCubemap(vector<string> vector1);

int main()
{
        // glfw: initialize and configure
        // ------------------------------
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        // glfw window creation
        // --------------------
        GLFWwindow *window =
            glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
        if (window == nullptr) {
                std::cout << "Failed to create GLFW window" << std::endl;
                glfwTerminate();
                return -1;
        }
        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
        glfwSetCursorPosCallback(window, mouseCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);
        // tell GLFW to capture our mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // glad: load all OpenGL function pointers
        // ---------------------------------------
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cout << "Failed to initialize GLAD" << std::endl;
                return -1;
        }

        // tell stb_image.h to flip loaded texture's on the y-axis (before loading
        // model).
        stbi_set_flip_vertically_on_load(true);

        programState = new ProgramState;
        programState->loadFromFile("resources/program_state.txt");
        if (programState->ImGuiEnabled) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        // Init Imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        // configure global opengl state
        // -----------------------------
        glEnable(GL_DEPTH_TEST);

        // build and compile shaders
        // -------------------------
        Shader ourShader("resources/shaders/modelLighting.vs",
                         "resources/shaders/modelLighting.fs");
        // shader za kocku
        Shader yellowShader("resources/shaders/yellow_light.vs",
                            "resources/shaders/yellow_light.fs");
        // dodajemo skybox shader
        Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
        Shader textureShader("resources/shaders/texture.vs",
                             "resources/shaders/texture.fs");


        // load models
        // -----------
        Model myModel("resources/objects/skull/12140_Skull_v3_L2.obj");
        Model myModel2("resources/objects/daisy/10441_Daisy_v1_max2010_iteration-2.obj");
        Model myModel3("resources/objects/book/ScrollBookCandle.obj");

        myModel.SetShaderTextureNamePrefix("material.");
        myModel2.SetShaderTextureNamePrefix("material.");
        myModel3.SetShaderTextureNamePrefix("material.");

        PointLight &pointLight = programState->pointLight;
        pointLight.position = glm::vec3(4.0f, 4.0, 0.0);
        pointLight.ambient = glm::vec3(0.1, 0.1, 0.1);
        pointLight.diffuse = glm::vec3(0.6, 0.6, 0.6);
        pointLight.specular = glm::vec3(1.0, 1.0, 1.0);

        pointLight.constant = 1.0f;
        pointLight.linear = 0.09f;
        pointLight.quadratic = 0.032f;

        // skybox temena
        float skyboxVertices[] = {
            // pozicije
            -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
            1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

            -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
            -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

            1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

            -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
            1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

        unsigned int skyboxVAO, skyboxVBO;
        glGenVertexArrays(1, &skyboxVAO);
        glGenBuffers(1, &skyboxVBO);
        glBindVertexArray(skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
                     GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        // kocka na sceni
        float vertices[] = {
            // pozicije                         // koordinate tekstura          //
            // normale
            -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  0.0f,  0.0f,  -1.0f, 0.5f,  0.5f,
            -0.5f, 1.0f,  1.0f,  0.0f,  0.0f,  -1.0f, 0.5f,  -0.5f, -0.5f, 1.0f,
            0.0f,  0.0f,  0.0f,  -1.0f, 0.5f,  0.5f,  -0.5f, 1.0f,  1.0f,  0.0f,
            0.0f,  -1.0f, -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  0.0f,  0.0f,  -1.0f,
            -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  0.0f,  -1.0f,

            -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.5f,  -0.5f,
            0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,
            1.0f,  0.0f,  0.0f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  1.0f,  0.0f,
            0.0f,  1.0f,  -0.5f, 0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f,

            -0.5f, 0.5f,  0.5f,  1.0f,  0.0f,  -1.0f, 0.0f,  0.0f,  -0.5f, 0.5f,
            -0.5f, 1.0f,  1.0f,  -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,
            1.0f,  -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,  -1.0f,
            0.0f,  0.0f,  -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  -1.0f, 0.0f,  0.0f,
            -0.5f, 0.5f,  0.5f,  1.0f,  0.0f,  -1.0f, 0.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f,  0.5f,  -0.5f,
            -0.5f, 0.0f,  1.0f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  -0.5f, 1.0f,
            1.0f,  1.0f,  0.0f,  0.0f,  0.5f,  -0.5f, -0.5f, 0.0f,  1.0f,  1.0f,
            0.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            0.5f,  -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,  0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f,
            -0.5f, 1.0f,  1.0f,  0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, 0.5f,  1.0f,
            0.0f,  0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.0f,
            -1.0f, 0.0f,  -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  0.0f,  -1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,  0.0f,  -1.0f, 0.0f,

            -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,
            0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  -0.5f, 1.0f,
            1.0f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
            1.0f,  0.0f,  -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, 0.5f,  0.5f,  0.0f,  0.0f,  0.0f,  1.0f,  0.0f

        };

        // pravimo VBO i VAO bafere za kocku
        unsigned int VBO_cube, VAO_cube;
        glGenVertexArrays(1, &VAO_cube);
        glGenBuffers(1, &VBO_cube);

        glBindVertexArray(VAO_cube);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_cube);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // VBO i VAO baferi za kocku sa teksturom -TREBA LI UPOSTE?
        unsigned int VBO_texture, VAO_texture;
        glGenVertexArrays(1, &VAO_texture);
        glGenBuffers(1, &VBO_texture);

        glBindVertexArray(VAO_texture);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void *)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        unsigned int texture =
            loadTexture(FileSystem::getPath("resources/textures/ophelia.jpg").c_str());

        textureShader.use();
        textureShader.setInt("texture_diffuse1", 0);
        // ucitavanje skybox modela
        stbi_set_flip_vertically_on_load(false);
        vector<std::string> faces{FileSystem::getPath("resources/textures/right.jpg"),
                                  FileSystem::getPath("resources/textures/left.jpg"),
                                  FileSystem::getPath("resources/textures/top.jpg"),
                                  FileSystem::getPath("resources/textures/bottom.jpg"),
                                  FileSystem::getPath("resources/textures/front.jpg"),
                                  FileSystem::getPath("resources/textures/back.jpg")};

        unsigned int cubemapTexture = loadCubemap(faces);

        skyboxShader.use();
        skyboxShader.setInt("skybox", 0);

        // draw in wireframe
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        // render loop
        // -----------
        while (!glfwWindowShouldClose(window)) {
                // per-frame time logic
                // --------------------
                float currentFrame = glfwGetTime();
                deltaTime = currentFrame - lastFrame;
                lastFrame = currentFrame;

                // input
                // -----
                processInput(window);

                // render
                // ------
                glClearColor(programState->clearColor.r, programState->clearColor.g,
                             programState->clearColor.b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // don't forget to enable shader before setting uniforms
                ourShader.use();
                setOurLights(ourShader);

                // view/projection transformations
                glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                    (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
                glm::mat4 view = programState->camera.GetViewMatrix();
                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);
                ourShader.setInt("blinn", blinn);

                // render the loaded model
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(
                    model,
                    glm::vec3(
                        -7.0f, -0.13f,
                        -0.5f)); // translate it down so it's at the center of the scene
                model = glm::rotate(model, glm::radians(270.0f), glm::vec3(1, 0, 0));
                model = glm::scale(
                    model,
                    glm::vec3(programState->skullScale)); // it's a bit too big for our
                                                          // scene, so scale it down
                ourShader.setMat4("model", model);
                myModel.Draw(ourShader);
                // renderuj prvu belu radu
                glm::mat4 model2 = glm::mat4(1.0f);
                model2 = glm::translate(model2, glm::vec3(-9.0, 0.27f, -1.0f));
                model2 = glm::rotate(model2, glm::radians(0.0f), glm::vec3(1, 0, 1));
                model2 = glm::scale(model2, glm::vec3(0.20f));
                ourShader.setMat4("model", model2);
                myModel2.Draw(ourShader);

                // renderuj drugu belu radu
                glm::mat4 model3 = glm::mat4(1.0f);
                model3 = glm::translate(model3, glm::vec3(-9.2, 0.27f, -1.0f));
                model3 = glm::rotate(model3, glm::radians(0.0f), glm::vec3(1, 0, 1));
                model3 = glm::scale(model3, glm::vec3(0.20f));
                ourShader.setMat4("model", model3);
                myModel2.Draw(ourShader);

                // renderujemo i poslednji model-book, candle, scroll
                glm::mat4 model4 = glm::mat4(1.0f);
                model4 = glm::translate(model4, glm::vec3(-15.0, -0.35f, -1.0f));
                model4 = glm::rotate(model4, glm::radians(0.0f), glm::vec3(1, 0, 1));
                model4 = glm::scale(model4, glm::vec3(0.1f));
                ourShader.setMat4("model", model4);
                myModel3.Draw(ourShader);

                yellowShader.use();

                // matrice transformacija: view, projection
                yellowShader.setMat4("projection", projection);
                yellowShader.setMat4("view", view);

                // model matrica i render kocke
                glm::mat4 yellowModel = glm::mat4(1.0f);
                yellowModel = glm::translate(
                    yellowModel,
                    glm::vec3(-6.9f, 3.7f + sin(glfwGetTime()) * 1 / 3, -4.0f));
                yellowModel = glm::scale(yellowModel, glm::vec3(0.5, 0.5, 0.5));
                yellowShader.setMat4("model", yellowModel);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke

                glm::mat4 yellowModel1 = glm::mat4(1.0f);
                yellowModel1 = glm::translate(
                    yellowModel1,
                    glm::vec3(-7.2f, 3.7f + sin(glfwGetTime()) * 1 / 3, -3.2f));
                yellowModel1 = glm::scale(yellowModel1, glm::vec3(0.4, 0.4, 0.4));
                yellowShader.setMat4("model", yellowModel1);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke

                glm::mat4 yellowModel2 = glm::mat4(1.0f);
                yellowModel2 = glm::translate(
                    yellowModel2,
                    glm::vec3(-6.7f, 4.5f + sin(glfwGetTime()) * 1 / 3, -4.0f));
                yellowModel2 = glm::scale(yellowModel2, glm::vec3(0.4, 0.4, 0.4));
                yellowShader.setMat4("model", yellowModel2);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel3 = glm::mat4(1.0f);
                yellowModel3 = glm::translate(
                    yellowModel3,
                    glm::vec3(-7.2f, 4.0f + sin(glfwGetTime()) * 1 / 3, -5.8f));
                yellowModel3 = glm::scale(yellowModel3, glm::vec3(0.4, 0.4, 0.4));
                yellowShader.setMat4("model", yellowModel3);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke

                glm::mat4 yellowModel4 = glm::mat4(1.0f);
                yellowModel4 = glm::translate(
                    yellowModel4,
                    glm::vec3(-7.2f, 3.7f + sin(glfwGetTime()) * 1 / 3, -2.2f));
                yellowModel4 = glm::scale(yellowModel4, glm::vec3(0.3, 0.3, 0.3));
                yellowShader.setMat4("model", yellowModel4);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke

                glm::mat4 yellowModel5 = glm::mat4(1.0f);
                yellowModel5 = glm::translate(
                    yellowModel5,
                    glm::vec3(-6.7f, 5.3f + sin(glfwGetTime()) * 1 / 3, -4.0f));
                yellowModel5 = glm::scale(yellowModel5, glm::vec3(0.3, 0.3, 0.3));
                yellowShader.setMat4("model", yellowModel5);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel6 = glm::mat4(1.0f);
                yellowModel6 = glm::translate(
                    yellowModel6,
                    glm::vec3(-7.2f, 4.0f + sin(glfwGetTime()) * 1 / 3, -6.8f));
                yellowModel6 = glm::scale(yellowModel6, glm::vec3(0.3, 0.3, 0.3));
                yellowShader.setMat4("model", yellowModel6);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel8 = glm::mat4(1.0f);
                yellowModel8 = glm::translate(
                    yellowModel8,
                    glm::vec3(-6.7f, 6.1f + sin(glfwGetTime()) * 1 / 3, -4.0f));
                yellowModel8 = glm::scale(yellowModel8, glm::vec3(0.2, 0.2, 0.2));
                yellowShader.setMat4("model", yellowModel8);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel9 = glm::mat4(1.0f);
                yellowModel9 = glm::translate(
                    yellowModel9,
                    glm::vec3(-7.2f, 4.0f + sin(glfwGetTime()) * 1 / 3, -7.6f));
                yellowModel9 = glm::scale(yellowModel9, glm::vec3(0.2, 0.2, 0.2));
                yellowShader.setMat4("model", yellowModel9);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel10 = glm::mat4(1.0f);
                yellowModel10 = glm::translate(
                    yellowModel10,
                    glm::vec3(-7.2f, 3.7f + sin(glfwGetTime()) * 1 / 3, -1.4f));
                yellowModel10 = glm::scale(yellowModel10, glm::vec3(0.2, 0.2, 0.2));
                yellowShader.setMat4("model", yellowModel10);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel11 = glm::mat4(1.0f);
                yellowModel11 = glm::translate(
                    yellowModel11,
                    glm::vec3(-7.4f, 4.4f + sin(glfwGetTime()) * 1 / 3, -3.7f));
                yellowModel11 = glm::scale(yellowModel11, glm::vec3(0.4, 0.4, 0.4));
                yellowShader.setMat4("model", yellowModel11);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel12 = glm::mat4(1.0f);
                yellowModel12 = glm::translate(
                    yellowModel12,
                    glm::vec3(-7.9f, 5.0f + sin(glfwGetTime()) * 1 / 3, -3.2f));
                yellowModel12 = glm::scale(yellowModel12, glm::vec3(0.3, 0.3, 0.3));
                yellowShader.setMat4("model", yellowModel12);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel13 = glm::mat4(1.0f);
                yellowModel13 = glm::translate(
                    yellowModel13,
                    glm::vec3(-8.3f, 5.4f + sin(glfwGetTime()) * 1 / 3, -2.8f));
                yellowModel13 = glm::scale(yellowModel13, glm::vec3(0.2, 0.2, 0.2));
                yellowShader.setMat4("model", yellowModel13);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel14 = glm::mat4(1.0f);
                yellowModel14 = glm::translate(
                    yellowModel14,
                    glm::vec3(-6.0f, 4.4f + sin(glfwGetTime()) * 1 / 3, -4.3f));
                yellowModel14 = glm::scale(yellowModel14, glm::vec3(0.4, 0.4, 0.4));
                yellowShader.setMat4("model", yellowModel14);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel15 = glm::mat4(1.0f);
                yellowModel15 = glm::translate(
                    yellowModel15,
                    glm::vec3(-5.5f, 5.0f + sin(glfwGetTime()) * 1 / 3, -4.7f));
                yellowModel15 = glm::scale(yellowModel15, glm::vec3(0.3, 0.3, 0.3));
                yellowShader.setMat4("model", yellowModel15);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // model matrica i render kocke
                glm::mat4 yellowModel16 = glm::mat4(1.0f);
                yellowModel16 = glm::translate(
                    yellowModel16,
                    glm::vec3(-5.3f, 5.4f + sin(glfwGetTime()) * 1 / 3, -5.1f));
                yellowModel16 = glm::scale(yellowModel16, glm::vec3(0.2, 0.2, 0.2));
                yellowShader.setMat4("model", yellowModel16);

                glBindVertexArray(VAO_cube);
                glDrawArrays(GL_TRIANGLES, 0, 36);


                // kocka sa teksturama
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);

                textureShader.use();

                // directional light
                textureShader.setVec3("dirLight.direction", 0.0f, -5.0f, -15.0f);
                textureShader.setVec3("dirLight.ambient", 0.6, 0.4f, 0.1f);
                textureShader.setVec3("dirLight.diffuse", 0.7f, 0.7f, 0.7f);
                textureShader.setVec3("dirLight.specular", 1.0f, 1.0f, 1.0f);
                textureShader.setVec3("viewPosition", programState->camera.Position);
                textureShader.setFloat("material.shininess", 126.0f);

                // spotlight
                textureShader.setVec3("spotLight.position",
                                      programState->camera.Position);
                textureShader.setVec3("spotLight.direction", programState->camera.Front);
                textureShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
                textureShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
                textureShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
                textureShader.setFloat("spotLight.constant", 1.0f);
                textureShader.setFloat("spotLight.linear", 0.022);
                textureShader.setFloat("spotLight.quadratic", 0.0019);
                textureShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
                textureShader.setFloat("spotLight.outerCutOff",
                                       glm::cos(glm::radians(15.0f)));

                // matrice transformacija: view, projection
                textureShader.setMat4("projection", projection);
                textureShader.setMat4("view", view);

                // model matrica i render kocke sa teksturom
                glm::mat4 textureModel = glm::mat4(1.0f);
                textureModel =
                    glm::translate(textureModel, glm::vec3(-4.5f, 1.0f, 1.0f));
                textureModel = glm::scale(textureModel, glm::vec3(2.0, 2.0, 2.0));
                textureModel = glm::rotate(textureModel, glm::radians(0.8f),
                                            glm::vec3(1.0f, 0.0f, 0.0f));
                textureShader.setMat4("model", textureModel);


                glBindVertexArray(VAO_texture);
                glDrawArrays(GL_TRIANGLES, 0, 36);

                // skybox sa matricama transformacije
                glDepthMask(GL_LEQUAL > 0 ? GL_TRUE : GL_FALSE);

                skyboxShader.use();
                // matrice transformacije: view, projection ( + uklanjanje translacije
                // iz matrice pogleda)
                view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
                skyboxShader.setMat4("view", view);
                skyboxShader.setMat4("projection", projection);

                glBindVertexArray(skyboxVAO);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
                glDrawArrays(GL_TRIANGLES, 0, 36);
                glBindVertexArray(0);
                glDepthMask(GL_LESS > 0 ? GL_TRUE : GL_FALSE);

                if (programState->ImGuiEnabled)
                        drawImGui(programState);
                // glfw: swap buffers and poll IO events (keys pressed/released, mouse
                // moved etc.)
                // -------------------------------------------------------------------------------
                glfwSwapBuffers(window);
                glfwPollEvents();
        }

        programState->saveToFile("resources/program_state.txt");
        delete programState;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        // glfw: terminate, clearing all previously allocated GLFW resources.
        // ------------------------------------------------------------------
        glDeleteVertexArrays(1, &skyboxVAO);
        glDeleteVertexArrays(1, &VAO_texture);

        glDeleteBuffers(1, &VBO_texture);
        glDeleteBuffers(1, &skyboxVBO);
        glfwTerminate();
        return 0;
}

void setOurLights(Shader shader){
    shader.setVec3("light.position", lightPosition);
    shader.setVec3("viewPos", programState->camera.Position);

    // direkciono svetlo
    shader.setVec3("dirLight.direction", 0.0f, -1.0, 0.0f);
    shader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
    shader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
    shader.setVec3("dirLight.specular", 0.4f, 0.4f, 0.4f);

    // osobine pointlight svetla
    shader.setVec3("pointLights[0].position", lightPosition);
    shader.setVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
    shader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
    shader.setVec3("pointLights[0].specular", 0.4f, 0.4f, 0.4f);
    shader.setFloat("pointLights[0].constant", 1.0f);
    shader.setFloat("pointLights[0].linear", 0.09f);
    shader.setFloat("pointLights[0].quadratic", 0.032f);

    // spotLight
    shader.setVec3("spotLight.position", programState->camera.Position);
    shader.setVec3("spotLight.direction", programState->camera.Front);
    shader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);

    if(spotLightOn){
        shader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        shader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
    }else{
        shader.setVec3("spotLight.diffuse", 0.0f, 0.0f, 0.0f);
        shader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
    }

    shader.setFloat("spotLight.constant", 1.0f);
    shader.setFloat("spotLight.linear", 0.09f);
    shader.setFloat("spotLight.quadratic", 0.032f);
    shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
void processInput(GLFWwindow *window)
{
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
        // make sure the viewport matches the new window dimensions; note that width
        // and height will be significantly larger than specified on retina
        // displays.
        glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouseCallback(GLFWwindow *window, double xpos, double ypos)
{
        if (firstMouse) {
                lastX = xpos;
                lastY = ypos;
                firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset =
            lastY - ypos; // reversed since y-coordinates go from bottom to top

        lastX = xpos;
        lastY = ypos;

        if (programState->CameraMouseMovementUpdateEnabled)
                programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
        programState->camera.ProcessMouseScroll(yoffset);
}

// TO DO : skloni sve osim modela sa scene
void drawImGui(ProgramState *programState)
{
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
                static float f = 0.0f;
                ImGui::Begin("Hello window");
                ImGui::Text("Hello text");
                ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
                ImGui::ColorEdit3("Background color", (float *)&programState->clearColor);
                ImGui::DragFloat3("Backpack position",
                                  (float *)&programState->skullPosition);
                ImGui::DragFloat("Backpack scale", &programState->skullScale, 0.05, 0.1,
                                 4.0);

                ImGui::DragFloat("pointLight.constant",
                                 &programState->pointLight.constant, 0.05, 0.0, 1.0);
                ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear,
                                 0.05, 0.0, 1.0);
                ImGui::DragFloat("pointLight.quadratic",
                                 &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
                ImGui::End();
        }

        {
                ImGui::Begin("Camera info");
                const Camera &c = programState->camera;
                ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y,
                            c.Position.z);
                ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
                ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y,
                            c.Front.z);
                ImGui::Checkbox("Camera mouse update",
                                &programState->CameraMouseMovementUpdateEnabled);
                ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
        if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
                programState->ImGuiEnabled = !programState->ImGuiEnabled;
                if (programState->ImGuiEnabled) {
                        programState->CameraMouseMovementUpdateEnabled = false;
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                } else {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
        }

        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
                blinn= !blinn;
                spotLightOn = !spotLightOn;
                SCR_WIDTH = 800;
                SCR_HEIGHT = 600;
        }

        if (key == GLFW_KEY_T && action == GLFW_PRESS) {
                SCR_WIDTH=1000;
                SCR_HEIGHT=720;
        }


        if (key == GLFW_KEY_E && action == GLFW_PRESS) {
                SCR_WIDTH = 1366;
                SCR_HEIGHT =  768;
        }
}

//! Funkcija za ucitavanje tekstura iz skyboxa
unsigned int loadCubemap(vector<std::string> faces)
{
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++) {
                unsigned char *data =
                    stbi_load((faces[i].c_str()), &width, &height, &nrChannels, 0);
                if (data) {
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB,
                                     width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                } else {
                        std::cout
                            << "Cubemap texture failed to load at path: " << faces[i]
                            << std::endl;
                        stbi_image_free(data);
                }
                stbi_image_free(data);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        return textureID;
}

// ucitavanje tekstura za kocku
unsigned int loadTexture(char const *path)
{
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
        if (data) {
                GLenum format;
                GLenum iternal;
                if (nrComponents == 1)
                        format = GL_RED;
                else if (nrComponents == 3) {
                        format = GL_RGB;
                        iternal = GL_SRGB;
                } else if (nrComponents == 4) {
                        iternal = GL_SRGB_ALPHA;
                        format = GL_RGBA;
                }

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, iternal, width, height, 0, format,
                             GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(
                    GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    format == GL_RGBA
                        ? GL_CLAMP_TO_EDGE
                        : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to
                                      // prevent semi-transparent borders. Due to
                                      // interpolation it takes texels from next repeat
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                stbi_image_free(data);
        } else {
                std::cout << "Texture failed to load at path: " << path << std::endl;
                stbi_image_free(data);
        }

        return textureID;
}
