#version 460 core
out vec4 Out_Color;
in vec4 Frag_Color;

void main()
{
    Out_Color = Frag_Color;
}