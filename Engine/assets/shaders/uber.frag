#version 460 core
#extension GL_GOOGLE_include_directive : require
#define PI 3.14159265359 

#include "locations.glsl"
#include "uniforms.glsl"

in vec3 v_position;
in vec3 v_normal;
in vec3 v_tangent;
in vec2 v_texture0;
in vec2 v_texture1;

layout(location = BASE_COLOR_SAMPLER_LOCATION) uniform sampler2D s_base_color;
layout(location = NORMAL_SAMPLER_LOCATION)     uniform sampler2D s_normal;
layout(location = EMISSIVE_SAMPLER_LOCATION)   uniform sampler2D s_emissive;
layout(location = ORM_SAMPLER_LOCATION)        uniform sampler2D s_orm;
layout(location = OCCLUSION_SAMPLER_LOCATION)  uniform sampler2D s_occulsion;
layout(location = LUT_SAMPER_LOCATION)         uniform sampler2D s_ibl_lut;
layout(location = DIFFUSE_SAMPER_LOCATION)     uniform samplerCube s_ibl_diffuse;
layout(location = SPECULAR_SAMPER_LOCATION)    uniform samplerCube s_ibl_specular;
layout(location = SHADOWMAP_LOCATION)          uniform sampler2DShadow[4] s_shadowmaps;

out vec4 frag_color;

uniform bool is_unlit;
uniform vec2 u_resolution;
uniform bool u_recieve_shadows;
uniform int u_ibl_specular_mip_count;

uniform bool use_base_texture;
uniform bool use_metallic_roughness_texture;
uniform bool use_emissive_texture;
uniform bool use_normal_texture;
uniform bool use_occlusion_texture;
uniform bool use_alpha_blending;

uniform vec4 base_color_factor;
uniform float metallic_factor;
uniform float roughness_factor;

uniform bool debug_base_color;
uniform bool debug_normals;
uniform bool debug_normal_map;
uniform bool debug_metallic;
uniform bool debug_roughness;
uniform bool debug_emissive;
uniform bool debug_occlusion;

const float c_zbias = 0.003;
const float c_gamma = 2.2;
const float c_inv_gamma = 1.0 / c_gamma;
const float u_EnvIntensity = 1.0;
const float c_point_light_tweak = 1.0 / 300.0;
const float c_dir_light_tweak = 1.0 / 200.0;

#define saturate(value) clamp(value, 0.0, 1.0)
#define DEBUG 1

struct fragment_light
{
    vec3    direction;   
    float   attenuation;
    vec4    color_intensity;
};

struct fragment_material
{
    vec4 albedo;
    vec4 emissive;
    float roughness;
    float metallic;
    float occlusuion;
    vec3 f0;
    vec3 f90;
    vec3 diffuse;
};

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 linear_to_sRGB(vec3 color)
{
    return pow(color, vec3(c_inv_gamma));
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);    
}

vec3 F_Schlick(vec3 f0, vec3 f90, float VoH) {
    return f0 + (f90 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float G_Smith(float NoV, float NoL, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GGXL = NoL / (NoL * (1.0 - k) + k);
	float GGXV = NoV / (NoV * (1.0 - k) + k);
	return GGXL * GGXV;
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NoV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NoV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NoV * NoV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NoV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (PI * f * f);
}


vec3 BRDF_Lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float VoH) {
    return (1.0 - F_Schlick(f0, f90, VoH)) * (diffuseColor / PI);
}


vec3 BRDF_SpecularGGX(vec3 f0, vec3 f90, float lin_roughness, float VdotH, float NdotL, float NoV, float NdotH) {
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NoV, lin_roughness);
    float D = D_GGX(NdotH, lin_roughness);
    return F * Vis * D;
}

