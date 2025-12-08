#version 330 core

/* Camera uniforms */
layout(std140) uniform camera
{
    mat4 projection;
    mat4 view;
    mat4 pvm;
    mat4 ortho;
    vec4 position;  // camera position in world space
};

/* Light uniforms */
struct light
{
    ivec4 att; 
    vec4 pos;   // position
    vec4 dir;
    vec4 amb;   // ambient intensity
    vec4 dif;   // diffuse intensity
    vec4 spec;  // specular intensity
    vec4 atten;
    vec4 r;
};

layout(std140) uniform lights
{
    vec4 amb;           // global ambient
    ivec4 lt_att;       // lt_att[0] = number of lights
    light lt[4];        // light array
};

/* Input from vertex shader */
in vec3 vtx_normal;     // normal in world space
in vec3 vtx_position;   // position in world space
in vec3 vtx_model_position;
in vec4 vtx_color;
in vec2 vtx_uv;
in vec3 vtx_tangent;

/* Material properties */
uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float shininess;

/* Texture samplers (not used yet) */
uniform sampler2D tex_color;
uniform sampler2D tex_normal;

/* Output */
out vec4 frag_color;

/* Phong shading function */
vec3 shading_phong(light li, vec3 e, vec3 p, vec3 s, vec3 n)
{
    // Ambient
    vec3 ambient = li.amb.rgb * ka;
    
    // Diffuse
    vec3 l = normalize(s - p);
    float diff = max(dot(n, l), 0.0);
    vec3 diffuse = li.dif.rgb * kd * diff;
    
    // Specular
    vec3 v = normalize(e - p);
    vec3 r = reflect(-l, n);
    float spec = pow(max(dot(r, v), 0.0), shininess);
    vec3 specular = li.spec.rgb * ks * spec;
    
    return ambient + diffuse + specular;
}

void main()
{
    vec3 e = position.xyz;          // eye position
    vec3 p = vtx_position;          // surface position
    vec3 n = normalize(vtx_normal); // normal vector
    
    // Simple lighting with a light grey base color
    vec3 base_color = vec3(0.8, 0.8, 0.85);  // Light grey with slight blue tint
    
    // Light from upper right
    vec3 light_pos1 = vec3(3, 2, 3);
    vec3 l1 = normalize(light_pos1 - p);
    float diff1 = max(dot(n, l1), 0.0);
    
    // Light from upper left (softer)
    vec3 light_pos2 = vec3(-3, 2, 3);
    vec3 l2 = normalize(light_pos2 - p);
    float diff2 = max(dot(n, l2), 0.0);
    
    // View direction for specular
    vec3 v = normalize(e - p);
    
    // Specular highlight (subtle)
    vec3 r1 = reflect(-l1, n);
    float spec1 = pow(max(dot(r1, v), 0.0), 32.0);
    
    // Combine everything
    vec3 ambient = vec3(0.3) * base_color;
    vec3 diffuse = (diff1 * 0.6 + diff2 * 0.3) * base_color;
    vec3 specular = vec3(0.2) * spec1;
    
    vec3 color = ambient + diffuse + specular;
    
    frag_color = vec4(color, 1.0);
}