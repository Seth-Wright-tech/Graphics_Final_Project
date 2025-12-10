#version 330 core

out vec4 FragColor;

in vec3 vtx_position;
in vec3 vtx_normal;

uniform vec3 cam_pos;


vec2 hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)),
             dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float perlin(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = dot(hash(i + vec2(0,0)), f - vec2(0,0));
    float b = dot(hash(i + vec2(1,0)), f - vec2(1,0));
    float c = dot(hash(i + vec2(0,1)), f - vec2(0,1));
    float d = dot(hash(i + vec2(1,1)), f - vec2(1,1));

    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p, int octaves) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < octaves; i++) {
        v += a * perlin(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float hash3D(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
}

vec3 rustColor(float t, float variation) {
    vec3 darkRust = vec3(0.18, 0.10, 0.06);
    vec3 brownRust = vec3(0.45, 0.22, 0.12);
    vec3 redRust = vec3(0.72, 0.25, 0.08);
    vec3 orangeRust = vec3(0.88, 0.45, 0.15);
    
    float t_varied = clamp(t + variation * 0.2, 0.0, 1.0);
    
    if (t_varied < 0.33) {
        return mix(darkRust, brownRust, t_varied * 3.0);
    } else if (t_varied < 0.66) {
        return mix(brownRust, redRust, (t_varied - 0.33) * 3.0);
    } else {
        return mix(redRust, orangeRust, (t_varied - 0.66) * 3.0);
    }
}

vec3 phongLighting(vec3 color, vec3 normal, vec3 world_pos, float roughness)
{
    vec3 n = normalize(normal);
    vec3 view_dir = normalize(cam_pos - world_pos);

    vec3 light_pos = vec3(3, 4, 3);
    vec3 light_color = vec3(1.0, 0.98, 0.95);

    vec3 L = normalize(light_pos - world_pos);
    float diff = max(dot(n, L), 0.0);

    vec3 R = reflect(-L, n);
    float shininess = mix(64.0, 8.0, roughness);
    float spec = pow(max(dot(view_dir, R), 0.0), shininess) * (1.0 - roughness * 0.8);

    vec3 light_pos2 = vec3(-2, 1, -2);
    vec3 L2 = normalize(light_pos2 - world_pos);
    float diff2 = max(dot(n, L2), 0.0) * 0.3;

    vec3 ambient = vec3(0.15, 0.16, 0.18) * color;
    vec3 diffuse = (diff * 0.8 + diff2) * color * light_color;
    vec3 specular = spec * vec3(0.6, 0.5, 0.4) * light_color;

    return ambient + diffuse + specular;
}

void main()
{
    vec3 p = vtx_position * 0.4;  

    float coarseNoise = fbm(p.xy * 0.5 + p.z * 0.2, 3);
    float mediumNoise = fbm(p.yz * 0.8, 4);
    float fineNoise = fbm(p.zx * 1.5, 3);
    
    float noise = coarseNoise * 0.7 + mediumNoise * 0.2 + fineNoise * 0.1;
    
    float variation = hash3D(floor(p * 2.0));
    noise += variation * 0.4;
    
    float edgeFactor = 1.0 - abs(dot(normalize(vtx_normal), normalize(cam_pos - vtx_position)));
    edgeFactor = pow(edgeFactor, 2.0);
    
    float verticalGrad = smoothstep(-1.0, 1.0, vtx_position.y) * 0.4;
    float rustMask = smoothstep(-0.5, 0.6, noise + edgeFactor * 0.2 - verticalGrad * 0.3);
    
    rustMask = pow(rustMask, 0.7);  
    rustMask = clamp(rustMask * .9, 0.0, 1.0);

    vec3 baseColor = vec3(0.08, 0.18, 0.52);  
    vec3 rust = rustColor(rustMask, variation);

    float blendFactor = smoothstep(0.2, 0.8, rustMask);
    vec3 finalSurface = mix(baseColor, rust, blendFactor * 0.95 + 0.05);
    
    float roughness = mix(0.2, 0.9, rustMask);
    
    vec3 lit = phongLighting(finalSurface, vtx_normal, vtx_position, roughness);
    
    lit = pow(lit, vec3(0.95)); 
    lit *= 1.05; 

    FragColor = vec4(lit, 1.0);
}