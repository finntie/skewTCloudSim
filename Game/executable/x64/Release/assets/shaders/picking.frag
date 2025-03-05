#version 460 core
#extension GL_ARB_explicit_uniform_location : enable

uniform uint gObjectIndex;
uniform uint gDrawIndex;

out uvec3 FragColor;

void main()
{
   FragColor = uvec3(gObjectIndex, gDrawIndex, gl_PrimitiveID);
}