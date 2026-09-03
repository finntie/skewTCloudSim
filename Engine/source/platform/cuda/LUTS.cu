#pragma once
#include "platform/cuda/LUTS.cuh"

#include <cuda_runtime.h>
#include "math/cudaMath.cuh"

#include <iostream>

__constant__ float radiusEarth = 6360.0f;  // Radius of the earth in km
__constant__ float sampleHeight = 100.0f;  // Height of sampled medium (until the medium gets small enough that we dont care)
__constant__ float3x4 invViewLUT;

void setConstants(const float* _invView, size_t sizeViewMat)
{
    if (_invView) cudaMemcpyToSymbol(invViewLUT, _invView, sizeViewMat);
}

// position.y ranging from 0 to sampleHeight
__device__ float posToLUT(const float3& pos, float width)
{
    // With help from AI
    // Calculate distance to center, then calculate max and current disance to horizon
    float r = distance(make_float3(pos.x, pos.y + radiusEarth, pos.z));
    const float total = radiusEarth + sampleHeight;
    const float rMax = sqrtf(total * total - radiusEarth * radiusEarth);
    const float rPart = sqrtf(fmaxf(r * r - radiusEarth * radiusEarth, 0.0f));

    const float u = clampf(rPart / rMax, 0.0f, 1.0f);
    const float W = 1.0f / width;

    // Correct for 0.5f offset
    return u * (1.0f - W) + 0.5f * W;
}

// position.y ranging from 0 to sampleHeight
__device__ float dirToLUT(const float3& pos, const float3& dir, float height)
{
    // Calculate distance to center
    const float3 posC = make_float3(pos.x, pos.y + radiusEarth, pos.z);
    float r = distance(posC);
    const float mu = dot(dir, posC) / r;

    //Calculate max and current disance to horizon
    const float total = radiusEarth + sampleHeight;
    const float rMax = sqrtf(total * total - radiusEarth * radiusEarth);
    const float rPart = sqrtf(fmaxf(r * r - radiusEarth * radiusEarth, 0.0f));
    const float dMin = total - r;
    const float dMax = rPart + rMax;

    // Calculate distance using quick ABC formula
    const float disc = r * r * (mu * mu - 1.0f) + total * total;
    const float d = fmaxf(-r * mu + sqrtf(fmaxf(disc, 0.0f)), 0.0f);
    
    // Undo the lerp
    const float u = clampf((d - dMin) / fmaxf(dMax - dMin, 1e-6f), 0.0f, 1.0f);
    const float H = 1.0f / height;
    // Correct for 0.5f offset
    return u * (1.0f - H) + 0.5f * H;
}

__device__ float3 UVToDir(float u, float v, float3 up, float3 sunDir, float r) 
{ 
    // From CLAUDE AI Opus 5

    // We start by calculating the angle from our current radius (r) to the horizon:
    const float cosBeta = sqrt(fmaxf(r * r - radiusEarth * radiusEarth, 0.0f)) / r;
    const float beta = acosf(clampf(cosBeta, -1.0f, 1.0f));
    const float zenithHorizon = 3.1415926 - beta;

    float viewZenith = 0.0f;
    if (v < 0.5f) // Above horizon
    {
        // The reason of mapping and another mapping with c^2, is to put more precision closer to the horizon
        const float c = 1.0f - 2.0f * v;
        viewZenith = zenithHorizon * (1.0f - c * c);
    }
    else // Beneath Horizon 
    {
        // Now we map from zenithHor to zenithHor + beta (which is just Z to PI)
        const float c = 2.0f * v - 1.0f;
        viewZenith = zenithHorizon + beta * c * c;
    }

    const float lightViewCos = 1.0f - 2.0f * u * u;

    // Now we built the Up, Right and Forward vector:
    // Up being Up, Right being Side and Forward being SunTangent

    // We remove first the up part from the sun direction, so we get only the forward (or the azimuthal)
    float3 sunTangent = sunDir - up * dot(sunDir, up);
    float len = distance(sunTangent);
    // if the sun is almost directly above us, create any perpendicular vector from the up vector
    if (len < 1e-4f) sunTangent = normalize(make_float3(-up.y, up.x, 0.0f));
    else sunTangent = sunTangent / len;

    const float3 side = cross(up, sunTangent);

    const float cosZ = cosf(viewZenith);
    const float sinZ = sqrtf(fmaxf(1.0f - cosZ * cosZ, 0.0f)); // Cheaper than sinf()
    const float sinA = sqrtf(fmaxf(1.0f - lightViewCos * lightViewCos, 0.0f)); // Also cheaper than sinf()

    // Now that we have all angles, we construct the direction by converting Spherical coordinates to Cartasian coordinates:
    return up * cosZ + sinZ * (sunTangent * lightViewCos + side * sinA);

    // up * cosZ is just how far up the sun is
    // sinZ * () is the horizontal part
}

