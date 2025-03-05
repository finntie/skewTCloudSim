#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

layout (location = POSITION_LOCATION) in vec3 a_position;
layout (location = NORMAL_LOCATION) in vec3 a_normal;
layout (location = TANGENT_LOCATION) in vec4 a_tangent;
layout (location = TEXTURE0_LOCATION) in vec2 a_texture0;
layout (location = TEXTURE1_LOCATION) in vec2 a_texture1;
layout (location = JOINTS0_LOCATION) in ivec4 boneIds; 
layout (location = WEIGHTS0_LOCATION) in vec4 weights;

const int MAX_BONES = 50;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool hasAnimation;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_tangent;
out vec2 v_texture0;	
out vec2 v_texture1;

void main()
{   
	vec4 totalPosition = vec4(a_position,1.0f);

	
    mat4 world = bee_transforms[gl_InstanceID].world;
    mat4 wv = bee_view * world;
    v_position = (world * totalPosition).xyz;
    v_normal = normalize((world * vec4(a_normal, 0.0)).xyz);
    v_tangent = normalize((world * vec4(a_tangent.xyz, 0.0)).xyz);
    v_texture0 = a_texture0;
    v_texture1 = a_texture1;
	mat4 wvp = bee_transforms[gl_InstanceID].wvp;

	if (hasAnimation)
	{
		totalPosition = vec4(0.0f);
		vec4 totalNormal = vec4(0.0f);
		vec4 totalTangent = vec4(0.0f);
		
		for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
		{
			if (boneIds[i] == -1) 
			{
				continue;
			}
			if (boneIds[i] >= MAX_BONES) 
			{
				totalPosition = vec4(a_position,1.0f);
				totalNormal = vec4(a_normal, 0.0f);
                totalTangent = vec4(a_tangent.xyz, 0.0f);
				break;
			}
		
			mat4 jointTransform = finalBonesMatrices[boneIds[i]];
			
			totalPosition += jointTransform * vec4(a_position, 1.0f) * weights[i];
			
			totalNormal += jointTransform * vec4(a_normal, 0.0f) * weights[i];
			
			totalTangent += jointTransform * vec4(a_tangent.xyz, 0.0f) * weights[i];
		}
		
		v_position = (world * totalPosition).xyz;
		v_normal = normalize((world * vec4(totalNormal.xyz, 0.0)).xyz);
		v_tangent = normalize((world * vec4(totalTangent.xyz, 0.0)).xyz);
	}
    gl_Position = wvp * totalPosition;
}