void apply_light(
    in fragment_light light,
    in fragment_material mt,
    in vec3 position,
    in vec3 normal,
    out vec3 diffuse,
    out vec3 specular)
{ 
    vec3 L = light.direction;                   // Light
    vec3 V = normalize(bee_eyePos - position);  // View
    vec3 N = normal;                            // Normal
    vec3 H = normalize(V + light.direction);    // Half Vector

    float NoV = saturate(dot(N, V));
    float NoL = saturate(dot(N, L));
    float NoH = saturate(dot(N, H));
    float LoH = saturate(dot(L, H));
    float VoH = saturate(dot(V, H));

    vec3 intensity = light.color_intensity.rgb * light.color_intensity.a;
    intensity *= light.attenuation;
    specular += intensity * NoL * BRDF_SpecularGGX(mt.f0, mt.f90, mt.roughness, VoH, NoL, NoV, NoH);
    diffuse += intensity * NoL * BRDF_Lambertian(mt.f0,  mt.f90, mt.diffuse, VoH);
}

vec3 get_diffuse_light_ibl(vec3 n)
{
    return texture(s_ibl_diffuse, n).rgb * u_EnvIntensity; // TODO: Add this
}

vec4 get_specular_sample_ibl(vec3 reflection, float lod)
{
    return textureLod(s_ibl_specular, reflection, lod) * u_EnvIntensity; // TODO: Add this
}