__device__ float2 dirToUV(float3 dir, float3 pos, float3 sunDir) 
{ 
    // Created from CLAUDE AI
    // Reverse of function above

    pos = pos + make_float3(0, radiusEarth, 0);

    const float r = distance(pos);
    const float3 up = pos / r;

    const float cosBeta = sqrt(fmaxf(r * r - radiusEarth * radiusEarth, 0.0f)) / r;
    const float beta = acosf(clampf(cosBeta, -1.0f, 1.0f));
    const float zenithHorizon = 3.1415926 - beta;

    // Get the zenith angle of the direction (vertical angle)
    const float viewZenith = acosf(clampf(dot(dir, up), -1.0f, 1.0f));

    float v = 0.0f;
    // Inverse of our little remapping trick
    if (viewZenith < zenithHorizon) v = 0.5f * (1.0f - sqrtf(fmaxf(1.0f - viewZenith / zenithHorizon, 0.0f)));
    else v = 0.5f * (1.0f - sqrtf(fmaxf((viewZenith - zenithHorizon) / beta, 0.0f)));

    // Remove up direction from the sun and normal direction
    float3 sunT = sunDir - up * dot(sunDir, up);
    float3 viewT = dir - up * dot(dir, up);
    const float ls = distance(sunT);
    const float lv = distance(viewT);
    // If any of the direction is almost fully up, we can not divide with it, so cos will be 1.0
    const float lightViewCos = (ls < 1e-4f || lv < 1e-4f) ? 1.0f : clampf(dot(sunT / ls, viewT / lv), -1.0f, 1.0f);
    
    // Map u from 0 to 1, and unmap the squaring we did
    const float u = sqrt(fmaxf(0.5f * (1.0f - lightViewCos), 0.0f));

    return make_float2(u, v);
}

// Pos.y ranging from 0 until sampleHeight, which is about 0 until 100
__device__ float3 envTransDir(const float3& pos, const float3& dir, unsigned long long TTexture)
{
    const float4 t = tex2D<float4>(TTexture, posToLUT(pos, 256), dirToLUT(pos, dir, 64));
    return expf3(make_float3(-t.x, -t.y, -t.z));
}

// Transmittance between 2 points
// Pos.y ranging from 0 until sampleHeight, which is about 0 until 100.
__device__ float3 envTrans(const float3& pos, const float3& pos2, unsigned long long TTexture)
{
    float3 a = pos;
    float3 b = pos2;

    const float3 dir = normalize(b - a);

    const float4 t1 = tex2D<float4>(TTexture, posToLUT(a, 256.0f), dirToLUT(a, dir, 64.0f));
    const float4 t2 = tex2D<float4>(TTexture, posToLUT(b, 256.0f), dirToLUT(b, dir, 64.0f));

    const float3 tau = make_float3(fmaxf(t1.x - t2.x, 0.0f), fmaxf(t1.y - t2.y, 0.0f), fmaxf(t1.z - t2.z, 0.0f));

    return expf3(tau * -1);
}

__device__ float3 envScat(const float3& pos, const float3& lightDir, unsigned long long TScat)
{
    // Height
    const float r = distance(make_float3(pos.x, pos.y + radiusEarth, pos.z)) - radiusEarth;
    const float v = clampf(r / sampleHeight, 0.0f, 1.0f);

    // Direction
    const float3 up = make_float3(0, 1, 0);
    const float sunD = dot(lightDir, up);
    const float u = (sunD + 1) * 0.5f;

    const float4 t = tex2D<float4>(TScat, u, v);
    return make_float3(t.x, t.y, t.z);
}

// Calculate phase function for Rayleigh Mie and Ozone, float3(Rayleigh, Mie, O3),
// Ozone contributes nothing, so will be 0
// Angle being incident between the viewDir and Sun and g the assymytry parameter (default = 0.8)
__device__ float3 envPhaseFunction(float angle, float g)
{
    float rayleigh = (3 * (1 + angle * angle)) / (16 * 3.1415926f);
    float mie = 3 / (8 * 3.1415926f) *
                (((1 - g * g) * (1 + angle * angle)) / ((2 + g * g) * powf(fmaxf(1 + g * g - 2 * g * angle, 1e-4f), 1.5f)));

    return make_float3(rayleigh, mie, 0.0f);
}

// Shadow term calculates shadow from the planet and from transmittance until space
// Position.y ranging from 0 to sampleHeight
__device__ float3 shadowTerm(const float3& pos, const float3& lightdir, unsigned long long TTexture)
{
    bool hitGround = false;
    getIpoint(pos, lightdir, hitGround);
    return int(!hitGround) * envTransDir(pos, lightdir, TTexture);
}

