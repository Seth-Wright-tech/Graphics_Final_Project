#include "Common.h"
#include "OpenGLCommon.h"
#include "OpenGLMarkerObjects.h"
#include "OpenGLBgEffect.h"
#include "OpenGLMesh.h"
#include "OpenGLViewer.h"
#include "OpenGLWindow.h"
#include "TinyObjLoader.h"
#include "OpenGLSkybox.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>
#include <string>

#ifndef __Main_cpp__
#define __Main_cpp__

#ifdef __APPLE__
#define CLOCKS_PER_SEC 100000
#endif

class MyDriver : public OpenGLViewer
{
    std::vector<OpenGLTriangleMesh *> mesh_object_array;
    OpenGLBgEffect *bgEffect = nullptr;
    OpenGLSkybox *skybox = nullptr;
    clock_t startTime;
    
    // Store references to animated objects
    OpenGLTriangleMesh* head_object = nullptr;
    OpenGLTriangleMesh* body_object = nullptr;

public:
    virtual void Initialize()
    {
        draw_axes = false;
        startTime = clock();
        OpenGLViewer::Initialize();
    }

    virtual void Initialize_Data()
    {
        // Load shaders
        OpenGLShaderLibrary::Instance()->Add_Shader_From_File("shaders/basic.vert", "shaders/basic.frag", "basic");

        // Add lights
        opengl_window->Add_Light(Vector3f(3, 1, 3), Vector3f(0.1, 0.1, 0.1), Vector3f(1, 1, 1), Vector3f(0.5, 0.5, 0.5));
        opengl_window->Add_Light(Vector3f(-3, 1, 3), Vector3f(0.05, 0.02, 0.03), Vector3f(0.4, 0.2, 0.3), Vector3f(0.4, 0.2, 0.3));

        // Load head object
        {
            head_object = Add_Obj_Mesh_Object("obj/repo_head.obj");
            
            // Initial position (will be animated in Toggle_Next_Frame)
            Matrix4f t;
            t << 1, 0, 0, -1.82,      // x position (centered)
                 0, 1, 0, 0,    // y position (adjust this to move head up/down)
                 0, 0, 1, 0,      // z position
                 0, 0, 0, 1;
            head_object->Set_Model_Matrix(t);

            // Set material properties
            head_object->Set_Ka(Vector3f(0.2, 0.2, 0.2));
            head_object->Set_Kd(Vector3f(0.7, 0.6, 0.5));  // Tan/beige color
            head_object->Set_Ks(Vector3f(0.5, 0.5, 0.5));
            head_object->Set_Shininess(32.0);

            // Bind shader
            head_object->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("basic"));
        }