vec3 getIBLRadianceLambertian(vec3 n, vec3 v, float roughness, vec3 diffuseColor, vec3 F0)
{
    float NoV = saturate(dot(n, v));
    vec2 brdfSamplePoint = clamp(vec2(NoV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(s_ibl_lut, brdfSamplePoint).rg;

    vec3 irradiance = get_diffuse_light_ibl(n);

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera

    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NoV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y; // <--- GGX / specular light contribution

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}

vec3 getIBLRadianceGGX(vec3 n, vec3 v, float roughness, vec3 F0)
{
    float NoV = saturate(dot(n, v));
    float lod = roughness * float(u_ibl_specular_mip_count - 1);
    vec3 reflection = normalize(reflect(-v, n));

    vec2 brdfSamplePoint = clamp(vec2(NoV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(s_ibl_lut, brdfSamplePoint).rg;
    vec4 specularSample = get_specular_sample_ibl(reflection, lod);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    // Roughness dependent fresnel, from Fdez-Aguera
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NoV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;
    return specularLight * FssEss;
}

float attenuation(float distance, float range)
{
    // GLTF 2.0 uses quadratic attenuation, but with range
    float distance2 = distance * distance;    
    return max(min(1.0 - pow(distance/range, 4), 1), 0) / distance2;
}

void apply_fog(inout vec3 pixel_color, in vec3 fog_color, float fog_near, float fog_far, float distance)
{
    float fog_amount = saturate((distance - fog_near) / (fog_far - fog_near));
    pixel_color = mix(pixel_color, fog_color, fog_amount);
}

void main()
{       
    // Collect all the properties in this struct (like g-buffer) 
    fragment_material mat;

    if(use_base_texture)
        mat.albedo = pow(texture(s_base_color, v_texture0), vec4(2.2));
    else
        mat.albedo = vec4(1.0, 1.0, 1.0, 1.0);
    mat.albedo *= base_color_factor;

    // Alpha clipping (unless alpha blending is enabled)
    if(!use_alpha_blending && mat.albedo.a < 0.2f) // TODO: Bring this from material
        discard;
        
    if(is_unlit)
    {
        frag_color = vec4(linear_to_sRGB(mat.albedo.rgb), mat.albedo.a);
        return;
    }

    mat.emissive = vec4(0.0);
    if(use_emissive_texture)
    {
        mat.emissive = pow(texture(s_emissive, v_texture0), vec4(2.2));
    }

    if(use_metallic_roughness_texture)
    {
        vec4 tex_orm = texture(s_orm, v_texture0);
        mat.occlusuion = tex_orm.r;
        mat.roughness = tex_orm.g * roughness_factor;        
        mat.metallic = tex_orm.b * metallic_factor;
    }
    else
    {
        mat.occlusuion = 1.0;
        mat.roughness = roughness_factor;
        mat.metallic = metallic_factor;
    }

    if(use_occlusion_texture)
    {
        float tex_occlusuion = texture(s_occulsion, v_texture0).r;
        mat.occlusuion = tex_occlusuion;
    }

    vec3 normal = normalize(v_normal);
    if(use_normal_texture)
    {
        vec3 bi_tangent = normalize(cross(v_normal, v_tangent));
        mat3 TBN = mat3 (v_tangent, bi_tangent, v_normal);
        normal = texture(s_normal, v_texture0).rgb;
        normal = normal * 2.0 - 1.0;
        normal = normalize(TBN * normal);
    }

    // Go from perceptual to linear(ish) roughness. 
    mat.f0 = mix(vec3(0.04), mat.albedo.rgb, mat.metallic);
    mat.diffuse = mix(mat.albedo.rgb,  vec3(0), mat.metallic);
    mat.f90 = vec3(1.0);    // all materials are reflective at grazing angles
    float reflectance = max(max(mat.f0.r, mat.f0.g), mat.f0.b);

    vec3 V = bee_eyePos - v_position;
    float distance = length(V);
    V = normalize(V);                               // View
    vec3 N = normal;                                // Normal

    vec3 specular = vec3(0.0);
    vec3 diffuse = vec3(0.0);

    specular += getIBLRadianceGGX(N, V, mat.roughness, mat.f0);
    diffuse += getIBLRadianceLambertian(N, V, mat.roughness, mat.diffuse, mat.f0);

    // To alpha roughness
    mat.roughness = mat.roughness * mat.roughness;

    for(int i = 0; i < bee_directionalLightsCount; i++) // Dir lights
    {        
        fragment_light light;
        light.direction = bee_directional_lights[i].direction;
        light.color_intensity = vec4(   bee_directional_lights[i].color,
                                        bee_directional_lights[i].intensity);
        light.attenuation = c_dir_light_tweak;
        vec3 dif = vec3(0.0);
        vec3 spc = vec3(0.0);
        apply_light(light, mat, v_position, normal, dif, spc);
        
        if(u_recieve_shadows)
        {
            vec4 pos_light_coor = bee_directional_lights[i].shadow_matrix * vec4(v_position, 1.0);
            vec3 proj_coords = pos_light_coor.xyz / pos_light_coor.w;
            proj_coords = proj_coords * 0.5 + 0.5;
            proj_coords.z -= c_zbias;        
            float shadow = texture(s_shadowmaps[i], proj_coords);
            dif *= shadow;
            spc *= shadow;
        }

        diffuse += dif;
        specular += spc;
    }

    for(int i = 0; i < bee_pointLightsCount; i++) // Point lights
    {        
        fragment_light light;        
        light.direction = bee_point_lights[i].position - v_position;
        float distance = length(light.direction);
        light.direction /= distance;
        light.color_intensity = vec4(   bee_point_lights[i].color,
                                        bee_point_lights[i].intensity);
        light.attenuation = attenuation(distance, bee_point_lights[i].range);
        light.attenuation *= c_point_light_tweak;
        vec3 dif = vec3(0.0);
        vec3 spc = vec3(0.0);
        apply_light(light, mat, v_position, normal, dif, spc);        
        diffuse += dif;
        specular += spc;
    }

    vec3 color = (specular + diffuse) /* * mat.occlusuion */ + mat.emissive.rgb;
    color = linear_to_sRGB(color);
    frag_color = vec4(color, 1.0);

    if(bee_FogColor.w > 0.0)
        apply_fog(frag_color.rgb, bee_FogColor.rgb, bee_FogNear, bee_FogFar, distance); 

#if DEBUG   
    if(debug_base_color)
        frag_color = vec4(mat.albedo.rgb, 1.0);
    if(debug_normals)
        frag_color = vec4(normal, 1.0);
    if(debug_normal_map && use_normal_texture)
        frag_color = texture(s_normal, v_texture0);    
    if(debug_metallic)
        frag_color = vec4(vec3(mat.metallic), 1.0);
    if(debug_roughness)
        frag_color = vec4(vec3(mat.roughness), 1.0);
    if(debug_metallic && debug_roughness)
        frag_color = vec4(0.0, mat.roughness, mat.metallic, 1.0);
    if(debug_occlusion)
        frag_color = vec4(vec3(mat.occlusuion), 1.0);
    if(debug_emissive)
        frag_color = vec4(vec3(mat.emissive), 1.0);
#endif
}