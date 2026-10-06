#pragma once
#include <iostream>

struct gridDataSky // 96 bytes
{
    // Initialize data with size in number of items
    void init(const int size)
    {
        Qv = new float[size];
        Qw = new float[size];
        Qc = new float[size];
        Qr = new float[size];
        Qs = new float[size];
        Qi = new float[size];
        potTemp = new float[size];
        velFieldX = new float[size];
        velFieldY = new float[size];
        velFieldZ = new float[size];
        pressure = new float[size];
        initialized = true;
        m_size = size;
    }
    float* Qv;         // Mixing Ratio of Water Vapor
    float* Qw;         // Mixing Ratio of Liquid Water
    float* Qc;         // Mixing Ratio of Ice
    float* Qr;         // Mixing Ratio of Rain
    float* Qs;         // Mixing Ratio of Snow
    float* Qi;         // Mixing Ratio of Ice (precip)
    float* potTemp;    // Potential temperature
    float* velFieldX;  // Velocity field X  (fluid sim)
    float* velFieldY;  // Velocity field Y  (fluid sim)
    float* velFieldZ;  // Velocity field Z  (fluid sim)
    float* pressure;   // Pressure in hPa

    void reset()
    {
        std::memset(Qv, 0, m_size);
        std::memset(Qw, 0, m_size);
        std::memset(Qc, 0, m_size);
        std::memset(Qr, 0, m_size);
        std::memset(Qs, 0, m_size);
        std::memset(Qi, 0, m_size);
        std::memset(potTemp, 0, m_size);
        std::memset(velFieldX, 0, m_size);
        std::memset(velFieldY, 0, m_size);
        std::memset(velFieldZ, 0, m_size);
        std::memset(pressure, 0, m_size);
    }

    gridDataSky() = default;
    ~gridDataSky()
    {
        if (initialized)
        {
            delete[] Qv;
            delete[] Qw;
            delete[] Qc;
            delete[] Qr;
            delete[] Qs;
            delete[] Qi;
            delete[] potTemp;
            delete[] velFieldX;
            delete[] velFieldY;
            delete[] velFieldZ;
            delete[] pressure;
            initialized = false;
        }
    }

    // Delete copy
    gridDataSky(const gridDataSky&) = delete;
    gridDataSky& operator=(const gridDataSky&) = delete;

    // Move constructor
    gridDataSky(gridDataSky&& other) noexcept
    {
        Qv = other.Qv;
        Qw = other.Qw;
        Qc = other.Qc;
        Qr = other.Qr;
        Qs = other.Qs;
        Qi = other.Qi;
        potTemp = other.potTemp;
        velFieldX = other.velFieldX;
        velFieldY = other.velFieldY;
        velFieldZ = other.velFieldZ;
        pressure = other.pressure;
        initialized = other.initialized;
        m_size = other.m_size;
        other.initialized = false;  // Since other is now empty
    }
    // Equal will now move:
    gridDataSky& operator=(gridDataSky&& other) noexcept
    {
        if (this != &other)
        {
            if (initialized)  // If current is already initialized, we need to free our data
            {
                delete[] Qv;
                delete[] Qw;
                delete[] Qc;
                delete[] Qr;
                delete[] Qs;
                delete[] Qi;
                delete[] potTemp;
                delete[] velFieldX;
                delete[] velFieldY;
                delete[] velFieldZ;
                delete[] pressure;
            }
            Qv = other.Qv;
            Qw = other.Qw;
            Qc = other.Qc;
            Qr = other.Qr;
            Qs = other.Qs;
            Qi = other.Qi;
            potTemp = other.potTemp;
            velFieldX = other.velFieldX;
            velFieldY = other.velFieldY;
            velFieldZ = other.velFieldZ;
            pressure = other.pressure;
            initialized = other.initialized;
            m_size = other.m_size;
            other.initialized = false;  // Since other is now empty
        }
        return *this;
    }

private:
    bool initialized = false;
    int m_size = 0;
};

struct gridDataGround // 64 bytes
{
    // Initialize data with size in number of items
    void init(const int size)
    {
        Qrs = new float[size];
        Qgr = new float[size];
        Qgs = new float[size];
        Qgi = new float[size];
        P = new float[size];
        t = new float[size];
        T = new float[size];
        initialized = true;
    }

