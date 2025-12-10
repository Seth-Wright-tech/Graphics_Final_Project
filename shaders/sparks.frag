#version 330 core

/* Camera uniforms */
layout(std140) uniform camera
{
    mat4 projection;
    mat4 view;
    mat4 pvm;
    mat4 ortho;
    vec4 position;
};

/* Input from vertex shader */
in vec3 vtx_normal;
in vec3 vtx_position;

/* Output */
out vec4 frag_color;

void main()
{
    // Make it glow bright orange/yellow
    vec3 glow_color = vec3(1.0, 0.6, 0.1);
    
    // Add a rim light effect for extra glow
    vec3 view_dir = normalize(position.xyz - vtx_position);
    vec3 normal = normalize(vtx_normal);
    float rim = 1.0 - max(dot(view_dir, normal), 0.0);
    rim = pow(rim, 2.0);
    
    // Combine base glow with rim
    vec3 final_color = glow_color * (1.0 + rim * 2.0);
    
    frag_color = vec4(final_color, 1.0);
}
