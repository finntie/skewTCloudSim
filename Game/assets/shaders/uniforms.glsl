layout (std140, binding = CAMERA_UBO_LOCATION) uniform CameraUBO
{
    mat4 bee_projection;              // 64
    mat4 bee_view;                    // 64
    mat4 bee_viewProjection;          // 64
    vec3 bee_eyePos;                  // 12
    float bee_time;                   // 4
    int bee_directionalLightsCount;   // 4
    int bee_pointLightsCount;         // 4  
    vec2 bee_resolution;              // 4
    vec4 bee_FogColor;                // 16
    float bee_FogFar;                 // 4
    float bee_FogNear;                // 4
};

struct directional_light_struct
{
    vec3    direction;   
    float   _dir_padding;               // 16
    vec3    color;
    float   intensity;                  // 16
    mat4    shadow_matrix;              // 64
};

#define MAX_DIRECTIONAL_LIGHTS 4

layout(std140, binding = DIRECTIONAL_LIGHTS_UBO_LOCATION) uniform DirectionalLightsUBO
{
    directional_light_struct bee_directional_lights[4];
};

#define MAX_POINT_LIGHT_INSTANCES 128

struct point_light_struct
{
    vec3    position;   
    float   range;      // 16
    vec3    color;
    float   intensity;  // 16
};

layout(std140, binding = LIGHTS_UBO_LOCATION) uniform PointLightsUBO
{
    point_light_struct bee_point_lights[MAX_POINT_LIGHT_INSTANCES];
};

#define MAX_TRANSFORM_INSTANCES 256

struct transform_struct
{    
    mat4    world;      // 64
    mat4    wvp;        // 64
};

layout(std140, binding = TRANSFORMS_UBO_LOCATION) uniform TransformsUBO
{
    transform_struct bee_transforms[MAX_TRANSFORM_INSTANCES];
};