        // Load body object
        {
            body_object = Add_Obj_Mesh_Object("obj/repo_body.obj");
            
            // Position body at origin (centered)
            Matrix4f t;
            t << 1, 0, 0, 0,      // x position (centered)
                 0, 1, 0, 0,      // y position (at origin)
                 0, 0, 1, 0,      // z position
                 0, 0, 0, 1;
            body_object->Set_Model_Matrix(t);

            // Set material properties
            body_object->Set_Ka(Vector3f(0.2, 0.2, 0.3));
            body_object->Set_Kd(Vector3f(0.5, 0.6, 0.8));  // Blue color
            body_object->Set_Ks(Vector3f(0.6, 0.6, 0.6));
            body_object->Set_Shininess(64.0);

            // Bind shader
            body_object->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("basic"));
        }

        // Initialize all mesh objects
        for (auto &mesh_obj : mesh_object_array)
        {
            Set_Polygon_Mode(mesh_obj, PolygonMode::Fill);
            Set_Shading_Mode(mesh_obj, ShadingMode::Phong);
            mesh_obj->Set_Data_Refreshed();
            mesh_obj->Initialize();
        }

        Toggle_Play();
    }

    // Load mesh from OBJ file and merge all submeshes
    OpenGLTriangleMesh *Add_Obj_Mesh_Object(std::string obj_file_name)
    {
        auto mesh_obj = Add_Interactive_Object<OpenGLTriangleMesh>();
        Array<std::shared_ptr<TriangleMesh<3>>> meshes;
        
        // Load OBJ (may contain multiple sub-objects)
        Obj::Read_From_Obj_File_Discrete_Triangles(obj_file_name, meshes);

        TriangleMesh<3> merged;

        // Merge all submeshes into one
        for (auto &m : meshes)
        {
            int offset = (int)merged.Vertices().size();

            for (auto &v : m->Vertices())
                merged.Vertices().push_back(v);

            for (auto &f : m->Elements())
                merged.Elements().push_back(Vector3i(
                    f[0] + offset,
                    f[1] + offset,
                    f[2] + offset
                ));
        }

        mesh_obj->mesh = merged;
        
        // Compute normals for Phong shading
        Compute_Vertex_Normals(mesh_obj);
        
        std::cout << "Loaded mesh: " << obj_file_name 
                  << ", vertices: " << mesh_obj->mesh.Vertices().size() 
                  << ", triangles: " << mesh_obj->mesh.Elements().size() 
                  << ", normals: " << mesh_obj->mesh.Normals().size() << std::endl;

        mesh_object_array.push_back(mesh_obj);
        return mesh_obj;
    }

    // Compute smooth per-vertex normals
    void Compute_Vertex_Normals(OpenGLTriangleMesh* mesh_obj)
    {
        auto& vertices = mesh_obj->mesh.Vertices();
        auto& triangles = mesh_obj->mesh.Elements();
        
        // Initialize normals to zero
        std::vector<Vector3> normals(vertices.size(), Vector3(0, 0, 0));
        
        // Accumulate face normals at each vertex
        for (const auto& tri : triangles)
        {
            Vector3 v0 = vertices[tri[0]];
            Vector3 v1 = vertices[tri[1]];
            Vector3 v2 = vertices[tri[2]];
            
            // Compute face normal via cross product
            Vector3 edge1 = v1 - v0;
            Vector3 edge2 = v2 - v0;
            Vector3 face_normal = edge1.cross(edge2);
            
            // Add to each vertex (area-weighted)
            normals[tri[0]] += face_normal;
            normals[tri[1]] += face_normal;
            normals[tri[2]] += face_normal;
        }
        
        // Normalize all normals
        for (auto& n : normals)
        {
            float length = n.norm();
            if (length > 1e-6f)
                n /= length;
            else
                n = Vector3(0, 1, 0);  // fallback
        }
        
        mesh_obj->mesh.Normals() = normals;
    }

    // Create mesh from vertex and triangle arrays
    OpenGLTriangleMesh* Add_Tri_Mesh_Object(const std::vector<Vector3>& vertices, const std::vector<Vector3i>& elements)
    {
        auto obj = Add_Interactive_Object<OpenGLTriangleMesh>();
        mesh_object_array.push_back(obj);
        obj->mesh.Vertices() = vertices;
        obj->mesh.Elements() = elements;
        
        // Compute normals
        Compute_Vertex_Normals(obj);
        
        return obj;
    }
    

    virtual void Toggle_Next_Frame()
    {
        // Get current time
        float time = GLfloat(clock() - startTime) / CLOCKS_PER_SEC;
        
        // Animate the head - twitching rotation
        if (head_object)
        {
            // Translation to move pivot to originMatrix4f translation;
            Matrix4f translation;
            translation << 1, 0, 0, -1.82,
                          0, 1, 0, 0,
                          0, 0, 1, 0,
                          0, 0, 0, 1;
            // Create rotation angle that oscillates (twitches back and forth)
            float rotation_angle = sin(time * 6.0f) * 0.6f;  // Adjust speed (3.0) and amount (0.3)
            
            // Create rotation matrix around Y axis (left-right twitch)
            float c = cos(rotation_angle);
            float s = sin(rotation_angle);
            
            Matrix4f rotation;
            rotation << c,  0, s, 0,
                       0,  1, 0, 0,
                      -s,  0, c, 0,
                       0,  0, 0, 1;
            
            // Combine: first rotate, then translate
            head_object->Set_Model_Matrix(rotation * translation);
        }
        
        for (auto &mesh_obj : mesh_object_array)
            mesh_obj->setTime(time);

        if (bgEffect){
            bgEffect->setResolution((float)Win_Width(), (float)Win_Height());
            bgEffect->setTime(time);
            bgEffect->setFrame(frame++);
        }

        if (skybox){
            skybox->setTime(time);
        }

        OpenGLViewer::Toggle_Next_Frame();
    }

    virtual void Run()
    {
        OpenGLViewer::Run();
    }
};

int main(int argc, char *argv[])
{
    MyDriver driver;
    driver.Initialize();
    driver.Run();
}

#endif