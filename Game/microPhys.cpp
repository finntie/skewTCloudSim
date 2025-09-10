#include "microPhys.h"

#include "math/constants.hpp"
#include "math/meteoformulas.h"
#include "environment.h" //For the defined variables


microPhys::microPhys()
{
    const float b = 0.8f;
    const float d = 0.25f;

    m_gammaR = meteoformulas::gamma(3 + b);
    m_gammaER = meteoformulas::gamma((b + 5.0f) / 2.0f);
    m_gammaS = meteoformulas::gamma(3 + d);
    m_gammaRC = meteoformulas::gamma(6 + b);
    m_gammaSS = meteoformulas::gamma((d + 5.0f) / 2.0f);
    m_gammaI = meteoformulas::gamma(3.5f);
    m_gammaRI = meteoformulas::gamma(2.75f);

}

void microPhys::calculateEnvMicroPhysics(microPhysResult& data)
{
    //----------------------------------------------------------------------------------------------------------------------------------//
    //--------- Formulas and variables from https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf ---------//
    //----------------------------------------------------------------------------------------------------------------------------------//

    float PVCON{ 0.0f }; // Condensation/Evaporation rate of cloud water to vapor
    float PVDEP{ 0.0f }; // Deposition/Sublimation rate of cloud ice to vapor.

    float PIMLT{ 0.0f }; // Melting of cloud ice to form cloud water T > 0
    float PIDW{ 0.0f };  // Depositional growth of cloud ice at expense of cloud water
    float PIHOM{ 0.0f }; // Homogeneous freezing of cloud water to form cloud ice
    float PIACR{ 0.0f }; // Accretion of rain by cloud ice; produces snow or graupel depending on the amount of rain
    float PRACI{ 0.0f }; // Accretion of cloud ice by rain; produces snow or graupel depending on the amount of rain.
    float PRAUT{ 0.0f }; // Autoconversion of cloud water to form rain.
    float PRACW{ 0.0f }; // Accretion of cloud water by rain.
    float PREVP{ 0.0f }; // Evaporation of rain.
    float PRACS{ 0.0f }; // Accretion of snow by rain; produces graupel if rain or snow exceeds threshold and T < 0
    float PSACW{ 0.0f }; // Accretion of cloud water by snow; produces snow if T < 0 or rain if T >= 0. Also enhances snow melting for T >= 0
    float PSACR{ 0.0f }; // Accretion of rain by snow. For T < 0, produces graupel if rain or snow exceeds threshold; if now, produces snow. For T >= 0, the accreted water enchances snow melting
    float PSACI{ 0.0f }; // Accretion of cloud ice by snow.
    float PSAUT{ 0.0f }; // Autoconversion (aggregation) of cloud ice to form snow
    float PSFW{ 0.0f };  // Bergeron process (deposition and riming) transfer of cloud water to form snow.
    float PSFI{ 0.0f };  // Transfer rate of cloud ice to snow through growth of Bergeron process embryos.
    float PSDEP{ 0.0f }; // Depositional growth of snow
    float PSSUB{ 0.0f }; // Sublimation of snow
    float PSMLT{ 0.0f }; // Melting of snow to form rain, T > 0
    float PGAUT{ 0.0f }; // Autoconversion (aggregation) of snow to form graupel.
    float PGFR{ 0.0f };  // Probalistic freezing of rain to form graupel.
    float PGACW{ 0.0f }; // Accretion of cloud water by graupel
    float PGACI{ 0.0f }; // Accretion of cloud ice by graupel
    float PGACR{ 0.0f }; // Accretion of rain by graupel
    float PGDRY{ 0.0f }; // Dry growth of graupel
    float PGACS{ 0.0f }; // Accretion of snow by graupel
    float PGSUB{ 0.0f }; // Sublimation of graupel
    float PGMLT{ 0.0f }; // Melting of graupel to form rain, T > 0. (In this regime, PGACW is assumed to be shed off as rain)
    float PGWET{ 0.0f }; // Wet growth of graupel; may involve PGACS and PGACI and must include: PGACW or PGACR, or both. The amount of PGACW which is not able to freeze is shed off as rain.
    float PGACR1{ 0.0f }; // Fallout or growth of hail by wetness

    //Setting data
    m_Qv = data.Qv > 1e-18f ? data.Qv : 0.0f;
    m_Qw = data.Qw > 1e-18f ? data.Qw : 0.0f;
    m_Qc = data.Qc > 1e-18f ? data.Qc : 0.0f;
    m_Qr = data.Qr > 1e-18f ? data.Qr : 0.0f;
    m_Qs = data.Qs > 1e-18f ? data.Qs : 0.0f;
    m_Qi = data.Qi > 1e-18f ? data.Qi : 0.0f;


    dt = data.dt;
    m_speed = data.speed;
    m_idx = data.index;
    m_T = data.temp;
    m_Tc = m_T - 273.15f;
    m_ps = data.pressure;
    m_Dair = data.density;
    m_GHeight = data.groundHeight;
    m_fallVel = data.fallingVelocity;

    m_QWS = meteoformulas::ws((m_Tc), m_ps); //Maximum water vapor air can hold
    m_QWI = meteoformulas::wi((m_Tc), m_ps); //Maximum water vapor cold air can hold

    //Production terms, used to sometimes create snow or ice or other stuff (formula 20): https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf 
    m_PTerm1 = m_Tc < 0.0f && m_Qw + m_Qc > 0 + 1e-9f ? 1.0f : 0.0f; //TODO: when is there no cloud? (value 1e-9f)
    m_PTerm2 = m_Tc < 0.0f && m_Qr < 1e-4f && m_Qs < 1e-4f ? 1.0f : 0.0f;
    m_PTerm3 = m_Tc < 0.0f && m_Qr < 1e-4f ? 1.0f : 0.0f;

    m_slopeR = meteoformulas::slopePrecip(m_Dair, m_Qr, 0);
    m_slopeS = meteoformulas::slopePrecip(m_Dair, m_Qs, 1);
    m_slopeI = meteoformulas::slopePrecip(m_Dair, m_Qi, 2);

    

    //Setting variables
    PVCON = FPVCON();
    PVDEP = FPVDEP();

    PIMLT = FPIMLT();
    PIDW = FPIDW();
    PIHOM = FPIHOM();
    PIACR = FPIACR();
    PRACI = FPRACI();
    PRAUT = FPRAUT();
    PRACW = FPRACW();
    PREVP = FPREVP();
    PRACS = FPRACS();
    PSACW = FPSACW();
    PSACR = FPSACR();
    PSACI = FPSACI();
    PSAUT = FPSAUT();
    PSFW = FPSFW();
    PSFI = FPSFI();
    PSDEP = FPSDEP();
    PSSUB = FPSSUB(PSDEP);
    PSDEP = std::max(0.0f, PSDEP); //Can't be negative
    PSMLT = FPSMLT(PSACW, PSACR);
    PGAUT = FPGAUT();
    PGFR = FPGFR();
    PGACW = FPGACW();
    PGACI = FPGACI();
    PGACR = FPGACR();
    PGACS = FPGACS(false);
    PGSUB = FPGSUB();
    PGMLT = FPGMLT(PGACW, PGACR);
    PGDRY = FPGDRY(PGACW, PGACI, PGACR, PGACS);
    PGWET = FPGWET(PGACI, PGACS);
    PGACR1 = FPGACR1(PGWET, PGACW, PGACI, PGACS);

    //Depending on which value is smaller, we choose or PGDRY or PGWET.
    //In case of PGWET, we use PGACR1 to decide how much rain or ice we gain, else we use PGACR
    //If PGACR1 >= 0, we remove from rain, because some will be frozen
    //IF PGACR1 < 0, we add to rain, since water will be sheded from hail
    float WTerm = 0.0f;
    if (PGDRY > PGWET || PGDRY == 0.0f) WTerm = 1.0f; 


    //Limit by speed and add to heat latency.
    //TODO: DONT FORGET PTERMS
    if (m_Tc < 0)
    {
        if (PVCON >= 0) data.condens += PVCON = dt * std::min(m_Qv, m_speed * PVCON);
        else if (PVCON < 0) data.condens += PVCON = dt * std::max(-m_Qw, m_speed * PVCON); //Use negative numbers
        if (PVDEP >= 0) data.depos += PVDEP = dt * std::min(m_Qv, m_speed * PVDEP);
        else if (PVDEP < 0) data.depos += PVDEP = dt * std::max(-m_Qc, m_speed * PVDEP); //Use negative numbers

        PIMLT;
        data.depos += PIDW = dt * std::min(m_Qw, m_speed * PIDW);
        data.freeze += PIHOM = dt * std::min(m_Qw, m_speed * PIHOM);
        data.freeze += PIACR = dt * std::min(m_Qr, m_speed * PIACR);
        PRACI = dt * std::min(m_Qc, m_speed * PRACI);
        PRAUT = dt * std::min(m_Qw, m_speed * PRAUT);
        PRACW = dt * std::min(m_Qw, m_speed * PRACW);
        data.condens -= PREVP = dt * std::min(m_Qr, m_speed * PREVP * (1 - m_PTerm1));
        PRACS = dt * std::min(m_Qs, m_speed * PRACS * (1 - m_PTerm2));
        data.freeze += PSACR = dt * std::min(m_Qr, m_speed * PSACR);
        data.freeze += PSACW = dt * std::min(m_Qw, m_speed * PSACW);
        PSACI = dt * std::min(m_Qc, m_speed * PSACI);
        PSAUT = dt * std::min(m_Qc, m_speed * PSAUT);
        data.freeze += PSFW = dt * std::min(m_Qw, m_speed * PSFW);
        PSFI = dt * std::min(m_Qc, m_speed * PSFI);
        data.depos += PSDEP = dt * std::min(m_Qv, m_speed * PSDEP * (m_PTerm1));
        data.depos -= PSSUB = dt * std::min(m_Qs, m_speed * PSSUB * (1 - m_PTerm1));
        PSMLT;

        PGAUT = dt * std::min(m_Qs, m_speed * PGAUT);
        data.freeze += PGFR = dt * std::min(m_Qr, m_speed * PGFR);
        PGACW;
        PGACI = dt * std::min(m_Qc, m_speed * PGACI);
        data.freeze += PGACR = dt * std::min(m_Qr, m_speed * PGACR * (1 - WTerm));
        PGACS = dt * std::min(m_Qs, m_speed * PGACS);
        data.depos -= PGSUB = dt * std::min(m_Qi, m_speed * PGSUB * (1 - m_PTerm1));
        PGMLT;
        if (WTerm == 0.0f)
        {
            //Values that were used for PGDRY are already limited, and added to the correct heat latency
            //PGDRY is only used as addition, so no use of limiting
            PGDRY;
        }
        else if (WTerm == 1.0f)
        {
            //Values that were used for PGWET are already limited, and added to the correct heat latency
            //Altough PGWET is calculated a bit different, it still is okay, since the other values are accounted for in PGACR1
            PGWET;
            //PGACR1 we want to limit on rain or ice depending on if its positive or negative.
            //Although we don't remove PGACR1 from hail, if value is negative, we add to rain (thus take from ice)
            if (PGACR1 >= 0) data.freeze += PGACR1 = dt * std::min(m_Qr, m_speed * PGACR1);
            else if (PGACR1 < 0) data.freeze += PGACR1 = dt * std::max(-m_Qi, m_speed * PGACR1); //Use negative numbers
        }
    }
    else
    {
        if (PVCON >= 0) data.condens += PVCON = dt * std::min(m_Qv, m_speed * PVCON);
        else if (PVCON < 0) data.condens += PVCON = dt * std::max(-m_Qw, m_speed * PVCON); //Use negative numbers
        if (PVDEP >= 0) data.depos += PVDEP = dt * std::min(m_Qv, m_speed * PVDEP);
        else if (PVDEP < 0) data.depos += PVDEP = dt * std::max(-m_Qc, m_speed * PVDEP); //Use negative numbers

        data.freeze -= PIMLT = dt * std::min(m_Qc, m_speed * PIMLT);
        PIDW;
        PIHOM;
        PIACR;
        PRACI;
        PRAUT = dt * std::min(m_Qw, m_speed * PRAUT);
        PRACW = dt * std::min(m_Qw, m_speed * PRACW);
        data.condens -= PREVP = dt * std::min(m_Qr, m_speed * PREVP * (1 - m_PTerm1));
        PRACS;
        PSACR;
        PSACW; //Limited by PSMLT
        PSACI;
        PSAUT;
        PSFW;
        PSFI;
        PSDEP;
        PSSUB;
        data.freeze -= PSMLT = dt * std::min(m_Qs, m_speed * PSMLT);

        PGAUT;
        PGFR;
        data.freeze += PGACW = dt * std::min(m_Qw, m_speed * PGACW);
        PGACI;
        PGACR;
        PGACS = dt * std::min(m_Qs, m_speed * PGACS);
        PGSUB;
        data.freeze -= PGMLT = dt * std::min(m_Qi, m_speed * PGMLT);
        if (WTerm == 0.0f)
        {
            //Values that were used for PGDRY are already limited, and added to the correct heat latency
            //PGDRY is only used as addition, so no use of limiting
            PGDRY;
        }
        else if (WTerm == 1.0f)
        {
            //Values that were used for PGWET are already limited, and added to the correct heat latency
            //Altough PGWET is calculated a bit different, it still is okay, since the other values are accounted for in PGACR1
            PGWET;
            //PGACR1 we want to limit on rain or ice depending on if its positive or negative.
            if (PGACR1 >= 0) PGACR1;
            else if (PGACR1 < 0) PGACR1;
        }
    }

    data.Qv = 0.0f;
    data.Qw = 0.0f;
    data.Qc = 0.0f;
    data.Qr = 0.0f;
    data.Qs = 0.0f;
    data.Qi = 0.0f;

    // Var       Add       Sub       Description
    //---------------------------------------------
    //PVCON   | +Qv, Qw | -Qv, Qw | (Depending on positive or negative)
    //PVDEP   | +Qv, Qc | -Qv, Qc | (Depending on positive or negative)
    //PIMLT:  | +Qw     | -Qc     | (if T >= 0)
    //PIDW:   | +Qc     | -Qw     | (if T < 0)
    //PIHOM:  | +Qc     | -Qw     | (if T < -40)
    //PIACR:  | +Qs, Qi | -Qr     | (Depending on PTerm3)
    //PRACI:  | +Qs, Qi | -Qc     | (Depending on PTerm3)
    //PRAUT:  | +Qr     | -Qw     |
    //PRACW:  | +Qr     | -Qw     |
    //PREVP:  | +Qv     | -Qr     |
    //PRACS:  | +Qi     | -Qs     | (Depending on PTerm2, and T < 0)
    //PSACR:  | +Qs, Qi | -Qr     | (Depending on PTerm2)
    //PSACW:  | +Qs, Qr | -Qr, Qw | (+Qs if T < 0, +Qr if T >= 0)
    //PSACI:  | +Qs     | -Qc     |
    //PSAUT:  | +Qs     | -Qc     |
    //PSFW:   | +Qs     | -Qw     |
    //PSFI:   | +Qs     | -Qc     |
    //PSDEP:  | +Qs     | -Qv     |
    //PSSUB:  | +Qv     | -Qs     |
    //PSMLT:  | +Qr     | -Qs     | (if T >= 0)
    //PGAUT:  | +Qi     | -Qi     |
    //PGFR:   | +Qi     | -Qr     |
    //PGSUB:  | +Qv     | -Qi     |
    //PGMLT:  | +Qr     | -Qi     | (if T >= 0)
    //PGACW:  | +Qi, Qr | -Qw     | (Included in PGWET or PGDRY)
    //PGACI:  | +Qi     | -Qc     | (Included in PGWET or PGDRY)
    //PGACR:  | +Qi     | -Qr     | (Included in PGWET or PGDRY)
    //PGACS:  | +Qi     | -Qs     | (Included in PGWET or PGDRY)
    //PGDRY:  | +Qi     |         | (Depending on dry or wet)
    //PGWET:  | +Qi     |         | (Is included in PGACR1)
    //PGACR1: | +Qr, Qi | -Qr, Qi | (If PGWET and depending on positive or negative)

    if (m_Tc < 0)
    {
        data.Qv = PSSUB * (1 - m_PTerm1) + PGSUB * (1 - m_PTerm1) +
            PREVP * (1 - m_PTerm1) - PSDEP * (m_PTerm1) -
            PVCON - PVDEP;

        data.Qw = PVCON - PSACW - PSFW  - PRAUT - PRACW - PIDW - PIHOM;
        data.Qc = PVDEP + PIDW + PIHOM - PSAUT - PSACI - PRACI - PSFI - PGACI;

        data.Qr = PRAUT + PRACW - PIACR - PSACR -
            PGACR * (1 - WTerm) - PGACR1 * (WTerm) - //Dependent on wet or dry growth
            PGFR - PREVP * (1 - m_PTerm1);

        data.Qs = PSAUT + PSACI + PSACW + PSFW + PSFI +
            PRACI * (m_PTerm3) + PIACR * (m_PTerm3) - PGACS - PGAUT -
            PRACS * (1 - m_PTerm2) + PSACR * (m_PTerm2) -
            PSSUB * (1 - m_PTerm1) + PSDEP * (m_PTerm1);

        data.Qi = PGAUT + PGFR + 
            PGDRY * (1 - WTerm) + PGWET * (WTerm) + //Wet or dry growth
            PSACR * (1 - m_PTerm2) + PRACS * (1 - m_PTerm2) +
            PRACI * (1 - m_PTerm3) + PIACR * (1 - m_PTerm3) - PGSUB * (1 - m_PTerm1);
    }
    else
    {
        data.Qv = PREVP * (1 - m_PTerm1) -
            PVCON - PVDEP;
        data.Qw = PVCON + PIMLT - PRAUT - PRACW - PGACW;
        data.Qc = PVDEP - PIMLT;

        data.Qr = PRAUT + PRACW + PSACW + PGACW +
            PGMLT + PSMLT - PREVP * (1 - m_PTerm1);

        data.Qs = -PSMLT - PGACS;
        data.Qi = -PGMLT + PGACS;
    }

    //Check for certainty
    if (m_Qv + data.Qv < 0.0f || m_Qw + data.Qw < 0.0f || m_Qc + data.Qc < 0.0f || m_Qr + data.Qr < 0.0f || m_Qs + data.Qs < 0.0f || m_Qi + data.Qi < 0.0f)
    {
        data.freeze = 0.0f;
    }
    if (data.Qi > 0.0f && m_Tc < -30.0f)
    {
        //data.freeze = 0.0f; //Check
    }
}