    float* Qrs;  // Subsurface water content
    float* Qgr;  // Rain content
    float* Qgs;  // Snow content
    float* Qgi;  // Ice content
    float* P;    // Ground Pressure
    float* t;    // Time since ground was wet
    float* T;    // Ground temperature

    gridDataGround() = default;
    ~gridDataGround()
    {
        if (initialized)
        {
            delete[] Qrs;
            delete[] Qgr;
            delete[] Qgs;
            delete[] Qgi;
            delete[] P;
            delete[] t;
            delete[] T;
        }
    }

    // Delete copy
    gridDataGround(const gridDataGround&) = delete;
    gridDataGround& operator=(const gridDataGround&) = delete;

    // Move constructor
    gridDataGround(gridDataGround&& other) noexcept
    {
        Qrs = other.Qrs;
        Qgr = other.Qgr;
        Qgs = other.Qgs;
        Qgi = other.Qgi;
        P = other.P;
        t = other.t;
        T = other.T;
        initialized = other.initialized;
        other.initialized = false;  // Since other is now empty
    }
    // Equal will now move:
    gridDataGround& operator=(gridDataGround&& other) noexcept
    {
        if (this != &other)
        {
            if (initialized)  // If current is already initialized, we need to free our data
            {
                delete[] Qrs;
                delete[] Qgr;
                delete[] Qgs;
                delete[] Qgi;
                delete[] P;
                delete[] t;
                delete[] T;
            }
            Qrs = other.Qrs;
            Qgr = other.Qgr;
            Qgs = other.Qgs;
            Qgi = other.Qgi;
            P = other.P;
            t = other.t;
            T = other.T;
            initialized = other.initialized;
            other.initialized = false;  // Since other is now empty
        }
        return *this;
    }

private:
    bool initialized = false;
};

struct gridDataSkyGPU  // 88 bytes
{
    float* Qv;       //  Mixing Ratio of Water Vapor
    float* Qw;       //	Mixing Ratio of	Liquid Water
    float* Qc;       //	Mixing Ratio of Ice
    float* Qr;       //	Mixing Ratio of Rain
    float* Qs;       //	Mixing Ratio of Snow
    float* Qi;       //	Mixing Ratio of Ice (precip)
    float* potTemp;  // Potential temperature
    float* velfieldX;
    float* velfieldY;
    float* velfieldZ;
    float* pressure;
};

struct gridDataGroundGPU  // 56 bytes
{
    float* Qrs;  // Subsurface water content
    float* Qgr;  // Rain content
    float* Qgs;  // Snow content
    float* Qgi;  // Ice content
    float* P;    // Ground Pressure
    float* t;    // Time since ground was wet
    float* T;    // Ground temperature
};


// Stores data for the debug environment, gaining ability to view and edit variables
struct envDebugData
{
    // Data is filled after each simulation tick from the GPU side
    envDebugData() = default;
    ~envDebugData()
    {
        if (m_groundHeight) delete[] m_groundHeight;
        if (m_debugArray0) delete[] m_debugArray0;
        if (m_debugArray1) delete[] m_debugArray1;
        if (m_debugArray2) delete[] m_debugArray2;
        if (m_envTemp) delete[] m_envTemp;
        if (m_envVapor) delete[] m_envVapor;
        if (m_envPressure) delete[] m_envPressure;
    }

    void init(unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ)
    {
        unsigned int totalSize = sizeX * sizeY * sizeZ;
        unsigned int groundSize = sizeX * sizeZ;

        m_envView.init(totalSize);
        m_groundView.init(groundSize);
        m_groundHeight = new int[groundSize];

        m_debugArray0 = new float[totalSize];
        m_debugArray1 = new float[totalSize];
        m_debugArray2 = new float[totalSize];

        m_envTemp = new float[sizeY];
        m_envVapor = new float[sizeY];
        m_envPressure = new float[sizeY];
    }

    // Variables
    gridDataSky m_envView;
    gridDataGround m_groundView;

    int* m_groundHeight{nullptr};

    float* m_debugArray0{nullptr};
    float* m_debugArray1{nullptr};
    float* m_debugArray2{nullptr};

    float* m_envTemp{nullptr};
    float* m_envVapor{nullptr};
    float* m_envPressure{nullptr};
};