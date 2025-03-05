#version 460 core
#extension GL_ARB_explicit_uniform_location : require

#include "locations.glsl"
#include "uniforms.glsl"

layout (location = POSITION_LOCATION) in vec3 a_position;
layout (location = NORMAL_LOCATION) in vec3 a_normal;
layout (location = TEXTURE0_LOCATION) in vec2 a_texture;
layout (location = JOINTS0_LOCATION) in ivec4 boneIds; 
layout (location = WEIGHTS0_LOCATION) in vec4 weights;

out vec2 tex_coords;

const int MAX_BONES = 50;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool hasAnimation;

void main()
{
	vec4 totalPosition = vec4(a_position,1.0);

	if (hasAnimation)
	{
		totalPosition = vec4(0.0f);
		
		for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
		{
			if (boneIds[i] == -1) 
			{
				continue;
			}
			if (boneIds[i] >= MAX_BONES) 
			{
				totalPosition = vec4(a_position,1.0f);
				break;
			}
		
			mat4 jointTransform = finalBonesMatrices[boneIds[i]];
			
			totalPosition += jointTransform * vec4(a_position, 1.0f) * weights[i];			
		}
	}
	
    mat4 wvp = bee_transforms[gl_InstanceID].wvp;
    gl_Position = wvp * totalPosition;
	tex_coords = a_texture;
}