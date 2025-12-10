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
#include <deque>

#ifndef __Main_cpp__
#define __Main_cpp__

#ifdef __APPLE__
#define CLOCKS_PER_SEC 100000
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Particle structure to hold physics data
struct Particle
{
    Vector3 position;
    Vector3 velocity;
    float lifetime;      // Time since emission
    float max_lifetime;  // When to recycle
    bool active;
    
    // Trail system - store recent positions
    std::deque<Vector3> trail_positions;
    int max_trail_length;
    
    Particle() : position(0,0,0), velocity(0,0,0), lifetime(0), max_lifetime(2.0f), 
                 active(false), max_trail_length(15) {}
};

class MyDriver : public OpenGLViewer
{
    std::vector<OpenGLTriangleMesh *> mesh_object_array;
    OpenGLBgEffect *bgEffect = nullptr;
    OpenGLSkybox *skybox = nullptr;
    clock_t startTime;
    
    // Store references to animated objects
    OpenGLTriangleMesh* head_object = nullptr;
    OpenGLTriangleMesh* body_object = nullptr;
    
    // Particle system
    std::vector<Particle> particles;
    std::vector<OpenGLTriangleMesh*> particle_meshes;
    std::vector<OpenGLTriangleMesh*> trail_meshes;  // One trail mesh per particle
    std::mt19937 rng;
    
    // Particle system parameters
    Vector3 emission_point;
    int num_particles;
    float emission_rate;
    float last_emission_time;

public:
    MyDriver() : rng(std::random_device{}()), num_particles(100), emission_rate(1.6f), last_emission_time(0.0f)
    {
        // Set emission point at neck area
        emission_point = Vector3(0.2f, 1.3f, 0.0f);
    }

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
        OpenGLShaderLibrary::Instance()->Add_Shader_From_File("shaders/basic.vert", "shaders/rust.frag", "rust");
        OpenGLShaderLibrary::Instance()->Add_Shader_From_File("shaders/basic.vert", "shaders/sparks.frag", "sparks");

        // Add lights
        opengl_window->Add_Light(Vector3f(3, 1, 3), Vector3f(0.1, 0.1, 0.1), Vector3f(1, 1, 1), Vector3f(0.5, 0.5, 0.5));
        opengl_window->Add_Light(Vector3f(-3, 1, 3), Vector3f(0.05, 0.02, 0.03), Vector3f(0.4, 0.2, 0.3), Vector3f(0.4, 0.2, 0.3));

        // Initialize particle system
        Initialize_Particles();

