#version 330 core
layout (location = 0) in vec3 aColor;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
	gl_Position =  projection * view * model * vec4(aTexCoord.x,aColor.x,aTexCoord.y, 1.0f);
}