// Calculate how much radiance comes from the ground when the sun hits it
__device__ float3 groundRadiance(const float3& pos, const float3& lightdir, unsigned long long TTexture)
{
    const float sunAngle = dot(normalize(pos + make_float3(0, radiusEarth, 0)), lightdir);
    if (sunAngle < 0.0f) return make_float3(0, 0, 0);  // Sun beneath horizon
    const float g = 0.35f;  // Asuming low albedo for the ground, somewhere like grass

    return shadowTerm(pos, lightdir, TTexture) * (g / 3.14159265f) * sunAngle;
}

// Calculates the intersectionpoint of the ray, if it hit the ground or space, TODO: currently only does it 2D
__device__ float3 getIpoint(const float3& pos, const float3& dir, bool& hitGround)
{
    // Calculate intersection point with space or ground.
    // Using the ray formula: P = O + D * t
    // And the circle formula P^2 = r^2
    // With P being a 3D point in space and the circle center being at (0,0,0)
    // Using the quadratic formula with intersection of a ray and circle (noting that D*D = 1):
    // t^2 + t * (2(O*D)) + (O*O) - r^2 = 0
    // With which we get:  A = 1,  B = 2(O*D),  C = (O*O) - r^2

    const float3 O = make_float3(pos.x, pos.y + radiusEarth, pos.z);
    const float3 D = normalize(dir); // Making sure
    float R = radiusEarth;
    hitGround = false;

    // The ABC formula then goes: t = (-b +- sqrt(b^2 - 4ac)) / 2a
    // Yet, we can simplify due to a being 1 and b being 2 * dot(O, D)
    // Imagine b being dot(O,D) then we must put a 2 in front: 2b
    // t = (-2b +- sqrt(4b^2 - 4c)) / 2
    // Multiply sqrt by 2, being able to divide the values inside the sqrt by 4:
    // t = (-2b +- 2 * sqrt(b^2 - c)) / 2
    // Remove the division by multiplying:
    // t = -b +- sqrt(b^2 - c)

    const float b = dot(O, D); // Not multiplying by 2, since we divided this away

    const float r = distance(O);
    float h = r - R;
    float c = h * (2.0f * R + h); // Same as dot(O,O) - R * R, but uses smaller numbers, meaning no floating point issues

    float disc = b * b - c;

    // If the discriminant not negative, we hit the ground
    if (disc >= 0.0f)
    {
        const float t = -b - sqrt(disc);
        // If t is positive, we hit the ground forward, else we hit it behind us and we dont care
        if (t >= 0.0f)
        {
            hitGround = true;
            return O + D * t;
        }
    }

    // Instead of the ground, we will now look at intersection with space
    // This must hit, since the position must be inside the circle
    R = radiusEarth + sampleHeight;

    h = r - R;
    c = h * (2.0f * R + h);  // Same as dot(O,O) - R * R, but uses smaller numbers, meaning no floating point issues
    disc = b * b - c;
    if (disc < 0.0f) return make_float3(0, 0, 0);
    const float t = -b + sqrt(disc);

    float3 result = O + D * t;
    return result;



    //const float height = pos.y + radiusEarth;
    //const float a = dir.y / (dir.x + 1e-6f);
    //hitGround = false;
    //float2 iPoint = make_float2(0, 0);
    //{
    //    const float A = 1 + a * a;
    //    const float B = 2 * a * height;
    //    float C = height * height - radiusEarth * radiusEarth;
    //    const float inSQRT = B * B - 4 * A * C;
    //    if (inSQRT >= 0.0f)  // If greaer than 0, we intersect with ground and thus not with space
    //    {
    //        float x = (-B - sqrt(inSQRT) )/ (2 * A);  // Only using - in this quadratic formula, this is because we only need the
    //                                                // most left intersectionpoint.
    //        if (x / dir.x >= 0.0f) // Check if the line hit the ground forward and not behind us
    //        {
    //            float y = a * x + height;
    //            iPoint = make_float2(x, y);
    //            hitGround = true;
    //        }
    //    }
    //    if (!hitGround)  // If inside the sqrt is < 0, we do not intersect with the ground, so we only need to calculate space intersection
    //    {
    //        // C is different due to the radius not being only earth, but earth's radius + the sample height, giving radius of
    //        // space
    //        C = height * height - (radiusEarth + sampleHeight) * (radiusEarth + sampleHeight);
    //        float x = (-B + sqrt(B * B - 4 * A * C)) /
    //                           (2 * A);  // Only use of + (not -) since we know we need intersection at + direction of a)
    //        float y = a * x + height;
    //        iPoint = make_float2(x, y);
    //        hitGround = false;
    //    }
    //}
    //return make_float3(iPoint.x, iPoint.y, 0.0f);
}

