#include "cl.hlsli"
RWStructuredBuffer<MoonState> OutMoon : register(u0);
void CSMain()
{
    const float PI = 3.14159265f;
    const float DEG2RAD = PI / 180.0f;
    const float SYNODIC_MONTH = 29.530588f;
    const float SIDEREAL_MONTH = 27.321661f;

    float totalDays = (CurrentYear - 2000.0f) * 365.25f + DayOfYear + (TimeOfDayHours / 24.0f);
    float lat = LatitudeDeg * DEG2RAD;

    // 1. Lunarna faza (0.0 = mlad, 0.5 = pun)
    float lunarAge = frac((totalDays - 5.5f) / SYNODIC_MONTH);
    float phaseAngle = lunarAge * 2.0f * PI;
    float illumination = 0.5f * (1.0f - cos(phaseAngle));

    // 2. Deklinacija Meseca sa nagibom orbite (5.145°)
    float sunDecl = 23.44f * sin((360.0f / 365.0f) * (totalDays - 81.0f) * DEG2RAD);
    float moonOrbitTilt = 5.145f * sin((2.0f * PI / SIDEREAL_MONTH) * totalDays);
    float declination = (-sunDecl * cos(phaseAngle) + moonOrbitTilt) * DEG2RAD;

    // 3. Satni ugao (kašnjenje Meseca ~50 min dnevno)
    float moonSolarTime = TimeOfDayHours - (lunarAge * 24.0f);
    float hourAngle = (moonSolarTime - 12.0f) * 15.0f * DEG2RAD;

    // 4. Elevacija i Azimut
    float sinElev = sin(lat) * sin(declination) + cos(lat) * cos(declination) * cos(hourAngle);
    float elevation = asin(clamp(sinElev, -1.0f, 1.0f));

    float cosAzim = (sin(declination) - sin(elevation) * sin(lat)) / (cos(elevation) * cos(lat) + 1e-5f);
    float azimuth = acos(clamp(cosAzim, -1.0f, 1.0f));

    float wrappedTime = frac(moonSolarTime / 24.0f) * 24.0f;
    if (wrappedTime > 12.0f)
        azimuth = 2.0f * PI - azimuth;

    MoonState moon;
    moon.Direction = normalize(float3(
        cos(elevation) * sin(azimuth),
        sin(elevation),
        cos(elevation) * cos(azimuth)
    ));
    moon.AngularRadius = 0.00455f;
    moon.Illumination = illumination;
    moon.PhaseAngle = phaseAngle;
    moon.Pad = float2(0, 0);

    OutMoon[0] = moon;
}
