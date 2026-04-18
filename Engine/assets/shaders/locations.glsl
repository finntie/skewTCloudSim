
// Atributes
#define POSITION_LOCATION       0
#define NORMAL_LOCATION         1
#define TEXTURE0_LOCATION       2
#define TEXTURE1_LOCATION       3
#define COLOR_LOCATION          4
#define TANGENT_LOCATION        5
#define LOCATION_COUNT          6

// UBOs
#define PER_FRAME_LOCATION                  1
#define PER_MATERIAL_LOCATION               2
#define PER_OBJECT_LOCATION                 3
#define CAMERA_UBO_LOCATION                 4
#define LIGHTS_UBO_LOCATION                 5
#define TRANSFORMS_UBO_LOCATION             6
#define DIRECTIONAL_LIGHTS_UBO_LOCATION     7
#define UBO_LOCATION_COUNT                  8

// G-buffer
#define GBUFFER_POSITION_LOCATION   0
#define GBUFFER_NORMAL_LOCATION     1
#define GBUFFER_ALBEDO_LOCATION     2
#define GBUFFER_ORM_LOCATION        3
#define GBUFFER_EMISSIVE_LOCATION   4
#define GBUFFER_DEPTH_LOCATION      5

// Samplers
#define BASE_COLOR_SAMPLER_LOCATION    0
#define NORMAL_SAMPLER_LOCATION        1
#define EMISSIVE_SAMPLER_LOCATION      2
#define ORM_SAMPLER_LOCATION           3
#define OCCLUSION_SAMPLER_LOCATION     4
#define DEPTH_SAMPLER_LOCATION         5
#define IRRADIANCE_LOCATION            6
#define LUT_SAMPER_LOCATION			   7
#define DIFFUSE_SAMPER_LOCATION		   8
#define SPECULAR_SAMPER_LOCATION	   9
#define SHADOWMAP_LOCATION			   10
 
// Attenuation
#define ATTENUATION_GLTF        0
#define ATTENUATION_BLAST       1
#define ATTENUATION_UNREAL      2
#define ATTENUATION_SMOOTHSTEP  3