__global__ void atmosphericTransmittance(float4* transmittance, int2 resolution)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int idx = x + y * resolution.x;
    if (x >= resolution.x || y >= resolution.y) return;

    // First create a position and direction from the x and y.
    // We do this by using different distances towards the horizon, this to put more resolution close to the horizon.

    // Calculate max distance to horizon
    const float rTotal = radiusEarth + sampleHeight;
    const float rMax = sqrt(rTotal * rTotal - radiusEarth * radiusEarth);

    const float u = (float(x) + 0.5f) / resolution.x;
    const float v = (float(y) + 0.5f) / resolution.y;

    // Part of distance that this thread will solve
    const float du = rMax * u;
    // Solving for long side of triangle to get current distance
    const float rCur = sqrt(du * du + radiusEarth * radiusEarth);

    // Min and max of the distances we can get
    const float dMin = rTotal - rCur;
    const float dMax = du + rMax;
    // Interpolate
    const float d = dMin + v * (dMax - dMin);
    // Now use the cosine law to solve for the angle
    const float mu = d <= 0.0f ? 1.0f : clampf((rMax * rMax - du * du - d * d) / (2.0f * rCur * d), -1.0f, 1.0f);

    const float sinZ = sqrtf(fmaxf(1.0f - mu * mu, 0.0f));
    float3 pos = make_float3(0, rCur - radiusEarth, 0);
    const float3 dir = make_float3(sinZ, mu, 0.0f);


    float3 tau = make_float3(0, 0, 0);

    //const float startHeight = sampleHeight / resolution.x * (float(x) + 0.5f);

    //// First map y from -1 to 1, this is our y direction, to get x direction we use pythagorean identity
    //const float angle = (float(y) + 0.5f) / resolution.y * 2.0f - 1.0f;
    //const float3 dir = make_float3(sqrtf(fmaxf(1.0f - angle * angle, 0.0f)), angle, 0.0f);
    //float3 pos = make_float3(0, startHeight, 0);

    bool hitGround = false;
    const float3 iPoint = getIpoint(pos, dir, hitGround) - make_float3(0, radiusEarth, 0);


    // components of scattering and absorption from rayleigh, mie and ozone in 1e-6 m (converted to 1 km):
    const float3 rs = make_float3(5.802f, 13.558f, 33.1f) * 1e-3f;
    const float ra = 0.0f;  // Rayleigh does almost not absorb light at all
    const float ms = 3.996f * 1e-3f;
    const float ma = 4.40f * 1e-3f;
    const float os = 0.0f;                                          // Ozone scattering is negatable
    const float3 oa = make_float3(0.650f, 1.881f, 0.085f) * 1e-3f;  // Note how ozone blue absorption is very low

    // Total steps we take
    const int stepCount = 40;

    // Calculate a stepsize based on max distance we will trace
    const float dist = distance(pos, iPoint);
    const float stepSize = d / stepCount;

    float t = 0;
    int steps = 0;

    // Trace through the circle (earth)
    while (steps < stepCount)
    {
        steps++;
        pos = pos + dir * stepSize * 0.5f;
        // Use medium height for our density calculation to be more in between
        // Addition of earth radius and removal due to working in earth space and not altitude space
        const float mediumHeight = distance(pos + make_float3(0, radiusEarth, 0)) - radiusEarth;
        pos = pos + dir * stepSize * 0.5f;
        t += stepSize;

        // Calculate density based on height
        const float dR = exp(-mediumHeight / 8.0f);
        const float dM = exp(-mediumHeight / 1.2f);
        const float dO = fmaxf(0, 1 - fabsf(mediumHeight - 25.0f) / 15.0f);

        // Add up all the densities
        tau = ((rs + ra) * dR + (ms + ma) * dM + (oa + os) * dO) * stepSize + tau;
    }

    //if (x == 0) printf("y %d: v %f d %f mu %f dist %f tau_b %f\n", y, v, d, mu, dist, tau.z);

    //if (x == 16)
    //{
    //    printf(
    //        "x %i, y %i, startHeight %f, sinZ %f, dir: %f, %f iPoint: %f, %f, hitGround: %i, dist %f, stepsize: %f, "
    //        "result: %f, %f, %f\n",x,y,startHeight,sinZ,dir.x,dir.y,iPoint.x,iPoint.y,int(hitGround),dist,stepSize,tau.x,tau.y,tau.z);
    //}

    transmittance[idx] = make_float4(tau.x, tau.y, tau.z, 0.0f);
}