        // Load head object
        {
            head_object = Add_Obj_Mesh_Object("obj/repo_head.obj");
            
            // Initial positioning of head
            Matrix4f t;
            t << 1, 0, 0, -1.82,
                 0, 1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1;
            head_object->Set_Model_Matrix(t);

            // Set material properties
            head_object->Set_Ka(Vector3f(0.2, 0.2, 0.2));
            head_object->Set_Kd(Vector3f(0.7, 0.6, 0.5));
            head_object->Set_Ks(Vector3f(0.5, 0.5, 0.5));
            head_object->Set_Shininess(32.0);

            // Bind shader
            head_object->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("rust"));
        }

        // Load body object
        {
            body_object = Add_Obj_Mesh_Object("obj/repo_body.obj");
            
            // Position body at origin (centered)
            Matrix4f t;
            t << 1, 0, 0, 0,
                 0, 1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1;
            body_object->Set_Model_Matrix(t);

            // Set material properties
            body_object->Set_Ka(Vector3f(0.2, 0.2, 0.3));
            body_object->Set_Kd(Vector3f(0.5, 0.6, 0.8));
            body_object->Set_Ks(Vector3f(0.6, 0.6, 0.6));
            body_object->Set_Shininess(64.0);

            // Bind shader
            body_object->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("rust"));
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

    void Initialize_Particles()
    {
        particles.resize(num_particles);
        particle_meshes.resize(num_particles);
        trail_meshes.resize(num_particles);
        
        for (int i = 0; i < num_particles; i++)
        {
            // Create mesh for each particle
            particle_meshes[i] = Add_Obj_Mesh_Object("obj/sphere.obj");
            
            // Initial scale and position
            float scale = 0.02f;
            Matrix4f t;
            t << scale, 0, 0, emission_point[0],
                 0, scale, 0, emission_point[1],
                 0, 0, scale, emission_point[2],
                 0, 0, 0, 1;
            particle_meshes[i]->Set_Model_Matrix(t);

            // Make it bright orange/yellow (emissive look)
            particle_meshes[i]->Set_Ka(Vector3f(1.0, 0.6, 0.1));
            particle_meshes[i]->Set_Kd(Vector3f(1.0, 0.6, 0.1));
            particle_meshes[i]->Set_Ks(Vector3f(1.0, 1.0, 1.0));
            particle_meshes[i]->Set_Shininess(128.0);

            // Use the sparks shader for glow effect
            particle_meshes[i]->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("sparks"));
            
            // Create trail mesh for each particle
            trail_meshes[i] = Add_Interactive_Object<OpenGLTriangleMesh>();
            mesh_object_array.push_back(trail_meshes[i]);
            
            trail_meshes[i]->Set_Ka(Vector3f(1.0, 0.6, 0.1));
            trail_meshes[i]->Set_Kd(Vector3f(1.0, 0.6, 0.1));
            trail_meshes[i]->Set_Ks(Vector3f(1.0, 1.0, 1.0));
            trail_meshes[i]->Set_Shininess(128.0);
            trail_meshes[i]->Add_Shader_Program(OpenGLShaderLibrary::Get_Shader("sparks"));
            
            Set_Polygon_Mode(trail_meshes[i], PolygonMode::Fill);
            Set_Shading_Mode(trail_meshes[i], ShadingMode::Phong);
            
            // Start inactive
            particles[i].active = false;
            particle_meshes[i]->visible = false;
            trail_meshes[i]->visible = false;
        }
    }

    void Emit_Particle(int index, float time)
    {
        std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> speed_dist(1.5f, 3.0f);
        std::uniform_real_distribution<float> upward_dist(0.3f, 0.7f);
        std::uniform_real_distribution<float> lifetime_dist(1.5f, 2.5f);
        
        particles[index].position = emission_point;
        particles[index].lifetime = 0.0f;
        particles[index].max_lifetime = lifetime_dist(rng);
        particles[index].active = true;
        
        // Clear trail
        particles[index].trail_positions.clear();
        particles[index].trail_positions.push_back(emission_point);
        
        // Random direction with upward bias
        float angle = angle_dist(rng);
        float speed = speed_dist(rng);
        float upward_bias = upward_dist(rng);
        
        // Cone emission with upward and rightward bias
        particles[index].velocity = Vector3(
            speed * upward_bias + 2.0f,  // x component
            cos(angle) * speed * 0.7f,   // y component
            sin(angle) * speed * 0.7f    // z component
        );
        
        particle_meshes[index]->visible = true;
        trail_meshes[index]->visible = true;
    }

    // Create a tube mesh between positions to form the trail
    void Update_Trail_Mesh(int particle_idx)
    {
        auto& trail_positions = particles[particle_idx].trail_positions;
        auto trail_mesh = trail_meshes[particle_idx];
        
        if (trail_positions.size() < 2)
        {
            trail_mesh->visible = false;
            return;
        }
        
        // Build a tube along the trail path
        std::vector<Vector3> vertices;
        std::vector<Vector3i> triangles;
        
        float base_radius = 0.008f;  // Thin trail
        int segments = 6;
        
        // Create vertices for each position in the trail
        for (size_t i = 0; i < trail_positions.size(); i++)
        {
            Vector3 pos = trail_positions[i];
            
            // Fade radius toward the back of the trail
            float fade = (float)i / (float)(trail_positions.size() - 1);
            float radius = base_radius * fade;
            
            // Calculate perpendicular vectors for the tube
            Vector3 forward;
            if (i < trail_positions.size() - 1)
                forward = (trail_positions[i + 1] - pos).normalized();
            else
                forward = (pos - trail_positions[i - 1]).normalized();
            
            // Create perpendicular vectors
            Vector3 right;
            if (abs(forward[1]) < 0.9f)
                right = forward.cross(Vector3(0, 1, 0)).normalized();
            else
                right = forward.cross(Vector3(1, 0, 0)).normalized();
            
            Vector3 up = forward.cross(right).normalized();
            
            // Create ring of vertices
            for (int j = 0; j < segments; j++)
            {
                float angle = 2.0f * M_PI * j / segments;
                Vector3 offset = (right * cos(angle) + up * sin(angle)) * radius;
                vertices.push_back(pos + offset);
            }
        }
        
        // Create triangles connecting the rings
        for (size_t i = 0; i < trail_positions.size() - 1; i++)
        {
            for (int j = 0; j < segments; j++)
            {
                int current = i * segments + j;
                int next = i * segments + (j + 1) % segments;
                int current_next_ring = (i + 1) * segments + j;
                int next_next_ring = (i + 1) * segments + (j + 1) % segments;
                
                // Two triangles per quad
                triangles.push_back(Vector3i(current, next, current_next_ring));
                triangles.push_back(Vector3i(next, next_next_ring, current_next_ring));
            }
        }
        
        // Update the mesh
        trail_mesh->mesh.Vertices() = vertices;
        trail_mesh->mesh.Elements() = triangles;
        
        // Compute normals
        Compute_Vertex_Normals(trail_mesh);
        
        // Refresh the mesh data
        trail_mesh->Set_Data_Refreshed();
        trail_mesh->Initialize();
        trail_mesh->visible = true;
    }

    void Update_Particles(float dt)
    {
        Vector3 gravity(0, -9.8f, 0);  // Gravity acceleration
        
        for (int i = 0; i < num_particles; i++)
        {
            if (!particles[i].active) continue;
            
            // Update lifetime
            particles[i].lifetime += dt;
            
            // Check if particle should be recycled
            if (particles[i].lifetime >= particles[i].max_lifetime)
            {
                particles[i].active = false;
                particle_meshes[i]->visible = false;
                trail_meshes[i]->visible = false;
                continue;
            }
            
            // Apply gravity
            particles[i].velocity += gravity * dt;
            
            // Update position
            particles[i].position += particles[i].velocity * dt;
            
            // Add current position to trail
            particles[i].trail_positions.push_back(particles[i].position);
            
            // Limit trail length
            if (particles[i].trail_positions.size() > (size_t)particles[i].max_trail_length)
            {
                particles[i].trail_positions.pop_front();
            }
            
            // Update particle head mesh
            float scale = 0.02f;
            
            // Fade out near end of life
            float life_ratio = particles[i].lifetime / particles[i].max_lifetime;
            if (life_ratio > 0.8f)
            {
                float fade = 1.0f - (life_ratio - 0.8f) / 0.2f;
                scale *= fade;
            }
            
            Matrix4f t;
            t << scale, 0, 0, particles[i].position[0],
                 0, scale, 0, particles[i].position[1],
                 0, 0, scale, particles[i].position[2],
                 0, 0, 0, 1;
            particle_meshes[i]->Set_Model_Matrix(t);
            
            // Update the trail mesh geometry
            Update_Trail_Mesh(i);
        }
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
                n = Vector3(0, 1, 0);
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
        float dt = 1.0f / 60.0f;
        
        // Emit new particles periodically (burst of sparks)
        if (time - last_emission_time >= emission_rate)
        {
            // Emit a burst of 5-10 particles at once
            std::uniform_int_distribution<int> burst_dist(5, 10);
            int burst_count = burst_dist(rng);
            
            int emitted = 0;
            for (int i = 0; i < num_particles && emitted < burst_count; i++)
            {
                if (!particles[i].active)
                {
                    Emit_Particle(i, time);
                    emitted++;
                }
            }
            
            last_emission_time = time;
        }
        
        // Update all particles
        Update_Particles(dt);
        
        // Animate the head - twitching rotation
        if (head_object)
        {
            Matrix4f translation;
            translation << 1, 0, 0, -1.82,
                          0, 1, 0, 0,
                          0, 0, 1, 0,
                          0, 0, 0, 1;
            
            float rotation_angle = sin(time * 6.0f) * 0.6f;
            
            float c = cos(rotation_angle);
            float s = sin(rotation_angle);
            
            Matrix4f rotation;
            rotation << c,  0, s, 0,
                       0,  1, 0, 0,
                      -s,  0, c, 0,
                       0,  0, 0, 1;
            
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