using namespace Constants;

float microPhys::FPVCON()
{
    //Condensation rate https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
    {
        const float _ES = meteoformulas::es(m_Tc) * 10.0f;
        const float derivative = ((E * m_ps) / ((m_ps - _ES) * (m_ps - _ES))) * (_ES * (17.27f * 237.3f / ((m_Tc + 237.3f) * (m_Tc + 237.3f))));
        return 1 / (dt * m_speed) * (m_Qv - m_QWS) / (1 + Constants::E0v / Constants::Cpd * derivative);
    }
}

float microPhys::FPVDEP()
{
    //Deposition rate based on https://www.ecmwf.int/sites/default/files/elibrary/2002/16952-parametrization-non-convective-condensation-processes.pdf
    {
        const float _EI = meteoformulas::ei(m_Tc) * 10.0f;
        const float derivative = ((E * m_ps) / ((m_ps - _EI) * (m_ps - _EI))) * (_EI * (21.875f * 265.5f / ((m_Tc + 265.5f) * (m_Tc + 265.5f))));
        return 1 / (dt * m_speed) * (m_Qv - m_QWI) / (1 + Constants::Ls / Constants::Cpd * derivative); //TODO: check pos/neg
    }
}

float microPhys::FPIMLT()
{
    //Melt cloud ice if possible
    if (m_Tc >= 0.0f && m_Qc > 0.0f)
    {
        //TODO: limit melting?
        //const float limit = Cpd / Lf * m_Tc;
        
        //Melt all or the maximum we can handle
        return m_Qc;
    }
    return 0.0f;
}