// get a point on a sphere from 2 angles ranging 0 to 1
__device__ float3 sphereSamplePoint(float u1, float u2)
{
    const float cos = 1.0f - 2.0f * u1;
    const float sin = sqrt(fmaxf(1.0f - cos * cos, 0.0f));
    const float phi = 2.0f * 3.14159265f * u2;
    return make_float3(sin * cosf(phi), sin * sinf(phi), cos);
}

// Calculate second order scattering together with the transfer of energy that would occur from all of the atmospheric medium
__device__ float3 transferFunction(const float3& pos, const float3& lightdir, unsigned long long TTexture)
{
    const int samplePoints = 8;
    const float omegaChange = 1 / float(samplePoints * samplePoints);
    float3 secondOrderScat = make_float3(0, 0, 0);
    float3 allLightScat = make_float3(0, 0, 0);
    // Loop over points on a sphere
    for (int i = 0; i < samplePoints; i++)
    {
        for (int j = 0; j < samplePoints; j++)
        {
            float3 currentsecondOrderScat = make_float3(0, 0, 0);
            float3 currentLightScat = make_float3(0, 0, 0);

            // Get a point on a sphere
            const float u1 = (float(i) + 0.5f) / samplePoints;
            const float u2 = (float(j) + 0.5f) / samplePoints;
            const float3 omega3 = sphereSamplePoint(u1, u2);

            // Get intersectionpoint with ground or spae
            bool hitGround = false;
            const float3 iPoint = getIpoint(pos, omega3, hitGround) - make_float3(0,radiusEarth,0);



            // Total steps we take
            const int stepCount = 50;

            // Calculate a stepsize based on max distance we will trace
            float3 rayPos = pos;
            float3 prevPos = rayPos;
            const float dist = distance(rayPos, iPoint);
            const float stepSize = dist / stepCount;

            //if (threadIdx.y == 5)
            //{
            //    printf(
            //        "x %i, y %i, u1 %f, u2 %f, omega3 %f, %f, %f, hitground: %i, pos: %f, %f, %f ipoint: %f, %f, %f\n, dist %f",
            //        threadIdx.x,
            //        threadIdx.y,
            //        u1, u2, omega3.x, omega3.y, omega3.z, int(hitGround), pos.x, pos.y, pos.z, iPoint.x, iPoint.y, iPoint.z, dist);
            //}
            // components of scattering from rayleigh, mie and ozone in 1e-6 m (converted to 1 km):
            const float3 rs = make_float3(5.802f, 13.558f, 33.1f) * 1e-3f;
            const float ms = 3.996f * 1e-3f;
            const float os = 0.0f;                  // Ozone scattering is negatable
            const float pu = 1.0f / (4.0f * 3.1415926f);  // Isotropic phase function

            float t = 0;
            int steps = 0;

            // Trace through the circle (earth)
            while (steps < stepCount)
            {
                steps++;
                rayPos = rayPos + omega3 * stepSize * 0.5f;
                // Use medium height for our density calculation to be more in between
                // Addition of earth radius and removal due to working in earth space and not altitude space
                const float mediumHeight = distance(rayPos + make_float3(0,radiusEarth,0)) - radiusEarth; 
                rayPos = rayPos + omega3 * stepSize * 0.5f;
                t += stepSize;

                // Calculate density based on height
                const float dR = exp(-mediumHeight / 8.0f);
                const float dM = exp(-mediumHeight / 1.2f);

                // Scattering multiplied by transmittance of this distance
                const float3 scatteringTrans =
                    (rs * dR + ms * dM) * envTrans(pos, rayPos, TTexture);

                // Only need to account for distance
                currentLightScat = (scatteringTrans * stepSize) + currentLightScat;

                // Second order scattering needs more
                // TODO: multiple use of Transmittance with shadow term?
                currentsecondOrderScat =
                    (scatteringTrans * shadowTerm(rayPos, lightdir, TTexture) *
                     pu * 1.0f * stepSize) + currentsecondOrderScat;

                prevPos = rayPos;
            }

            // Add terms outside of tracing loop
            allLightScat = (currentLightScat * omegaChange) + allLightScat;

            float3 groundTerm = make_float3(0, 0, 0);
            if (hitGround)
            {
                groundTerm = envTrans(pos, iPoint, TTexture) *
                             groundRadiance(iPoint, lightdir, TTexture);
            }

            secondOrderScat = (groundTerm + currentsecondOrderScat) * omegaChange + secondOrderScat;

            //if (blockIdx.y * blockDim.y + threadIdx.y == 5)
            //{
            //    printf("x %i, y %i, i %i, j %i, iPoint %f %f %f, currentsecondOrderScat %f %f %f, allLightScatter %e %e %e, secondScatter %e %e %e\n",
            //        blockIdx.x * blockDim.x + threadIdx.x,
            //        blockIdx.y * blockDim.y + threadIdx.y,
            //        i,
            //        j,
            //        iPoint.x,
            //        iPoint.y,
            //        iPoint.z,
            //        currentsecondOrderScat.x,
            //        currentsecondOrderScat.y,
            //        currentsecondOrderScat.z,
            //        allLightScat.x,
            //        allLightScat.y,
            //        allLightScat.z,
            //        secondOrderScat.x,
            //        secondOrderScat.y,
            //        secondOrderScat.z);
            //}
        }
    }

    // going from only 1 scattering phase to an infinite amount of scattering:
    allLightScat = clamp3f(1 - allLightScat, 1e-3f, 1e3f);
    allLightScat = 1 / (allLightScat);



    // Now we calculate the total contribution of a direction light (sun) with infinite numbers of scattering.
    return secondOrderScat * allLightScat;
}

