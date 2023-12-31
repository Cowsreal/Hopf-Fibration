#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <string>

#include "Renderer.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "VertexArray.hpp"
#include "VertexBufferLayout.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Camera.hpp"
#include "Controls.hpp"
#include "FrameBuffer.hpp"
#include "GlobalFunctions.hpp"
#include "render_geom/CoordinateAxis/CoordinateAxis.hpp"
#include "render_geom/Plane/Plane.hpp"
#include "render_geom/Sphere/Sphere.hpp"
#include "render_geom/Circle/Circle.hpp"
#include "render_geom/Hopf/Hopf.hpp"
#include "render_geom/Points/Points.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/color_space.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw_gl3.h"

#define PI 3.14159265358979323846

int fullscreenWidth = 1920;
int fullscreenHeight = 1080;

int windowedWidth = 1920;
int windowedHeight = 1080;

int main()
{
    float size = 50.0;
    bool render = false;
    bool menu = true;
    bool drawGround = false;
    bool drawCoordinateAxis = true;
    bool drawCircle = true;
    bool drawAsPoints = false;
    bool hideUi = false;
    bool fullscreen = false;
    bool vsync = true;
    float escCooldown = 0.5f;
    float pointSize = 5.0f;
    float elevation = 0.0f;
    double lastKeyPressTime = 0;
    int numPoints[] = {100, 20, 20, 100};
    int numGreatCircles = 1;
    int numElevationCircles = 1;
    int numPointsUniform = 20;
    std::vector<float> elevations(numElevationCircles, 0.0f);
    std::vector<float> rotationXs(numGreatCircles, 0.0f);
    std::vector<float> rotationYs(numGreatCircles, 0.0f);
    std::vector<float> rotationZs(numGreatCircles, 0.0f);
    std::string mode = "GreatCircle";

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);	//set the major version of OpenGL to 3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);	//set the minor version of OpenGL to 3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);	//set the OpenGL profile to core

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(windowedWidth, windowedHeight, "Hopf Fibration", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
    {
        std::cout << "Error!" << std::endl;
    }
    std::cout << glGetString(GL_VERSION) << std::endl;

    //INITIALIZATION OPTIONS

    GLCall(glEnable(GL_CULL_FACE));
    GLCall(glCullFace(GL_FRONT));
    GLCall(glFrontFace(GL_CCW));
    GLCall(glEnable(GL_BLEND));
    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GLCall(glViewport(0, 0, windowedWidth, windowedHeight));
    GLCall(glEnable(GL_DEPTH_TEST));
    GLCall(glDepthFunc(GL_LESS));
    //GLCall(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));

    // IMGUI INITIALIZATION
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(window, true);
    ImGui::StyleColorsDark();
    {
        Renderer renderer;

        // CREATE SHADERS

        Shader shader("res/shaders/Basic.shader"); //create a shader
        Shader shader2("res/shaders/Points.shader"); //create a shader

        // CREATE OBJECTS

        float nearPlaneDistance = 0.1;
        Camera camera(45.0f, (float)windowedWidth/ (float)windowedHeight, nearPlaneDistance, 50000.0f, window, true);
        camera.SetPosition(glm::vec3(0, 50.0f, 0.0));

        Controls controls(window);
        camera.BindControls(&controls);

        glfwSetWindowUserPointer(window, &camera);
        glfwSetCursorPosCallback(window, mouse_callback);

        Camera camera1(45.0f, (float)200 / (float)200, nearPlaneDistance, 100.0f, window, false);
        camera1.SetPosition(glm::vec3(30.0f, 30.0f, 30.0f));
        glm::vec3 cameraPos = glm::vec3(30.0f, 30.0f, 30.0f);
        glm::vec3 targetPos = glm::vec3(0.0f, 0.0f, 0.0f); // Origin
        glm::vec3 forwardVector = glm::normalize(targetPos - cameraPos);
        camera1.SetFront(forwardVector);

        std::vector<std::vector<std::vector<double>>> points;
        points.push_back(GenerateGreatCircle(rotationXs[0], rotationYs[0], rotationZs[0], numPoints[0]));
        
        std::vector<Hopf> hopfs;
        hopfs.push_back(Hopf(&(points[0]), drawAsPoints, pointSize));

        std::vector<Points> pointsDrawers;
        pointsDrawers.push_back(Points(points[0], 10.0f));

        Axis axis(10000.0f);

        Sphere sphere(15.0f, 50);

        FrameBuffer fbo = FrameBuffer();
        fbo.AttachTexture();
        
        Plane plane(10000.0f, 10000.0f);
        glm::vec3 translationA(0, -1000.0f, 0);
        
        // MOVEMENT SETTINGS

        float fov = 45.0f;
        float speed = 2.5f;
        float sensitivity = 0.1f;


        int currentMode = 0; 
        char* modes[] = {"Great Circle", "Uniform", "Random", "Elevation"};

        // RENDERING LOOP
        while (!glfwWindowShouldClose(window))
        {
            float time = glfwGetTime(); // Get the current time in seconds

            glm::mat4 viewMatrix = camera.GetViewMatrix();
            glm::mat4 projectionMatrix = camera.GetProjectionMatrix();
            /* Render here */

            ImGui_ImplGlfwGL3_NewFrame();
            if(menu)
            {
                GLCall(glClearColor(0.529f, 0.828f, 0.952f, 1.0f));
                renderer.Clear();

                ImGui::SetNextWindowSize(ImVec2(500, 500));
                ImGui::SetNextWindowPos(ImVec2(960 - 250, 540 - 250));

                ImGui::Begin("Application Controls");

                if(ImGui::Checkbox("Vsync", &vsync))
                {
                    SetVsync(vsync);
                }
                
                ImGui::Text("Movement Controls");
                ImGui::SliderFloat("FOV", &fov, 0.0f, 120.0f);
                ImGui::SliderFloat("Flying Speed", &speed, 0.0f, 50.0f);
                ImGui::SliderFloat("Mouse Sensitivity", &sensitivity, 0.0f, 2.0f);

                ImGui::Text("ESC - Show Menu");
                ImGui::Text("Left Alt - Toggle Mouse");
                ImGui::Text("F1 - Hide UI");
                ImGui::Text("F11 - Toggle Fullscreen");

                if (ImGui::Button("Exit Application"))
                {
					glfwSetWindowShouldClose(window, true);
				}
                ImGui::End();
            }
            else if(render)
            {
                if(!hideUi)
                {
                    ImGui::SetNextWindowSize(ImVec2(400, 900));
                    ImGui::SetNextWindowPos(ImVec2(20, 120)); // Position: x=50, y=50
                    ImGui::Begin("Object Controls");
                    ImGui::Checkbox("Ground", &drawGround);
                    ImGui::Checkbox("Coordinate Axis", &drawCoordinateAxis);
                    ImGui::Checkbox("Circle", &drawCircle);

                    if(ImGui::BeginCombo("Mode", modes[currentMode]))
                    {
                        for(int n = 0; n < IM_ARRAYSIZE(modes); n++)
                        {
                            bool is_selected = (currentMode == n);
                            if(ImGui::Selectable(modes[n], is_selected))
                            {
                                currentMode = n;
                                Initialize(points, currentMode, numPoints[currentMode]); 
                                pointsDrawers.clear();
                                pointsDrawers.push_back(Points(points[0], 10.0f));
                                hopfs.clear();
                                hopfs.push_back(Hopf(&(points[0]), drawAsPoints, pointSize));
                            }
                            if(is_selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if(modes[currentMode] == "Great Circle")
                    {
                        bool nChanged = ImGui::SliderInt("Fibers", &numPoints[0], 1, 300);
                        for(int i = 0; i < numGreatCircles; i++)
                        {
                            std::string label = "Great Circle " + std::to_string(i + 1);
                            ImGui::Text(label.c_str());

                            std::string xLabel = "X Rotation##" + std::to_string(i);
                            std::string yLabel = "Y Rotation##" + std::to_string(i);
                            std::string zLabel = "Z Rotation##" + std::to_string(i);

                            bool xChanged = ImGui::SliderFloat(xLabel.c_str(), &rotationXs[i], -3.14159f, 3.14159f);
                            bool yChanged = ImGui::SliderFloat(yLabel.c_str(), &rotationYs[i], -3.14159f, 3.14159f);
                            bool zChanged = ImGui::SliderFloat(zLabel.c_str(), &rotationZs[i], -3.14159f, 3.14159f);

                            if(xChanged || yChanged || zChanged || nChanged)
                            {
                                points[i] = GenerateGreatCircle(rotationXs[i], rotationYs[i], rotationZs[i], numPoints[0]);
                                pointsDrawers[i].UpdatePoints(&(points[i]));
                                hopfs[i].UpdateCircles(&(points[i]));
                            }
                        }
                        if(ImGui::Button("Reset Rotations"))
                        {
                            for(int i = 0; i < numGreatCircles; i++)
                            {
                                rotationXs[i] = 0.0f;
                                rotationYs[i] = 0.0f;
                                rotationZs[i] = 0.0f;
                                points[i] = GenerateGreatCircle(rotationXs[i], rotationYs[i], rotationZs[i], numPoints[0]);
                                pointsDrawers[i].UpdatePoints(&(points[i]));
                                hopfs[i].UpdateCircles(&(points[i]));
                            }
                        }
                        if(ImGui::SliderInt("Great Circles", &numGreatCircles, 1, 10))
                        {
                            if(numGreatCircles > hopfs.size())
                            {
                                for(int i = hopfs.size(); i < numGreatCircles; i++)
                                {
                                    rotationXs.push_back(0.0f);
                                    rotationYs.push_back(0.0f);
                                    rotationZs.push_back(0.0f);
                                    points.push_back(GenerateGreatCircle(rotationXs[i], rotationYs[i], rotationZs[i], numPoints[0]));
                                    hopfs.push_back(Hopf(&(points[i]), drawAsPoints, pointSize));
                                    pointsDrawers.push_back(Points(points[i], 10.0f));
                                }
                            }
                            else if(numGreatCircles < hopfs.size())
                            {
                                for(int i = hopfs.size(); i > numGreatCircles; i--)
                                {
                                    rotationXs.pop_back();
                                    rotationYs.pop_back();
                                    rotationZs.pop_back();
                                    points.pop_back();
                                    hopfs.pop_back();
                                    pointsDrawers.pop_back();
                                }
                            }
                        }
                    }
                    else if(modes[currentMode] == "Uniform")
                    {
                        if(ImGui::SliderInt("Number of Points", &numPoints[1], 1, 200))
                        {
                            points.clear();
                            points.push_back(GenerateUniform(numPoints[1]));
                            pointsDrawers[0].UpdatePoints(&(points[0]));
                            hopfs[0].UpdateCircles(&(points[0]));
                        }
                    }
                    else if(modes[currentMode] == "Random")
                    {
                        if(ImGui::SliderInt("Number of Points", &numPoints[2], 1, 200))
                        {
                            points.clear();
                            points.push_back(GenerateRandom(numPoints[2]));
                            pointsDrawers[0].UpdatePoints(&(points[0]));
                            hopfs[0].UpdateCircles(&(points[0]));
                        }
                    }
                    else if(modes[currentMode] == "Elevation")
                    {
                        bool nChanged = ImGui::SliderInt("Fibers", &numPoints[3], 1, 300);
                        for(int i = 0; i < numElevationCircles; i++)
                        {
                            std::string label = "Elevation Circle " + std::to_string(i + 1);
                            ImGui::Text(label.c_str());

                            std::string elevationLabel = "Elevation##" + std::to_string(i);

                            bool elevChanged = ImGui::SliderFloat(elevationLabel.c_str(), &elevations[i], -3.14159f / 2, 3.14159f / 2);

                            if(elevChanged || nChanged)
                            {
                                points[i] = GenerateElevation(numPoints[3], elevations[i]);
                                pointsDrawers[i].UpdatePoints(&(points[i]));
                                hopfs[i].UpdateCircles(&(points[i]));
                            }
                        }
                        if(ImGui::SliderInt("Elevation Circles", &numElevationCircles, 1, 10))
                        {
                            if(numElevationCircles > hopfs.size())
                            {
                                for(int i = hopfs.size(); i < numElevationCircles; i++)
                                {
                                    elevations.push_back(0.0f);
                                    points.push_back(GenerateElevation(numPoints[3], elevations[i]));
                                    hopfs.push_back(Hopf(&(points[i]), drawAsPoints, pointSize));
                                    pointsDrawers.push_back(Points(points[i], 10.0f));
                                }
                            }
                            else if(numElevationCircles < hopfs.size())
                            {
                                for(int i = hopfs.size(); i > numElevationCircles; i--)
                                {
                                    elevations.pop_back();
                                    points.pop_back();
                                    hopfs.pop_back();
                                    pointsDrawers.pop_back();
                                }
                            }
                        }
                    }
                    
                    
                    if(ImGui::Checkbox("Draw as Points", &drawAsPoints))
                    {
                        for(int i = 0; i < numGreatCircles; i++)
                        {
                            hopfs[i].SetDrawAsPoints(drawAsPoints);
                        }
                    }
                    if(drawAsPoints)
                    {
                        if(ImGui::SliderFloat("Point Size", &pointSize, 0.05f, 10.0f))
                        {
                            for(int i = 0; i < numGreatCircles; i++)
                            {
                                hopfs[i].ChangePointSize(pointSize);
                            }
                        }
                    }
                    ImGui::End();

                    // STATUS WINDOW

                    ImGui::SetNextWindowPos(ImVec2(20, 20));
                    ImGui::SetNextWindowSize(ImVec2(400, 80));
                    ImGui::Begin("Status:");
                    ImGui::Text("Camera Position: %.3f, %.3f, %.3f", camera.getPosition().x, camera.getPosition().y, camera.getPosition().z);
                    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                    ImGui::Text("Time: %.3fs", time);
                    ImGui::End();

                    // S2 Sphere Preview Window

                    fbo.Bind();
                    {
                        GLCall(glClearColor(0.0f, 0.0, 0.0, 0.5f));
                        renderer.Clear();
                        shader.Bind();
                        glm::mat4 projectionMatrix = camera1.GetProjectionMatrix();
                        glm::mat4 viewMatrix = camera1.GetViewMatrix();
                        glm::mat4 model = glm::mat4(1.0f); //create a model matrix
                        glm::mat4 mvp = projectionMatrix * viewMatrix * model;
                        shader.SetUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f); //set the uniform
                        shader.SetUniformMat4f("u_MVP", mvp); //set the uniform
                        sphere.Draw();
                        shader2.Bind();
                        shader2.SetUniformMat4f("u_MVP", mvp);
                        for(int i = 0; i < pointsDrawers.size(); i++)
                        {
                            pointsDrawers[i].Draw();
                        }
                    }
                    fbo.Unbind();
                    ImGui::SetNextWindowSize(ImVec2(300, 300));
                    ImGui::SetNextWindowPos(ImVec2(1600, 20)); // Position: x=50, y=50
                    ImGui::Begin("Rendered Image");
                    ImGui::Image((void*)(intptr_t)fbo.GetTextureID(), ImVec2(300, 300));
                    ImGui::End();
                }
                
                GLCall(glClearColor(0.529f, 0.828f, 0.952f, 1.0f));
                renderer.Clear();
                if (drawCoordinateAxis)
                {   //Coordinate axis
                    shader.Bind();
                    glm::mat4 model = glm::mat4(1.0f); //create a model matrix
                    glm::mat4 mvp = projectionMatrix * viewMatrix * model;
                    shader.SetUniformMat4f("u_MVP", mvp); //set the uniform
                    shader.SetUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f); //set the uniform
                    glLineWidth(3.0f);
                    axis.Draw();
                }
                if (drawGround)
                {   //Green plane
                    shader.Bind();
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), translationA); //create a model matrix
                    glm::mat4 mvp = projectionMatrix * viewMatrix * model;
                    shader.SetUniform4f("u_Color", 0.482f, 0.62f, 0.451f, 1.0f); //set the uniform
                    shader.SetUniformMat4f("u_MVP", mvp); //set the uniform
                    plane.Draw();
                }
                if(drawCircle)
                {
                    shader.Bind();
                    glm::mat4 model = glm::mat4(1.0f); //create a model matrix
                    glm::mat4 mvp = projectionMatrix * viewMatrix * model;
                    shader.SetUniformMat4f("u_MVP", mvp); //set the uniform
                    for(int i = 0; i < hopfs.size(); i++)
                    {
                        hopfs[i].Draw(&shader);
                    }
                }

                // CAMERA CONTROLS
                camera.SetFOV(fov);
                camera.setSpeed(speed);
                camera.setSensitivity(sensitivity);

                camera.ProcessControls();

                if(glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS && time - lastKeyPressTime > escCooldown)
                {
                    if(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
                    {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    }
                    else
                    {
                        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    }
                    lastKeyPressTime = time;
                }
                if(glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS && time - lastKeyPressTime > escCooldown)
                {
                    hideUi = !hideUi;
                    lastKeyPressTime = time;
                }
            }

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && time - lastKeyPressTime > escCooldown)
            {
                ChangeStates(menu, render);
                if(menu)
                {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                else
                {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                lastKeyPressTime = time;
            }

            if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS && time - lastKeyPressTime > escCooldown)
            {
                fullscreen = !fullscreen; // Toggle the fullscreen flag
                if (fullscreen) {
                    // Store the window size to restore later
                    glfwGetWindowSize(window, &fullscreenWidth, &fullscreenHeight);

                    // Get the primary monitor's resolution
                    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

                    // Switch to fullscreen
                    glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
                    glViewport(0, 0, mode->width, mode->height);
                    SetVsync(vsync);
                } 
                else 
                {
                    // Switch to windowed mode
                    glfwSetWindowMonitor(window, nullptr, 100, 100, windowedWidth, windowedHeight, GLFW_DONT_CARE);
                    glViewport(0, 0, windowedWidth, windowedHeight);
                    SetVsync(vsync);
                }
                lastKeyPressTime = time;
            }

            ImGui::Render();
            ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());

            /* Swap front and back buffers */
            glfwSwapBuffers(window);

            /* Poll for and process events */
            glfwPollEvents();
        }
    }
    ImGui_ImplGlfwGL3_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}