float microPhys::FPIDW()
{
    if (m_Tc < 0.0f && m_Qw > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/mwre/128/4/1520-0493_2000_128_1070_asfcot_2.0.co_2.xml and WeatherScapes
        const float a = 0.5f; // capacitance for hexagonal crystals
        const float quu = std::max(1e-12f * meteoformulas::Ni(m_Tc) / m_Dair, m_Qc);
        return pow((1 - a) * meteoformulas::cvd(m_Tc, m_ps, m_Dair) * dt + pow(quu, 1 - a), 1 / (1 - a)) - m_Qc;
    }
    return 0.0f;
}

float microPhys::FPIHOM()
{
    if (m_Tc < -40 && m_Qw > 0.0f)
    {
        return m_Qw;
    }
    return 0.0f;
}

float microPhys::FPIACR()
{
    if (m_Qr > 0 && m_Qc > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0R = 8e-2f;
        //Densities in g/cm3
        const float densW = 0.99f;
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;
        const float MassI = 4.19e-10f; //Mass ice in gram
        //collection efficiency rain from cloud ice and cloud water
        const float ERI = 0.3f;

        const float density = pow(1.225f / m_Dair, 0.5f);

        return (PI * PI * ERI * N0R * a * m_Qc * densW * m_gammaRC) / (24 * MassI * pow(m_slopeR, 6 + b)) * density;
    }
    return 0.0f;
}