// Calculate all scattering from the atmosphere
__global__ void envScattering(float4* scattering, float2 resolution, unsigned long long TTexture)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int idx = x + y * resolution.x;

     // First create a position and direction from the x and y.
    // We do this by using different distances towards the horizon, this to put more resolution close to the horizon.

   
    const float u = (float(x) + 0.5f) / resolution.x;
    const float v = (float(y) + 0.5f) / resolution.y;

    const float r = sampleHeight * v;
    const float sunD = u * 2.0f - 1.0f; // Range from -1 to 1

    

    const float sinZ = sqrtf(fmaxf(1.0f - sunD * sunD, 0.0f));
    float3 pos = make_float3(0, r, 0);
    const float3 dir = normalize( make_float3(sinZ, sunD, 0.0f));


    float3 scatter = transferFunction(pos, dir, TTexture);

    //if (y == 0)
    //{
    //    printf(
    //        "x %i, y %i, pos %f, %f %f, dir: %f, %f, %f"
    //        "result: %f, %f, %f\n", x,y,pos.x,pos.y, pos.z,dir.x,dir.y, dir.z,scatter.x,scatter.y,scatter.z);
    //}

    scattering[idx] = make_float4(scatter.x, scatter.y, scatter.z, 0.0f);
}

// Calculate total luminance through a medium towards the view direction
// If only need to trace into the direction with no care about the 'to' position, just put in a very high value so that tracing
// until space/ground will be prefered
__device__ float3 luminanceFunction(const float3& from,
                                    const float3& to,
                                    const float3& dir,
                                    const float3& lightDir,
                                    unsigned long long TTexture,
                                    unsigned long long TScattering)
{
    // components of scattering from rayleigh, mie and ozone in 1e-6 m (converted to 1 km):
    const float3 rs = make_float3(5.802f, 13.558f, 33.1f) * 1e-3f;
    const float ms = 3.996f * 1e-3f;
    const float os = 0.0f;  // Ozone scattering is negatable
    const float3 phaseFuncs = envPhaseFunction(dot(dir, lightDir), 0.9f);

    // Get intersectionpoint with ground or spae
    bool hitGround = false;
    const float3 iPoint = getIpoint(from, dir, hitGround) - make_float3(0, radiusEarth, 0);

    // Total steps we take
    const int stepCount = 40;

    // Calculate a stepsize based on max distance we will trace
    float3 rayPos = from;
    float3 prevPos = rayPos;
    // Trace through the smallest distance
    float distIpoint = distance(from, iPoint);
    float distFromTo = distance(from, to);
    bool usingIPoint = distIpoint <= distFromTo;
    const float dist = usingIPoint ? distIpoint : distFromTo;
    const float stepSize = dist / stepCount;


    //int x = blockIdx.x * blockDim.x + threadIdx.x;
    //int y = blockIdx.y * blockDim.y + threadIdx.y;
    //if (x == 25)
    //{
    //    printf("x %i, y %i, iPoint %f %f %f, rayPos %f %f %f,   from %f %f %f, dir %f %f %f,dist %f, hitground %i\n",
    //           x, y, iPoint.x,iPoint.y,iPoint.z,rayPos.x,rayPos.y,rayPos.z,from.x,from.y,from.z,dir.x,dir.y,dir.z,dist, int(hitGround));
    //}

    float t = 0;
    int steps = 0;

    float3 scattering = make_float3(0, 0, 0);

    // Trace through the circle (earth)
    while (steps < stepCount)
    {
        steps++;
        rayPos = rayPos + dir * stepSize * 0.5f;
        // Use medium height for our density calculation to be more in between
        // Addition of earth radius and removal due to working in earth space and not altitude space
        const float mediumHeight = distance(rayPos + make_float3(0, radiusEarth, 0)) - radiusEarth;
        rayPos = rayPos + dir * stepSize * 0.5f;
        t += stepSize;

        // Calculate density based on height
        const float dR = exp(-mediumHeight / 8.0f);
        const float dM = exp(-mediumHeight / 1.2f);

        // Scattering with density
        float3 scatterDens = (rs * dR + ms * dM);
        float3 phaseScatter = rs * dR * phaseFuncs.x + ms * dM * phaseFuncs.y;


        const float3 envT = envTrans(from, rayPos, TTexture);
        const float3 shadowT = shadowTerm(rayPos, lightDir, TTexture);
        const float3 envS = envScat(rayPos, lightDir, TScattering);


        scattering = envT * (shadowT * phaseScatter + envS * scatterDens * 1.0f) * stepSize + scattering;


        // int x = blockIdx.x * blockDim.x + threadIdx.x;
        // int y = blockIdx.y * blockDim.y + threadIdx.y;
        // if (x == 25 && y == 0)//int(float(blockDim.y * gridDim.y) / 2.0f))
        //{
        //     printf("x %i, y %i, rayPos %f %f %f, scattering %f %f %f, dist %f, dR %f, dM %f, phaseFuncs %e, %e, scatterDens: %e, %e, %e\n",
        //        x,
        //        y,
        //        rayPos.x,
        //        rayPos.y,
        //        rayPos.z,
        //        scattering.x,
        //        scattering.y,
        //        scattering.z,
        //        dist,
        //        dR,
        //        dM,
        //        phaseFuncs.x,
        //        phaseFuncs.y,
        //        scatterDens.x,
        //        scatterDens.y,
        //        scatterDens.z);
        // }

        //int x = blockIdx.x * blockDim.x + threadIdx.x;
        //int y = blockIdx.y * blockDim.y + threadIdx.y;
        //if (x == 25 && y == 0)//int(float(blockDim.y * gridDim.y) / 2.0f))
        //{
        //    printf("x %i, y %i, rayPos %f %f %f, scattering %f %f %f, dist %f, dR %f, dM %f, phaseFuncs %e, %e, envT: %f, %f, %f, shadowT: %f, %f, %f envS: %f, %f, %f\n",
        //           x,y,rayPos.x,rayPos.y,rayPos.z,scattering.x,scattering.y,scattering.z,dist,dR,dM,phaseFuncs.x,phaseFuncs.y,envT.x,envT.y,envT.z,shadowT.x,shadowT.y,shadowT.z,envS.x,envS.y,envS.z);
        //}

        prevPos = rayPos;
    }


    //int x = blockIdx.x * blockDim.x + threadIdx.x;
    //int y = blockIdx.y * blockDim.y + threadIdx.y;
    //if (x == 5 && y >= 35 && y <= 65)
    //{
    //    printf(
    //        "x %i, y %i, iPoint %f %f %f, scattering %f %f %f, dist %f\n",
    //        x,y, iPoint.x, iPoint.y,iPoint.z,scattering.x,scattering.y,scattering.z,dist);
    //}

    const float3 pos2 = usingIPoint ? iPoint : to;
    float3 groundTerm = make_float3(0, 0, 0);
    if (hitGround && usingIPoint)
    {
        groundTerm = envTrans(from, pos2, TTexture) * groundRadiance(pos2, lightDir, TTexture);
    }
    else
    {
        //scattering = make_float3(0, 0, 0);

    }

    float3 result = groundTerm + scattering;

    //int x = blockIdx.x * blockDim.x + threadIdx.x;
    //int y = blockIdx.y * blockDim.y + threadIdx.y;
    //if (x == 5 && y >= 45 && y <= 55)
    //{
    //    printf("x %i, y %i, iPoint %f %f %f, result %f %f %f, result %f %f %f,groundTerm %f %f %f, dist %f, hitGround %i\n",
    //           x,
    //           y,
    //           iPoint.x,
    //           iPoint.y,
    //           iPoint.z,
    //           result.x,
    //           result.y,
    //           result.z,
    //           scattering.x,
    //           scattering.y,
    //           scattering.z,
    //           groundTerm.x,
    //           groundTerm.y,
    //           groundTerm.z,
    //           dist, int(hitGround));
    //}

    return result;
}

