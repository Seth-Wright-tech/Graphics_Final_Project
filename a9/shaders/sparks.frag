#version 330 core
out vec4 fragColor;

void main() {
    float d = length(gl_PointCoord - vec2(0.5));
    float alpha = 1.0 - smoothstep(0.3, 0.5, d);
    fragColor = vec4(1.0, 0.6, 0.1, alpha); // orange spark
}
