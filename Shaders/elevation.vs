#version 330 core
layout (location = 0) in vec4 aPos;

out vec4 ourColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position =  projection * view * model * vec4(aPos.x,aPos.y,aPos.z, 1.0f);
    ourColor = vec4(aPos.y,0.0f,0.0f,aPos.w);
}