float microPhys::FPRACI()
{
    if (m_Qr > 0 && m_Qc > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0R = 8e-2f;
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;
        //collection efficiency rain from cloud ice and cloud water
        const float ERI = 0.3f;

        const float density = pow(1.225f / m_Dair, 0.5f);

        return (PI * ERI * N0R * a * m_Qc * m_gammaR) / (4 * pow(m_slopeR, 3 + b)) * density;
    }
    return 0.0f;
}

float microPhys::FPRAUT()
{
    //Cloud to rain due to autoconversion
    if (m_Qw >= m_qwmin)
    {
        const float rc = 10e-6f; // Estimated cloud droplet size, averages between 4 and 12 μm;
        float Nc = (m_Qw / (4 / 3 * PI * meteoformulas::pwater(m_Tc) * rc * rc * rc)) * 1.225f; //Cloud number concentation in m3
        Nc /= 1e6f; //Convert to how many droplets would be in cm3 instead of m3
        const float disE = 0.5f * 0.5f; //following Liu et al. (2006)
        //Normally its ^ 1 / 6, but in the formula we will be using its ^6, so it cancells out.
        const float dispersion = ((1 + 3 * disE) * (1 + 4 * disE) * (1 + 5 * disE)) / ((1 + disE) * (1 + 2 * disE)); //the relative dispersion of cloud droplets, 
        const float kBAW = 1.1e10f; //Constant k in g-2/cm3/s-1
        const float shapeParam = 1.0f; //Shape of tail of drop, in micrometer
        const float L = m_Qw * 1.225f * 1e-3f; //From kg/kg to g/cm3

        // Aggregation rate of liquid to rain rate coefficient: // Liu–Daum–McGraw–Wood (LD) scheme: https://journals.ametsoc.org/view/journals/atsc/63/3/jas3675.1.xml
        return 1e3f * kBAW * dispersion * (L * L * L) * std::powf(Nc, -1) *
            static_cast<float>(1 - std::exp(-std::pow(1.03e16f * std::pow(Nc, -2.0f / 3.0f) * (L * L), shapeParam))); //First 1e3 is conversion to kg/m3
    }
    return 0.0f;
}

