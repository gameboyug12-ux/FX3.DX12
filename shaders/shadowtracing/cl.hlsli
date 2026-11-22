// XWeCelestial.hlsli
struct SunState
{
    float3 Direction;      // Pravac ka Suncu
    float  AngularRadius;  // 0.00465 rad (IRL prečnik Sunca)
    float  Intensity;      // 0.0 noću, 1.0 danju
    float3 Pad;
};
struct MoonState
{
    float3 Direction;      // Pravac ka Mesecu
    float  AngularRadius;  // 0.00455 rad (IRL prečnik Meseca)
    float  Illumination;   // 0.0 (mlad mesec) do 1.0 (pun mesec)
    float  PhaseAngle;     // 0 do 2*PI (korisno za Sky shader da iscrta fazu Meseca!)
    float2 Pad;
};
cbuffer AstroTimeCB : register(b0)
{
    float LatitudeDeg;
    float CurrentYear;
    float DayOfYear;
    float TimeOfDayHours;
};