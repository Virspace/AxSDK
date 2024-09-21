#version 460 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

out vec4 Frag_Color;

uniform mat4 ProjMtx;

void main()
{
    gl_Position = ProjMtx * vec4(Position, 1.0);
    Frag_Color = Color;
}