__global__ void envSkyView(float4* skyView,
                           const float3 lightDir,
                           const int2 resolution,
                           unsigned long long TTexture,
                           unsigned long long TScat)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int idx = x + y * resolution.x;

    if (x >= resolution.x || y >= resolution.y) return;

    
    // Set UV, ranging between 0 and 1
    float u = (float(x) + 0.5f) / float(resolution.x);
    float v = (float(y) + 0.5f) / float(resolution.y);

    float4 pos = mul(invViewLUT, make_float4(0.0f, 0.0f, 0.0f, 1.0f));

    // Put our coordinates to km
    pos = pos / 1000.0f;

    const float3 posC = make_float3(pos.x, pos.y + radiusEarth, pos.z);
    const float r = distance(posC);
    const float3 up = posC / r;
    float3 rayD = UVToDir(u, v, up, lightDir, r);

    //if (x == 5)
    //{
    //    printf("x %i, y %i, pos %f, %f, %f, up %f %f %f, rayDir: %f, %f, %f\n",
    //           x,y,pos.x,pos.y,pos.z,up.x,up.y,up.z,rayD.x,rayD.y,rayD.z);
    //}

    float3 luminance =
        luminanceFunction(make_float3(pos.x, pos.y, pos.z), make_float3(1e16f, 1e16f, 1e16f), rayD, lightDir, TTexture, TScat);

    // Shade properly
    const float exposure = 7.5f;
    float3 result = luminance * exposure;
    result = result / (result + 1);
    result = make_float3(powf(result.x, 1 / 2.2f), powf(result.y, 1 / 2.2f), powf(result.z, 1 / 2.2f));

    skyView[idx] = make_float4(result.x, result.y, result.z, 0.0f);
}