float microPhys::FPRACW()
{
    if (m_Qr > 0 && m_Qw > 0.0f)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0R = 8e-2f;
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;
        //collection efficiency rain from cloud ice and cloud water
        const float ERW = 1.0f;

        const float density = pow(1.225f / m_Dair, 0.5f);

        //Collection from cloud water
        return (PI * ERW * N0R * a * m_Qw * m_gammaR) / (4 * pow(m_slopeR, 3 + b)) * density;
    }
    return 0.0f;
}

float microPhys::FPREVP()
{
    //TODO: will be changed
    // Evaporation rate of rain
    if (m_Qr > 0.0f && m_Qv / m_QWS < 1.0f && (1 - m_PTerm1)) //Only if there is vapor dificit in relation to saturation mixing ratio
    {
        //Intercept parameter size distribution in cm-4
        const float N0R = 8e-2f;
        const float S = m_Qv / m_QWS; //Saturation ratio
        //constants
        const float b = 0.8f;
        const float a = 2115.0f;

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)

        const float density = pow(1.225f / (m_Dair * 0.001f), 0.25f);

        //Added a -1 multiplier to make it positive instead of negative (Due to S - 1)
        return std::max(0.0f, -1 * 2 * PI * (S - 1) * N0R * (0.78f * powf(m_slopeR, -2) + 0.31f * pow(Sc, 1.0f / 3.0f) *
            m_gammaER * powf(a, 0.5f) * powf(kv, -0.5f) * density * pow(m_slopeR, -((b + 5) / 2))) *
            1 / (m_Dair * 0.001f) * powf(E0v * E0v / (Ka * Rsw * m_T * m_T) + (1 / (m_Dair * 0.001f * m_QWS * dQv)), -1.0f));
    }
    return 0.0f;
}

