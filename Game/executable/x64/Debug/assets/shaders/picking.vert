#version 460 core
#extension GL_ARB_explicit_uniform_location : enable

layout (location = 0) in vec3 Position;                                             
                                                                                    
uniform mat4 gWVP;                                                                  
                                                                                    
void main()                                                                         
{                                                                                   
    gl_Position = gWVP * vec4(Position, 1.0);                                       
}