// 3D texture, the third dimension covering multiple depths
__global__ void envAerialView(float4* aerialView,
                              const float2 screenSize,
                              const float3 lightDir,
                              const int3 resolution,
                              unsigned long long TTexture,
                              unsigned long long TScat)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    const int z = blockIdx.z * blockDim.z + threadIdx.z;
    const int idx = x + y * resolution.x + z * resolution.x * resolution.y;

    if (x >= resolution.x || y >= resolution.y || z >= resolution.z) return;

    // Calculation of fov and how this would effect the focalLength
    // With focalLength being the distance from the camera origin to the virtual image plane
    // The further away the focalLenght is, the more zoomed in the image
    float aspectRatio = float(screenSize.x) / float(screenSize.y);
    float fov = 60.0f;
    float focalLength = 1.0f / tanf(fov * 0.5f * 3.14159f / 180.0f);

    // Set UV, ranging between -1 and 1
    float u = ((float(x) / float(screenSize.x)) * 2.0f - 1.0f) * aspectRatio;
    float v = ((float(y) / float(screenSize.y)) * 2.0f - 1.0f);

    // Create ray and set origin + direction
    float4 Otemp = mul(invViewLUT, make_float4(0.0f, 0.0f, 0.0f, 1.0f));
    float3 rayO = make_float3(Otemp.x, Otemp.y, Otemp.z);
    float3 rayD = mul(invViewLUT, normalize(make_float3(u, v, -focalLength)));

    // For every z, move 1 km further
    float3 nextO = rayO + rayD * z * 1000.0f;

    float3 scattering = luminanceFunction(rayO, nextO, rayD, lightDir, TTexture, TScat);
    float3 transmittance = envTrans(rayO, nextO, TTexture);
    // We store transmittance approximation as a 1D value, so we take the mean value
    float transMean = (transmittance.x + transmittance.y + transmittance.z) / 3.0f;

    aerialView[idx] = make_float4(scattering.x, scattering.y, scattering.z, transMean);
}

__device__ float4 getAtmosphericSkyView(float3 dir, float3 pos, float3 lightDir, float2 resolution, unsigned long long skyViewTexture)
{
    float u = (resolution.x + 1.0f) * 0.5f;
    float v = (resolution.y + 1.0f) * 0.5f;

    pos = pos / 1000.0f; // Make sure 1000.0f coordinate is 1 km and not 1000 km

    float2 uv = dirToUV(dir, pos, lightDir);

    
     //float3 value = envTransDir(pos, dir,256, 64, skyViewTexture) ;

    // Unmap texel center correction
    const float s = uv.x * (1.0f - 1.0f / resolution.x) + 0.5f / resolution.x;
    const float t = uv.y * (1.0f - 1.0f / resolution.y) + 0.5f / resolution.y;

    //if (blockIdx.x * blockDim.x + threadIdx.x == 100 && blockIdx.y * blockDim.y + threadIdx.y == 0)
    //{
    //    printf("resolution: %f, %f, pos x %f, y %f, z %f lightDir: %f, %f, %f trans %f, %f, %f\n",
    //           pos.y / sampleHeight,
    //           resolution.y,
    //           pos.x,
    //           pos.y,
    //           pos.z,
    //           lightDir.x,
    //           lightDir.y,
    //           lightDir.z,
    //           trans.x,
    //           trans.y,
    //           trans.z);
    //}

    //const float3 value = envScat(pos, lightDir, resolution.x, resolution.y, skyViewTexture);
    //return make_float4(value.x, value.y, value.z, 0.0f);

    // Thats all    
    return tex2D<float4>(skyViewTexture, s, t);
}