float microPhys::FPRACS()
{
    //Used for 3 component freezing snow to hail.
    if (m_Qr > 0.0f && m_Qs > 0.0f && m_PTerm2 == 0)
    {
        //Densities in g/cm3
        const float densS = 0.11f;
        //Intercept parameter size distribution in cm-4
        const float N0S = 3e-2f;
        const float N0R = 8e-2f;

        const float ESR = 1.0f;
        //If PTerm2 = 0, 3-component freezing to increase hail.
        //Else 2-component freezing, snow grows in expens of rain, only PSACR is used.
        //But if T > 0, PSACR is used to enhance PSMLT.

        const float size = PI * PI * ESR * N0R * N0S;

        //TODO: check densS / m_Dair
        return size * fabs(m_fallVel.x - m_fallVel.y) * (densS / (m_Dair * 0.001f)) *
            (5 / (powf(m_slopeS, 6) * m_slopeR) + (2 / (powf(m_slopeS, 5) * powf(m_slopeR, 2))) + (0.5f / (powf(m_slopeS, 4) * powf(m_slopeR, 3))));
    }
    return 0.0f;
}

float microPhys::FPSACW()
{
    if (m_Qs > 0 && m_Qw > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;
        //collection efficiency snow from cloud ice and cloud water
        const float ESW = 1.0f;

        const float density = pow(1.225f / m_Dair, 0.5f);

        return (PI * ESW * N0S * c * m_Qw * m_gammaS) / (4 * pow(m_slopeS, 3 + d)) * density;
    }
    return 0.0f;
}

float microPhys::FPSACR()
{
    //Used in PSMLT or decreases rain
    if (m_Qr > 0.0f && m_Qs > 0.0f)
    {
        //Densities in g/cm3
        const float densW = 0.99f;
        //Intercept parameter size distribution in cm-4
        const float N0S = 3e-2f;
        const float N0R = 8e-2f;

        const float ESR = 1.0f;
        //If PTerm2 = 0, 3-component freezing to increase hail.
        //Else 2-component freezing, snow grows in expens of rain, only PSACR is used.
        //But if T > 0, PSACR is used to enhance PSMLT.

        const float size = PI * PI * ESR * N0R * N0S;

        return size * fabs(m_fallVel.y - m_fallVel.x) * (densW / (m_Dair * 0.001f)) *
            (5 / (powf(m_slopeR, 6) * m_slopeS) + (2 / (powf(m_slopeR, 5) * powf(m_slopeS, 2))) + (0.5f / (powf(m_slopeR, 4) * powf(m_slopeS, 3))));
    }
    return 0.0f;
}

float microPhys::FPSACI()
{
    if (m_Qs > 0 && m_Qc > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //How many of this particle are in this region
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;
        //collection efficiency snow from cloud ice and cloud water
        const float ESI = exp(0.025f * (m_Tc)); 

        const float density = pow(1.225f / m_Dair, 0.5f);

        return (PI * ESI * N0S * c * m_Qc * m_gammaS) / (4 * pow(m_slopeS, 3 + d)) * density;
    }
    return 0.0f;
}

float microPhys::FPSAUT()
{
    if (m_Qc - m_qcmin > 0.0f)
    {
        return 1e-3f * exp(0.025f * (m_Tc)) * (m_Qc - m_qcmin);
    }
    return 0.0f;
}

float microPhys::FPSFW()
{
    if (m_Qs > 0 && m_Qw > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //collection efficiency snow from cloud ice and cloud water
        const float EIW = 1.0f;

        //radius, mass and terminal velocity of 50 or 40 micrometer size ice crystal to cm or gram
        const float RI50 = 5e-3f;
        const float mI50 = 4.8e-7f;
        const float mI40 = 2.46e-7f;
        const float UI50 = 100.0f;

        //Formula for A and B https://journals.ametsoc.org/view/journals/atsc/40/5/1520-0469_1983_040_1185_tmamsa_2_0_co_2.xml?tab_body=pdf
        //const float density = pow(1.225f / m_Dair, 0.5f);
        float Si = m_Qv / m_QWI; // Saturation ratio over ice
        const float X = meteoformulas::DQVair(m_Tc, m_ps);
        const float A = Constants::E0v / (Constants::Ka * m_T) * (Constants::Ls * Constants::Mw * 0.001f / (Constants::R * m_T) - 1); //Mw to kg/mol
        const float B = Constants::R * m_T * X * Constants::Mw * 0.001f * meteoformulas::ei(m_Tc) * 1000; //kPa to Pa
        
        const float a1g = 206.2f * (Si - 1) / (A + B);
        const float a2g = exp(0.09f * m_Tc);

        const float dt1 = 1 / (a1g * (1 - a2g)) * (pow(mI50, 1 - a2g) - pow(mI40, 1 - a2g));

        const float Qc50 = m_Qc * (dt / dt1) / 100; //Should vary between 0.5 and 10%
        const float NI50 = Qc50 / mI50;
        return NI50 * (a1g * pow(mI50, a2g) + PI * EIW * (m_Dair * 0.001f) * m_Qw * RI50 * RI50 * UI50);

    }
    return 0.0f;
}

float microPhys::FPSFI()
{
    if (m_Qs > 0 && m_Qc > 0)
    {
        //Formula from https://journals.ametsoc.org/view/journals/apme/22/6/1520-0450_1983_022_1065_bpotsf_2_0_co_2.xml
        //Correction from https://ntrs.nasa.gov/api/citations/19990100647/downloads/19990100647.pdf

        //radius, mass and terminal velocity of 50 or 40 micrometer size ice crystal to cm or gram
        const float mI50 = 4.8e-7f;
        const float mI40 = 2.46e-7f;

        //Formula for A and B https://journals.ametsoc.org/view/journals/atsc/40/5/1520-0469_1983_040_1185_tmamsa_2_0_co_2.xml?tab_body=pdf
        float Si = m_Qv / m_QWI; // Saturation ratio over ice
        const float X = meteoformulas::DQVair(m_Tc, m_ps);
        const float A = Constants::E0v / (Constants::Ka * m_T) * (Constants::Ls * Constants::Mw * 0.001f / (Constants::R * m_T) - 1); //Mw to kg/mol
        const float B = Constants::R * m_T * X * Constants::Mw * 0.001f * meteoformulas::ei(m_Tc) * 1000; //kPa to Pa

        const float a1g = 206.2f * (Si - 1) / (A + B);
        const float a2g = exp(0.09f * m_Tc);

        if (a1g > 0) return a2g * a1g * m_Qc / (pow(mI50, 0.5f) - pow(mI40, 0.5f));
    }
    return 0.0f;
}

float microPhys::FPSDEP()
{
    if (m_Tc < 0.0f)
    {
        //How many of this particle
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        float Si = m_Qv / m_QWI; // Saturation ratio over ice

        const float A = Constants::Ls * Constants::Ls / (Constants::Ka * Constants::Rsw * m_T * m_T);
        const float B = 1 / (m_Dair * 0.001f * meteoformulas::wi(m_Tc, m_ps) * dQv);

        const float density = pow(1.225f / m_Dair, 0.25f);

        return PI * PI * (Si - 1) / (m_Dair * 0.001f * (A + B)) * N0S *
            (0.78f * powf(m_slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaSS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(m_slopeS, -(d + 5) / 2));
    }
    return 0.0f;
}

float microPhys::FPSSUB(const float PSDEP)
{
    return std::max(0.0f, -1 * PSDEP); //Inverse of depos and can't be negative
}

float microPhys::FPSMLT(const float PSACW, const float PSACR)
{
    if (m_Tc >= 0.0f && m_Qs > 0.0f)
    {
        //TODO: introduce limit?
        //const float limit = Cpd / Lf * m_Tc;

        //PSMLT by https://research.csiro.au/ccam/wp-content/uploads/sites/520/2024/01/1377337420.pdf
        //How many of this particle
        const float N0S = 3e-2f;
        //constants
        const float c = 152.93f;
        const float d = 0.25f;

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = meteoformulas::ws(0.0f, m_ps) - m_Qv;

        const float density = pow(1.225f / m_Dair, 0.25f);

        //Multiply by -1 to make value turned around (we want to make it positive)
        return std::max(0.0f, -1 * (-(2 * PI / (density * 0.001f * Lf)) * (Ka * m_Tc - E0v * dQv * density * 0.001f * Drs) * N0S *
            (0.78f * powf(m_slopeS, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaSS * powf(c, 0.5f) * density * powf(kv, -0.5f) * powf(m_slopeS, -(d + 5) / 2))
            - (Constants::Cvl * m_Tc / Lf) * (PSACW + PSACR)));
    }
    return 0.0f;
}

float microPhys::FPGAUT()
{
    if (m_Qs - m_qimin > 0.0f)
    {
        return 1e-3f * exp(0.09f * m_Tc);
    }
    return 0.0f;
}

float microPhys::FPGFR()
{
    if (m_Qr > 0.0f)
    {
        //How many of this particle
        const float N0R = 8e-2f;
        //Densities in g/cm3
        const float densW = 0.99f;
        //Constants
        const float A = 0.66f; // Kelvin
        const float B = 100.0f; // m3/s

        return 20 * PI * PI * B * N0R * (densW / (m_Dair * 0.001f)) * (exp(A * (273.15f - m_T)) - 1) * powf(m_slopeR, -7);
    }
    return 0.0f;
}

float microPhys::FPGACW()
{
    if (m_Qi > 0.0f && m_Qw > 0.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float EGW = 1.0f;
        const float CD = 0.6f; //Drag coefficient
        
        return PI * EGW * N0I * m_Qw * m_gammaI / (4 * pow(m_slopeI, 3.5f)) * pow(4 * g * 100 * densI / (3 * CD * m_Dair * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
    }
    return 0.0f;
}

float microPhys::FPGACI()
{
    if (m_Qi > 0.0f && m_Qc > 0.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float EGI = 1.0f;
        const float CD = 0.6f; //Drag coefficient

        return PI * EGI * N0I * m_Qc * m_gammaI / (4 * pow(m_slopeI, 3.5f)) * pow(4 * g * 100 * densI / (3 * CD * m_Dair * 0.001f), 0.5f); //Converting g to cm/s2 and densAir to g/cm3
    }
    return 0.0f;
}

float microPhys::FPGACR()
{
    if (m_Qi > 0.0f && m_Qr > 0.0f)
    {
        //How many of this particle
        const float N0R = 8e-2f;
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densW = 0.99f;
        //Constants
        const float EGR = 0.1f; //0.1 since this is for dry growth

        return PI * PI * EGR * N0R * N0I * fabs(m_fallVel.z - m_fallVel.x) * (densW / (m_Dair * 0.001f)) *
            (5 / (powf(m_slopeR, 6) * m_slopeI) + (2 / (powf(m_slopeR, 5) * powf(m_slopeI, 2))) + (0.5f / (powf(m_slopeR, 4) * powf(m_slopeI, 3))));
    }
    return 0.0f;
}

float microPhys::FPGACS(const bool EGS1)
{
    if (m_Qs > 0.0f && m_Qi > 0.0f)
    {
        //Densities in g/cm3
        const float densS = 0.11f;
        //How many of this particle
        const float N0S = 3e-2f;
        const float N0I = 4e-4f;

        const float EGS = m_Tc >= 0.0f || EGS1 ? 1.0f : exp(0.09f * m_Tc);

        return PI * PI * EGS * N0S * N0I * fabs(m_fallVel.z - m_fallVel.y) * (densS / (m_Dair * 0.001f)) *
            (5 / (powf(m_slopeS, 6) * m_slopeI) + (2 / (powf(m_slopeS, 5) * powf(m_slopeI, 2))) + (0.5f / (powf(m_slopeS, 4) * powf(m_slopeI, 3))));

    }
    return 0.0f;
}

float microPhys::FPGDRY(const float PGACW, const float PGACI, const float PGACR, const float PGACS)
{
    return PGACW + PGACI + PGACR + PGACS;
}

float microPhys::FPGSUB()
{
    float Si = m_Qv / m_QWI; // Saturation ratio over ice

    if (m_Qi > 0.0f && Si < 1.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)

        const float A = Constants::Ls * Constants::Ls / (Constants::Ka * Constants::Rsw * m_T * m_T);
        const float B = 1 / (m_Dair * 0.001f * meteoformulas::wi(m_Tc, m_ps) * dQv);

        const float density = pow(4 * g * 100 * densI / (3 * CD * m_Dair * 0.001f), 0.25f);

        //We multiply by -1, since Si - 1 should be negative for sublimation to occur. Hail deposition could not occur, since this would convert to snow!
        return std::max(0.0f, -1 * PI * PI * (Si - 1) / (m_Dair * 0.001f * (A + B)) * N0I *
            (0.78f * powf(m_slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(m_slopeI, -2.75f)));
    }
    return 0.0f;
}

float microPhys::FPGMLT(const float PGACW, const float PGACR)
{
    if (m_Qi > 0.0f)
    {
        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = meteoformulas::ws(0.0f, m_ps) - m_Qv;

        const float density = pow(4 * g * 100 * densI / (3 * CD), 0.25f);

        //Multiply by -1 to make value turned around (we want to make it positive)
        return std::max(0.0f, -1 * (-2 * PI / (m_Dair * 0.001f * Lf) * (Ka * m_Tc - E0v * dQv * m_Dair * 0.001f * Drs) * N0I *
            (0.78f * powf(m_slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(m_slopeI, -2.75f)) -
            Cvl * m_Tc / Lf * (PGACW + PGACR)));
    }
    return 0.0f;
}


float microPhys::FPGWET(const float PGACI, const float PGACS)
{
    if (m_Qi > 0.0f)
    {
        const float PGACI1 = PGACI * 10.0f; //Multiplying by 10 is the same as re-calculating PGACI with EGI as 1.0f
        //Re-calcuating PGACS if Temp is lower then 0.0 to ensure EGS is 1.0f TODO: could otherwise use division by exp(0.09f * m_TC) to regain value
        const float PGACS1 = m_Tc >= 0.0f ? PGACS : FPGACS(true);

        //How many of this particle
        const float N0I = 4e-4f;
        //Density in g/cm3
        const float densI = 0.91f;
        //Constants
        const float CD = 0.6f; //Drag coefficient

        const float dQv = meteoformulas::DQVair(m_Tc, m_ps); //Diffusivity of water vapor in air
        const float kv = meteoformulas::ViscAir(m_Tc); //Kinematic viscosity of air
        float Sc = kv / dQv; // Schmidt number (kv /dQv)
        const float Drs = meteoformulas::ws(0.0f, m_ps) - m_Qv;

        const float density = pow(4 * g * 100 * densI / (3 * CD), 0.25f);

        return 2 * PI * N0I * (m_Dair * 0.001f * E0v * dQv * Drs - Ka * m_Tc) / (m_Dair * 0.001f * (Lf + Cvl * m_Tc)) *
            (0.78f * powf(m_slopeI, -2) + 0.31f * powf(Sc, 1.0f / 3.0f) * m_gammaRI * density * powf(kv, -0.5f) * powf(m_slopeI, -2.75f)) +
            (PGACI1 + PGACS1) * (1 - Cpi * m_Tc / (Lf + Cvl * m_Tc));
    }

    return 0.0f;
}

float microPhys::FPGACR1(const float PGWET, const float PGACW, const float PGACI, const float PGACS)
{
    if (m_Qi > 0.0f)
    {
        const float PGACI1 = PGACI * 10.0f; //Multiplying by 10 is the same as re-calculating PGACI with EGI as 1.0f
        //Re-calcuating PGACS if Temp is lower then 0.0 to ensure EGS is 1.0f TODO: could otherwise use division by exp(0.09f * m_TC) to regain value
        const float PGACS1 = m_Tc >= 0.0f ? PGACS : FPGACS(true);

        //If PGACW < (PGWET - PGACI1 - PGACS1), then PGACR1 is positive, meaning, we have freezing, else the wetness collected on the hail will melt off. (PGACR1 < 0)
        return PGWET - PGACW - PGACI1 - PGACS1;
    }

    return 0.0f;
}

