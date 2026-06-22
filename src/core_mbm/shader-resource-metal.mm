/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal shader resource catalogue.
// Each built-in shader is stored as a triple: name, MSL source, CFG string.
// PS entries contain ONLY the fragment function — compileShader() prepends
// the auto-generated vertex preamble (buildVertexHeader) at compile time.
// VS entries are complete MSL programs (vertex + fragment).
//
// Custom uniform layout:  constant float* f [[buffer(2)]]  (fragment)
//                         constant float* vf [[buffer(3)]] (vertex, scale.vs only)
// Variables are stored sequentially in CFG declaration order.

#if defined(USE_METAL)

#include <light.h>
#include <string>

namespace mbm
{
    static std::string buildLitTexturedPixelShaderMetal()
    {
        const std::string supportedMaxLights = std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS);
        return R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample2 [[texture(2)]],
    sampler          samp    [[sampler(0)]],
    constant int    &LightEnabled      [[buffer(4)]],
    constant float4 &AmbientColor      [[buffer(6)]],
    constant float3 &LightDirectionView [[buffer(7)]],
    constant float4 *LightColor        [[buffer(8)]],
    constant float4 &MaterialDiffuse   [[buffer(9)]],
    constant float4 &MaterialAmbient   [[buffer(10)]],
    constant float4 &MaterialSpecular  [[buffer(11)]],
    constant float4 &MaterialEmissive  [[buffer(12)]],
    constant float  &MaterialPower     [[buffer(13)]],
    constant int    &LightMode         [[buffer(14)]],
    constant float3 *LightPositionView [[buffer(15)]],
    constant float  *LightRadius       [[buffer(16)]],
    constant int    &LightCount        [[buffer(5)]],
    constant int    &HasNormalMap      [[buffer(17)]])
{
    float4 texColor = sample0.sample(samp, in.uv);
    if (LightEnabled == 0 || LightMode == 0)
        return texColor;
    float3 base        = texColor.rgb * MaterialDiffuse.rgb;
    float3 light       = AmbientColor.rgb * MaterialAmbient.rgb;
    float3 specular    = float3(0.0f);
    if (LightMode == 1)
    {
        float3 normalView  = normalize(in.nor);
        float3 viewDir     = normalize(-in.positionView);
        float3 lightTravel = normalize(LightDirectionView);
        float  diffuse     = max(dot(normalView, -lightTravel), 0.0f);
        light += LightColor[0].rgb * diffuse;
        if (diffuse > 0.0f && MaterialPower > 0.0f)
        {
            float3 lightDir = normalize(-lightTravel);
            float3 halfDir  = normalize(lightDir + viewDir);
            float  spec     = pow(max(dot(normalView, halfDir), 0.0f), MaterialPower);
            specular       += LightColor[0].rgb * MaterialSpecular.rgb * spec;
        }
    }
    else
    {
        float3 normalView = float3(0.0f, 0.0f, 1.0f);
        if (HasNormalMap != 0)
            normalView = normalize((sample2.sample(samp, in.uv).xyz * 2.0f) - 1.0f);
        for (int i = 0; i < )msl" + supportedMaxLights + R"msl(; ++i)
        {
            if (i >= LightCount) break;
            float3 toLight = LightPositionView[i] - in.positionView;
            float  dist = length(toLight);
            if (LightRadius[i] > 0.0001f)
            {
                float3 lightDir = toLight / max(dist, 0.0001f);
                float  diffuse = max(dot(normalView, lightDir), 0.0f);
                float  attenuation = 1.0f - clamp(dist / LightRadius[i], 0.0f, 1.0f);
                attenuation *= attenuation;
                light += LightColor[i].rgb * diffuse * attenuation;
                if (diffuse > 0.0f && MaterialPower > 0.0f)
                {
                    float3 viewDir = normalize(-in.positionView);
                    float3 halfDir = normalize(lightDir + viewDir);
                    float  spec = pow(max(dot(normalView, halfDir), 0.0f), MaterialPower);
                    specular += LightColor[i].rgb * MaterialSpecular.rgb * spec * attenuation;
                }
            }
        }
    }
    light = clamp(light, 0.0f, 1.0f);
    float3 litColor    = clamp((base * light) + MaterialEmissive.rgb + specular, 0.0f, 1.0f);
    return float4(litColor, texColor.a * MaterialDiffuse.a);
}
)msl";
    }

    static const std::string kLitTexturedPixelShaderMetal = buildLitTexturedPixelShaderMetal();
    static const char* resourceShader[] = {

    /* =========================================================
       PIXEL SHADERS  (fragment function only — no vertex preamble)
       ========================================================= */

    // ---- edge gradient magnitude ----------------------------------------
    "edge gradient magnitude.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv        = in.uv;
    float2 imageSize = float2(f[0], f[1]);
    float  tolerance = f[2];

    float4 col = sample0.sample(samp, uv);
    if (col.a == 0.0f) discard_fragment();

    float2 off = 1.0f / imageSize;
    float2 pr = uv + float2( off.x, 0.0f);
    float2 pl = uv + float2(-off.x, 0.0f);
    float2 pt = uv + float2(0.0f,  off.y);
    float2 pb = uv + float2(0.0f, -off.y);

    float2 grad = float2(
        length(sample0.sample(samp, pr).xyz - sample0.sample(samp, pl).xyz),
        length(sample0.sample(samp, pt).xyz - sample0.sample(samp, pb).xyz));
    float  a = length(grad);
    float4 result = float4(a, a, a, 1.0f);

    if (result.r <= tolerance && result.g <= tolerance && result.b <= tolerance)
        discard_fragment();
    return result;
}
)msl",
    "[edge-gradient-magnitude.ps] = edge gradient magnitude.ps\n"
    "[edge-gradient-magnitude.ps][vector2][imageSize] = min 0 0 max 1024 1024 default 256 256 \n"
    "[edge-gradient-magnitude.ps][float][tolerance] = min 0.0 max 1.0 default 0.0 \n",

    // ---- pie ----------------------------------------------------------------
    "pie.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv                 = in.uv;
    float  clockwise          = f[0];
    float  angle_start_in_deg = f[1];
    float  percent            = f[2];

    // Normalize fragment angle to [0, 1) where 0 = east / 3-o'clock, clockwise.
    float  PI      = 3.14159265358979f;
    float2 c       = uv - 0.5f;
    float  a_rad   = atan2(c.y, c.x);
    float  a01     = fmod(a_rad / (2.0f * PI) + 1.0f, 1.0f);
    float  start01 = fmod(angle_start_in_deg / 360.0f, 1.0f);

    // Arc distance from start to fragment, in requested winding direction.
    float delta;
    if (clockwise >= 0.5f)
        delta = fmod(a01 - start01 + 1.0f, 1.0f);
    else
        delta = fmod(start01 - a01 + 1.0f, 1.0f);

    if (delta < percent)
        return sample0.sample(samp, uv);
    discard_fragment();
    return float4(0.0f);
}
)msl",
    "[ps-pie.ps] = pie.ps\n"
    "[ps-pie.ps][float][clockwise]          = min 0.0    max 1.0   default 1.0 \n"
    "[ps-pie.ps][float][angle_start_in_deg] = min -360.0 max 360.0 default 0.0 \n"
    "[ps-pie.ps][float][percent]            = min 0.0    max 1.0   default 0.5 \n",

    // ---- explosion gaussian -------------------------------------------------
    "explosion gaussian.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv     = in.uv;
    float  sigma  = f[0];
    float2 center = float2(f[1], f[2]);
    float3 color  = float3(f[3], f[4], f[5]);

    float PI = 3.14152f;
    float s  = 2.0f * pow(sigma, 2.0f);
    float q  = 1.0f / (PI * s);
    float a  = q * exp(-(pow(uv.x - center.x, 2.0f) + pow(uv.y - center.y, 2.0f)) / s);
    float alpha = sample0.sample(samp, uv).a;
    return float4(a * color.r, a * color.g, a * color.b, alpha);
}
)msl",
    "[ps-explosion-gaussian.ps] = explosion gaussian.ps\n"
    "[ps-explosion-gaussian.ps][float][sigma] = min -2.0 max 2.0 default 0.15 \n"
    "[ps-explosion-gaussian.ps][vector2][center] = min -1.0 -1.0 max 1.5 1.5 default 0.5 0.5 \n"
    "[ps-explosion-gaussian.ps][rgb][color]  = min 0.0 0.0 0.0 max 1.0 1.0 1.0 default 1.0 1.0 1.0 \n",

    // ---- luminance ----------------------------------------------------------
    "luminance.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float4 color    = float4(f[0], f[1], f[2], f[3]);
    float4 texColor = sample0.sample(samp, in.uv);
    float  lum      = dot(texColor, float4(0.2126f, 0.7152f, 0.0722f, 0.0f));
    float4 white    = float4(1.0f);
    float4 output;
    if (lum < 0.5f)
        output = 2.0f * texColor * color;
    else
        output = white - 2.0f * (white - texColor) * (white - color);
    output.w = texColor.w * color.w;
    return output;
}
)msl",
    "[luminance.ps] = luminance.ps\n"
    "[luminance.ps][rgba][color] = min 0 0 0 0 max 1.0 1.0 1.0 1.0 default 0.5 0.5 0.5 0.5 \n",

    // ---- blend --------------------------------------------------------------
    "blend.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0       [[texture(0)]],
    texture2d<float> sample1       [[texture(1)]],
    sampler          samp          [[sampler(0)]],
    constant float*  f             [[buffer(2)]])
{
    float2 uv             = in.uv;
    float  invertSample   = f[0];
    float  disableSample1 = f[1];
    float3 colorAdd       = float3(f[2], f[3], f[4]);
    float4 junctionRemove = float4(f[5], f[6], f[7], f[8]);

    float4 c0, c1;
    if (invertSample > 0.5f) {
        c0 = sample1.sample(samp, uv);
        c1 = sample0.sample(samp, uv);
    } else {
        c0 = sample0.sample(samp, uv);
        c1 = sample1.sample(samp, uv);
    }
    if (disableSample1 > 0.5f) {
        c0.rgb += colorAdd;
        return c0;
    }
    c1 -= junctionRemove;
    float4 original = c0;
    float4 outColor;
    outColor.w   = c0.w;
    c1.xyz      *= c0.w;
    outColor.xyz = c0.xyz * (1.0f - c1.w) + c1.xyz;
    outColor     = mix(original, outColor, float4(c1.w));
    outColor.xyz += colorAdd;
    return outColor;
}
)msl",
    "[ps-blend.ps] = blend.ps\n"
    "[ps-blend.ps][float][invertSample] = min 0 max 1 default 0 \n"
    "[ps-blend.ps][float][disableSample1] = min 0 max 1 default 0 \n"
    "[ps-blend.ps][rgb][colorAdd] = min 0 0 0 max 255 255 255 default 255 0 0 \n"
    "[ps-blend.ps][rgba][junctionRemove] = min 0 0 0 0 max 255 255 255 255 default 0.2 0.2 0.2 0.0  \n",

    // ---- font ---------------------------------------------------------------
    "font.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float3 colorFont = float3(f[0], f[1], f[2]);
    float4 color     = sample0.sample(samp, in.uv);
    float3 c2        = float3(1.0f - colorFont.r, 1.0f - colorFont.g, 1.0f - colorFont.b);
    color.rgb -= c2;
    return color;
}
)msl",
    "[ps-font.ps] = font.ps\n"
    "[ps-font.ps][rgb][colorFont]             = min 0 0 0   max 1.0 1.0 1.0 default 1.0 1.0 1.0   \n",

    // ---- color keying -------------------------------------------------------
    "color keying.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  granThen  = f[0];
    float  tolerance = f[1];
    float4 colorDst  = float4(f[2], f[3], f[4], f[5]);
    float4 colorSrc  = float4(f[6], f[7], f[8], f[9]);

    float4 color = sample0.sample(samp, in.uv);
    if (color.w == 0.0f) discard_fragment();

    if (granThen > 0.5f) {
        if (all(abs(color.xyz - colorSrc.xyz) < float3(tolerance)))
            color = colorDst;
    } else {
        if (all(abs(color.xyz - colorSrc.xyz) > float3(tolerance)))
            color = colorDst;
    }
    return color;
}
)msl",
    "[ps-color-keying.ps] = color keying.ps\n"
    "[ps-color-keying.ps][float][granThen]             = min 0.0              max 1.0              default 1.0             #boolean\n"
    "[ps-color-keying.ps][float][tolerance]            = min 0.0              max 1.0              default 0.3             #float\n"
    "[ps-color-keying.ps][rgba][colorDst]              = min 0.1 0.2 0.3 0.0  max 1.0 1.0 1.0 1.0  default 1.0 0.2 1.0 1.0 #cor RGBA definida como float\n"
    "[ps-color-keying.ps][rgba][colorSrc]              = min 10 20 30  255    max 125 128 250 255  default 125 128 50 255  #cor RGBA\n",

    // ---- tiled map ----------------------------------------------------------
    "tiled map.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  alpha             = f[0];
    float  enableColorKeying = f[1];
    float  granThen          = f[2];
    float  tolerance         = f[3];
    float3 colorSrc          = float3(f[4], f[5], f[6]);

    float4 color = sample0.sample(samp, in.uv);
    color.a -= alpha;
    if (color.a <= 0.0f) discard_fragment();
    if (enableColorKeying > 0.5f) {
        if (granThen > 0.5f) {
            if (all(abs(color.xyz - colorSrc) < float3(tolerance)))
                discard_fragment();
        } else {
            if (all(abs(color.xyz - colorSrc) > float3(tolerance)))
                discard_fragment();
        }
    }
    return color;
}
)msl",
    "[ps-tiled-map.ps] = tiled map.ps\n"
    "[ps-tiled-map.ps][float][alpha]                = min 0.0        max 1.0          default 0.0\n"
    "[ps-tiled-map.ps][float][enableColorKeying]    = min 0.0        max 1.0          default 0.0\n"
    "[ps-tiled-map.ps][float][granThen]             = min 0.0        max 1.0          default 1.0\n"
    "[ps-tiled-map.ps][float][tolerance]            = min 0.0        max 1.0          default 0.3\n"
    "[ps-tiled-map.ps][rgb][colorSrc]               = min 0 0 0      max 255 255 255  default 255 0 255\n",

    // ---- transparent --------------------------------------------------------
    "transparent.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  alpha = f[0];
    float4 color = sample0.sample(samp, in.uv);
    color.a -= alpha;
    return color;
}
)msl",
    "[ps-transparent.ps] = transparent.ps\n"
    "[ps-transparent.ps][float][alpha]                = min 0.0              max 1.0              default 0.7\n",

    // ---- saturate -----------------------------------------------------------
    "saturate.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float3 color = float3(f[0], f[1], f[2]);
    float4 c1    = sample0.sample(samp, in.uv);
    c1.xyz *= color;
    return c1;
}
)msl",
    "[ps-saturate.ps] = saturate.ps\n"
    "[ps-saturate.ps][rgb][color]                = min 0.0 0.0 0.0           max 1.0 1.0 1.0           default 1.0 1.0 1.0\n",

    // ---- out of bounds ------------------------------------------------------
    "out of bounds.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv       = in.uv;
    float3 color    = float3(f[0], f[1], f[2]);
    float4 colorRet = sample0.sample(samp, uv);
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        colorRet.rgb *= color;
    return colorRet;
}
)msl",
    "[ps-out-of-bounds.ps] = out of bounds.ps\n"
    "[ps-out-of-bounds.ps][rgb][color]           = min 0 0 0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

    // ---- color it -----------------------------------------------------------
    "color it.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  enable = f[0];
    float3 color  = float3(f[1], f[2], f[3]);
    float4 c      = sample0.sample(samp, in.uv);
    if (enable > 0.5f)
        return float4(color.r, color.g, color.b, c.a);
    return c;
}
)msl",
    "[ps-color-it.ps] = color it.ps\n"
    "[ps-color-it.ps][float][enable]        = min 0.0   max 1.0   default 1.0 \n"
    "[ps-color-it.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

    // ---- tint ---------------------------------------------------------------
    "tint.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float3 color = float3(f[0], f[1], f[2]);
    float4 tex   = sample0.sample(samp, in.uv);
    return float4(max(tex.r, color.r), max(tex.g, color.g), max(tex.b, color.b), tex.a);
}
)msl",
    "[ps-tint.ps] = tint.ps\n"
    "[ps-tint.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

    // ---- night vision -------------------------------------------------------
    "night vision.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv                    = in.uv;
    float  fInverseViewportWidth  = f[0];
    float  fInverseViewportHeight = f[1];
    float  iw = fInverseViewportWidth;
    float  ih = fInverseViewportHeight;

    float4 color0 = sample0.sample(samp, uv);
    float4 col    = color0;
    col += 0.0625f * sample0.sample(samp, uv + float2(-iw, -ih));
    col += 0.0625f * sample0.sample(samp, uv + float2(-iw,  ih));
    col += 0.0625f * sample0.sample(samp, uv + float2( iw, -ih));
    col += 0.0625f * sample0.sample(samp, uv + float2( iw,  ih));
    col += 0.125f  * sample0.sample(samp, uv + float2(-iw,  0.0f));
    col += 0.125f  * sample0.sample(samp, uv + float2( iw,  0.0f));
    col += 0.125f  * sample0.sample(samp, uv + float2(0.0f, -ih));
    col += 0.125f  * sample0.sample(samp, uv + float2(0.0f,  ih));
    col += 0.25f   * sample0.sample(samp, uv);
    float lum = 0.299f * col.x + 0.587f * col.y + 0.184f * col.z;
    col = float4(lum, lum, lum, col.w);
    col.y *= 3.0f;
    col = col * color0 * 0.5f;
    return col;
}
)msl",
    "[ps-night-vision.ps] = night vision.ps\n"
    "[ps-night-vision.ps][float][fInverseViewportWidth]   = min 0.00001    max 1.0      default 1.0\n"
    "[ps-night-vision.ps][float][fInverseViewportHeight]  = min 0.00001    max 1.0      default 1.0\n",

    // ---- night vision blur --------------------------------------------------
    "night vision blur.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  brightness = f[0];
    float  contrast   = f[1];
    float4 color      = sample0.sample(samp, in.uv);
    if (color.a == 0.0f) discard_fragment();

    float4 pixelColor = color;
    float4 color0     = pixelColor;
    float  lum = 0.299f * pixelColor.x + 0.587f * pixelColor.y + 0.184f * pixelColor.z;
    pixelColor    = float4(lum, lum, lum, pixelColor.w);
    pixelColor.xyz /= pixelColor.w;
    pixelColor.xyz  = (pixelColor.xyz - 0.5f) * max(contrast, 0.0f) + 0.5f;
    pixelColor.y   += brightness;
    pixelColor.xyz *= pixelColor.w;
    return pixelColor * color0 * 0.25f;
}
)msl",
    "[ps-night-vision-blur.ps] = night vision blur.ps\n"
    "[ps-night-vision-blur.ps][float][brightness]   = min 0.00001    max 100.0     default 10\n"
    "[ps-night-vision-blur.ps][float][contrast]     = min 0.00001    max 10.0      default 6.8\n",

    // ---- multi textura ------------------------------------------------------
    "multi textura.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  gamma      = f[0];
    float4 color1     = sample0.sample(samp, in.uv);
    float4 color2     = sample1.sample(samp, in.uv);
    return clamp(color1 * color2 * gamma, 0.0f, 1.0f);
}
)msl",
    "[ps-multi-textura.ps] = multi textura.ps\n"
    "[ps-multi-textura.ps][float][gamma]    = min 0.0    max 100.0     default 2.0\n",

    // ---- wave ---------------------------------------------------------------
    "wave.ps",
    R"msl(
static float mbm_wave_dist(float a, float b, float c, float d) {
    return sqrt((a - c)*(a - c) + (b - d)*(b - d));
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv         = in.uv;
    float  effectTime = f[0];
    float  sizeWave   = f[1];

    float val = sin(mbm_wave_dist(uv.x + effectTime, uv.y, 0.128f, 0.128f) * sizeWave)
              + sin(mbm_wave_dist(uv.x, uv.y, 0.640f, 0.640f) * sizeWave)
              + sin(mbm_wave_dist(uv.x, uv.y + effectTime / 7.0f, 0.192f, 0.640f) * sizeWave);
    uv += val / sizeWave;
    return sample0.sample(samp, uv);
}
)msl",
    "[ps-wave.ps] = wave.ps\n"
    "[ps-wave.ps][float][effectTime]   = min 0.0    max 10.0      default 0.0\n"
    "[ps-wave.ps][float][sizeWave]     = min -50.0    max 100.0     default 10\n",

    // ---- bands --------------------------------------------------------------
    "bands.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  bandDensity   = f[0];
    float  bandIntensity = f[1];
    float4 color         = sample0.sample(samp, in.uv);
    color.xyz += tan(in.uv.x * bandDensity) * bandIntensity;
    return color;
}
)msl",
    "[ps-bands.ps] = bands.ps\n"
    "[ps-bands.ps][float][bandDensity]   = min 0.0   max 150.0   default 65.0\n"
    "[ps-bands.ps][float][bandIntensity] = min 0.001 max 0.56    default 0.56\n",

    // ---- bloom --------------------------------------------------------------
    "bloom.ps",
    R"msl(
static float3 mbm_adj_sat(float3 col, float sat) {
    float grey = dot(col, float3(0.3f, 0.59f, 0.11f));
    return mix(float3(grey), col, sat);
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  BloomIntensity  = f[0];
    float  BaseIntensity   = f[1];
    float  BloomSaturation = f[2];
    float  BaseSaturation  = f[3];

    float4 color = sample0.sample(samp, in.uv);
    if (color.a == 0.0f) discard_fragment();

    float  BloomThreshold = 0.25f;
    float3 base  = color.xyz / color.w;
    float3 bloom = saturate((base - BloomThreshold) / (1.0f - BloomThreshold));
    bloom = mbm_adj_sat(bloom, BloomSaturation) * BloomIntensity;
    base  = mbm_adj_sat(base,  BaseSaturation)  * BaseIntensity;
    base *= (1.0f - saturate(bloom));
    return float4((base + bloom) * color.w, color.w);
}
)msl",
    "[ps-bloom.ps] = bloom.ps\n"
    "[ps-bloom.ps][float][BloomIntensity]   = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseIntensity]    = min 0.0   max 2.0   default 0.5\n"
    "[ps-bloom.ps][float][BloomSaturation]  = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseSaturation]   = min 0.0   max 2.0   default 0.5\n",

    // ---- bright extract -----------------------------------------------------
    "bright extract.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  threshold     = f[0];
    float4 originalColor = sample0.sample(samp, in.uv);
    float3 rgb           = originalColor.xyz / originalColor.w;
    rgb = saturate((rgb - threshold) / (1.0f - threshold));
    return float4(rgb * originalColor.w, originalColor.w);
}
)msl",
    "[ps-bright-extract.ps] = bright extract.ps\n"
    "[ps-bright-extract.ps][float][threshold]   = min 0.0   max 1.0   default 0.5\n",

    // ---- color tone ---------------------------------------------------------
    "color tone.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  desaturation = f[0];
    float  toned        = f[1];
    float4 lightColor   = float4(f[2], f[3], f[4], f[5]);
    float4 darkColor    = float4(f[6], f[7], f[8], f[9]);

    float4 color = sample0.sample(samp, in.uv);
    if (color.a == 0.0f) discard_fragment();

    float3 scnColor = lightColor.xyz * (color.xyz / color.w);
    float  gray     = dot(float3(0.3f, 0.59f, 0.11f), scnColor);
    float3 muted    = mix(scnColor, float3(gray), desaturation);
    float3 middle   = mix(darkColor, lightColor, gray).xyz;
    scnColor        = mix(muted, middle, toned);
    return float4(scnColor * color.w, color.w);
}
)msl",
    "[ps-color-tone.ps] = color tone.ps\n"
    "[ps-color-tone.ps][float][desaturation] = min 0.0      max 1.0              default 0.5\n"
    "[ps-color-tone.ps][float][toned]        = min 0.0      max 1.0              default 0.5\n"
    "[ps-color-tone.ps][rgba][lightColor]    = min 0 0 0 0  max 255 255 255 255  default 255 255 255 255\n"
    "[ps-color-tone.ps][rgba][darkColor]     = min 0 0 0 0  max 255 255 255 255  default 255 255 0 0.7\n",

    // ---- brightness ---------------------------------------------------------
    "brightness.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  brightness  = f[0];
    float  contrast    = f[1];
    float4 pixelColor  = sample0.sample(samp, in.uv);
    if (pixelColor.a == 0.0f) discard_fragment();
    pixelColor.xyz /= pixelColor.w;
    pixelColor.xyz  = (pixelColor.xyz - 0.5f) * max(contrast, 0.0f) + 0.5f;
    pixelColor.xyz += brightness;
    pixelColor.xyz *= pixelColor.w;
    return pixelColor;
}
)msl",
    "[ps-brightness.ps] = brightness.ps\n"
    "[ps-brightness.ps][float][brightness] = min 0.0      max 1.0              default 0.5\n"
    "[ps-brightness.ps][float][contrast]   = min 0.0      max 2.0              default 1.5\n",

    // ---- blur directional ---------------------------------------------------
    "blur directional.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv         = in.uv;
    float  angle      = f[0];
    float  blurAmount = f[1];
    float  rad        = angle * 0.0174533f;
    float  xOff       = cos(rad);
    float  yOff       = sin(rad);
    float4 c          = float4(0.0f);
    for (int i = 0; i < 16; ++i) {
        uv.x -= blurAmount * xOff;
        uv.y -= blurAmount * yOff;
        c += sample0.sample(samp, uv);
    }
    return c / 16.0f;
}
)msl",
    "[ps-blur-directional.ps] = blur directional.ps\n"
    "[ps-blur-directional.ps][float][angle]      = min 0.0      max 360.0      default 0.0\n"
    "[ps-blur-directional.ps][float][blurAmount] = min 0.000    max 0.01       default 0.000\n",

    // ---- embossed -----------------------------------------------------------
    "embossed.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv     = in.uv;
    float  amount = f[0];
    float  width  = f[1];
    float4 color  = sample0.sample(samp, uv);
    if (color.a == 0.0f) discard_fragment();
    float4 outC = float4(0.5f, 0.5f, 0.5f, 1.0f);
    outC -= sample0.sample(samp, uv - width) * amount;
    outC += sample0.sample(samp, uv + width) * amount;
    float avg  = (outC.x + outC.y + outC.z) / 3.0f;
    outC.xyz   = float3(avg);
    return outC;
}
)msl",
    "[ps-embossed.ps] = embossed.ps\n"
    "[ps-embossed.ps][float][amount]      = min 0.0      max 1.0      default 0.5\n"
    "[ps-embossed.ps][float][width]       = min 0.0      max 0.1      default 0.0022999998\n",

    // ---- frosty out line ----------------------------------------------------
    "frosty out line.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv     = in.uv;
    float  width  = f[0];
    float  height = f[1];
    float4 color  = sample0.sample(samp, uv);
    if (color.a == 0.0f) discard_fragment();

    float rw = 1.0f / width;
    float rh = 1.0f / height;
    float2 tl = float2(uv.x - rw, uv.y - rh);
    float2 t  = float2(uv.x,      uv.y - rh);
    float2 tr = float2(uv.x + rw, uv.y - rh);
    float2 l  = float2(uv.x - rw, uv.y);
    float2 r  = float2(uv.x + rw, uv.y);
    float2 bl = float2(uv.x - rw, uv.y + rh);
    float2 b  = float2(uv.x,      uv.y + rh);
    float2 br = float2(uv.x + rw, uv.y + rh);

    float4 edge = (-sample0.sample(samp, tl) - sample0.sample(samp, t)  - sample0.sample(samp, tr))
                + (-sample0.sample(samp, l)  + 8.0f * sample0.sample(samp, uv) - sample0.sample(samp, r))
                + (-sample0.sample(samp, bl) - sample0.sample(samp, b)  - sample0.sample(samp, br));
    float avg  = (edge.x + edge.y + edge.z) / 3.0f;
    edge.xyz   = float3(avg);
    edge.w     = 1.0f;
    return color + edge;
}
)msl",
    "[ps-frosty-out-line.ps] = frosty out line.ps\n"
    "[ps-frosty-out-line.ps][float][width]      = min 0.0      max 650.0      default 300.0\n"
    "[ps-frosty-out-line.ps][float][height]     = min 0.0      max 500.0      default 300.0\n",

    // ---- glass tile ---------------------------------------------------------
    "glass tile.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv         = in.uv;
    float  tiles      = f[0];
    float  bevelWidth = f[1];
    float  offset     = f[2];
    float4 groutColor = float4(f[3], f[4], f[5], f[6]);

    float a = sample0.sample(samp, uv).a;
    if (a == 0.0f && groutColor.a == 0.0f) discard_fragment();

    float2 newUV = uv + tan(tiles * 2.5f * uv + offset) * (bevelWidth / 100.0f);
    float4 c1    = sample0.sample(samp, newUV);
    if (c1.a == 0.0f && groutColor.a == 0.0f) discard_fragment();
    if (newUV.x < 0.0f || newUV.x > 1.0f || newUV.y < 0.0f || newUV.y > 1.0f)
        c1 = groutColor;
    if (c1.a == 0.0f && groutColor.a == 0.0f) discard_fragment();
    return c1;
}
)msl",
    "[ps-glass-tile.ps] = glass tile.ps\n"
    "[ps-glass-tile.ps][float][tiles]        = min 0.0           max 20.0                 default 5.0\n"
    "[ps-glass-tile.ps][float][bevelWidth]   = min 1.0           max 10.0                 default 10.0\n"
    "[ps-glass-tile.ps][float][offset]       = min 0.0           max 3.0                  default 3.0\n"
    "[ps-glass-tile.ps][rgba][groutColor]    = min 0 0 0 0       max 255 255 255 255      default 0 0 0 0 \n",

    // ---- poisson ------------------------------------------------------------
    "poisson.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv        = in.uv;
    float  poisson   = f[0];
    float2 inputSize = float2(f[1], f[2]);

    float4 cOut = sample0.sample(samp, uv);
    cOut += sample0.sample(samp, uv + (float2(-0.326212f, -0.40581f)  / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2(-0.840144f, -0.07358f)  / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2(-0.695914f,  0.457137f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2(-0.203345f,  0.620716f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.96234f,  -0.194983f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.473434f, -0.480026f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.519456f,  0.767022f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.185461f, -0.893124f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.507431f,  0.064425f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2( 0.89642f,   0.412458f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2(-0.32194f,  -0.932615f) / inputSize) * poisson);
    cOut += sample0.sample(samp, uv + (float2(-0.791559f, -0.59771f)  / inputSize) * poisson);
    return cOut / 13.0f;
}
)msl",
    "[ps-poisson.ps] = poisson.ps\n"
    "[ps-poisson.ps][float][poisson]       = min 1.0           max 10.0                 default 3.0\n"
    "[ps-poisson.ps][vector2][inputSize]   = min 1.0 1.0       max 300.0 300.0        default 100.0 100.0\n",

    // ---- invert color -------------------------------------------------------
    "invert color.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]])
{
    float4 color = sample0.sample(samp, in.uv);
    return float4(color.w - color.xyz, color.w);
}
)msl",
    "[ps-invert-color.ps] = invert color.ps\n",

    // ---- magnifying glass ---------------------------------------------------
    "magnifying glass.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv            = in.uv;
    float2 center        = float2(f[0], f[1]);
    float  radius        = f[2];
    float  magnification = f[3];
    float  aspectRatio   = f[4];

    float4 color = sample0.sample(samp, uv);
    if (color.a == 0.0f) discard_fragment();

    float2 ctp  = uv - center;
    float  dist = length(ctp / float2(1.0f, aspectRatio));
    float2 sp   = (dist < radius) ? (center + ctp / magnification) : uv;
    return sample0.sample(samp, sp);
}
)msl",
    "[ps-magnifying-glass.ps] = magnifying glass.ps\n"
    "[ps-magnifying-glass.ps][vector2][center]       = min 0.0 0.0       max 1.0 1.0             default 0.5 0.5\n"
    "[ps-magnifying-glass.ps][float][radius]         = min 0.0           max 1.0                 default 0.25\n"
    "[ps-magnifying-glass.ps][float][magnification]  = min 0.0           max 5.0                 default 2.0\n"
    "[ps-magnifying-glass.ps][float][aspectRatio]    = min 0.5           max 2.0                 default 0.94\n",

    // ---- old movie ----------------------------------------------------------
    "old movie.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv            = in.uv;
    float  scratchAmount = f[0];
    float  noiseAmount   = f[1];
    float  frame         = f[2];

    float4 color = sample0.sample(samp, uv);
    if (color.a == 0.0f) discard_fragment();

    float  ScratchAmountInv = 1.0f / scratchAmount;
    float2 sc      = frame * float2(0.001f, 0.4f);
    sc.x           = fract(uv.x + sc.x);
    float  scratch = sc.x;
    scratch        = 2.0f * scratch * ScratchAmountInv;
    scratch        = 1.0f - abs(1.0f - scratch);
    scratch        = max(0.0f, scratch);
    color.xyz     += float3(scratch);

    float2 rCoord = uv * 0.33f;
    float3 rand   = sample0.sample(samp, rCoord).xyz;
    if (noiseAmount > rand.x)
        color.xyz = float3(0.1f + rand.z * 0.4f);

    float  gray = dot(color, float4(0.3f, 0.59f, 0.11f, 0.0f));
    color        = float4(gray * float3(0.9f, 0.8f, 0.6f), 1.0f);
    float2 d2    = 0.5f - uv;
    color.xyz   *= (0.4f - dot(d2, d2)) * 2.8f;
    return color;
}
)msl",
    "[ps-old-movie.ps] = old movie.ps\n"
    "[ps-old-movie.ps][float][scratchAmount]       = min 0.00001     max 0.1             default 0.044\n"
    "[ps-old-movie.ps][float][noiseAmount]         = min 0.00001     max 1.0             default 0.0001\n"
    "[ps-old-movie.ps][float][frame]               = min 0.0         max 2.0             default 1.0\n",

    // ---- pinch mouse --------------------------------------------------------
    "pinch mouse.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv          = in.uv;
    float2 center      = float2(f[0], f[1]);
    float  radius      = f[2];
    float  strength    = f[3];
    float  aspectRatio = f[4];

    float2 newCenter;
    newCenter.x = 1.0f - (center.x / 794.0f);
    newCenter.y = 1.0f - (center.y / 678.0f);
    float2 dir      = newCenter - uv;
    float2 scaledDir = dir;
    scaledDir.y    /= aspectRatio;
    float  dist     = length(scaledDir);
    float  range    = saturate(1.0f - (dist / (abs(-sin(radius * 8.0f) * radius) + 1.0e-8f)));
    float2 sp       = uv + (dir * range) * strength;
    return sample0.sample(samp, sp);
}
)msl",
    "[ps-pinch-mouse.ps] = pinch mouse.ps\n"
    "[ps-pinch-mouse.ps][vector2][center]         = min 0.0 0.0     max 794.0 678.0  default 0.5 0.5\n"
    "[ps-pinch-mouse.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
    "[ps-pinch-mouse.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
    "[ps-pinch-mouse.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",

    // ---- pinch --------------------------------------------------------------
    "pinch.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv          = in.uv;
    float2 center      = float2(f[0], f[1]);
    float  radius      = f[2];
    float  strength    = f[3];
    float  aspectRatio = f[4];

    float2 dir       = center - uv;
    float2 scaledDir = dir;
    scaledDir.y     /= aspectRatio;
    float  dist  = length(scaledDir);
    float  range = saturate(1.0f - (dist / (abs(-sin(radius * 8.0f) * radius) + 1.0e-8f)));
    float2 sp    = uv + (dir * range) * strength;
    return sample0.sample(samp, sp);
}
)msl",
    "[ps-pinch.ps] = pinch.ps\n"
    "[ps-pinch.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
    "[ps-pinch.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
    "[ps-pinch.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
    "[ps-pinch.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",

    // ---- ripple -------------------------------------------------------------
    "ripple.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv          = in.uv;
    float2 center      = float2(f[0], f[1]);
    float  amplitude   = f[2];
    float  frequency   = f[3];
    float  phase       = f[4];
    float  aspectRatio = f[5];

    float2 dir = uv - center;
    dir.y     /= aspectRatio;
    float  dist    = length(dir);
    dir           /= dist;
    dir.y         *= aspectRatio;
    float  waveX   = sin(frequency * dist + phase);
    float  waveY   = cos(frequency * dist + phase);
    float  falloff = saturate(1.0f - dist);
    falloff       *= falloff;
    dist          += amplitude * waveX * falloff;
    float2 sp      = center + dist * dir;
    float4 color   = sample0.sample(samp, sp);
    float  lighting = 1.0f - (amplitude * 0.2f * (1.0f - saturate(waveY * falloff)));
    color.xyz      *= lighting;
    return color;
}
)msl",
    "[ps-ripple.ps] = ripple.ps\n"
    "[ps-ripple.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
    "[ps-ripple.ps][float][amplitude]        = min 0.0         max 1.0          default 0.1\n"
    "[ps-ripple.ps][float][frequency]        = min 0.0         max 100.0        default 70.0\n"
    "[ps-ripple.ps][float][phase]            = min -20.0       max 20.0         default 0.0\n"
    "[ps-ripple.ps][float][aspectRatio]      = min 0.5         max 2.0          default 1.34\n",

    // ---- sharpen ------------------------------------------------------------
    "sharpen.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv        = in.uv;
    float2 inputSize = float2(f[0], f[1]);
    float  amount    = f[2];
    float2 offset    = 1.0f / inputSize;
    float4 color     = sample0.sample(samp, uv);
    color.xyz += float3(sample0.sample(samp, uv - offset) * amount);
    color.xyz -= float3(sample0.sample(samp, uv + offset) * amount);
    return color;
}
)msl",
    "[ps-sharpen.ps] = sharpen.ps\n"
    "[ps-sharpen.ps][vector2][inputSize]     = min 1.0 1.0     max 1000.0 1000.0      default 800.0 600.0\n"
    "[ps-sharpen.ps][float][amount]          = min 0.0         max 2.0                default 1.0 \n",

    // ---- sketch -------------------------------------------------------------
    "sketch.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv        = in.uv;
    float  brushSize = f[0];

    const float2 k_s1 = float2( 0.0f, -1.0f);
    const float2 k_s2 = float2(-1.0f,  0.0f);
    const float2 k_s3 = float2( 1.0f,  0.0f);

    float4 color   = sample0.sample(samp, uv);
    float4 laplace = -4.0f * color;

    laplace += sample0.sample(samp, uv + brushSize * k_s1);
    laplace.y = laplace.x; laplace.z = laplace.x;

    laplace += sample0.sample(samp, uv + brushSize * k_s2);
    laplace.y = laplace.x; laplace.z = laplace.x;

    laplace += sample0.sample(samp, uv + brushSize * k_s3);
    laplace.y = laplace.x; laplace.z = laplace.x;

    laplace += sample0.sample(samp, uv + brushSize * k_s2);
    laplace.y = laplace.x; laplace.z = laplace.x;

    laplace = 1.0f / laplace;
    float4 complement;
    complement.xyz = 1.0f - laplace.xyz;
    complement.w   = color.w;

    if (complement.x > 1.0f) {
        float gray = complement.x * 0.3f + complement.y * 0.59f + complement.z * 0.11f;
        return float4(gray, gray, gray, complement.w);
    }
    float gray = color.x * 0.3f + color.y * 0.59f + color.z * 0.11f;
    return float4(gray, gray, gray, color.w);
}
)msl",
    "[ps-sketch.ps] = sketch.ps\n"
    "[ps-sketch.ps][float][brushSize]          = min 0.0006         max 1.0                default 0.003 \n",

    // ---- smooth magnify -----------------------------------------------------
    "smooth magnify.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv            = in.uv;
    float2 center        = float2(f[0], f[1]);
    float  innerRadius   = f[2];
    float  outerRadius   = f[3];
    float  magnification = f[4];
    float  aspectRatio   = f[5];

    float2 ctp   = uv - center;
    float  dist  = length(ctp / float2(1.0f, aspectRatio));
    float  ratio = smoothstep(innerRadius, max(innerRadius, outerRadius), dist);
    float2 sp    = mix(center + ctp / magnification, uv, float2(ratio));
    return sample0.sample(samp, sp);
}
)msl",
    "[ps-smooth-magnify.ps] = smooth magnify.ps\n"
    "[ps-smooth-magnify.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-smooth-magnify.ps][float][innerRadius]       = min 0.0         max 1.0                default 0.2 \n"
    "[ps-smooth-magnify.ps][float][outerRadius]       = min 0.0         max 1.0                default 0.4 \n"
    "[ps-smooth-magnify.ps][float][magnification]     = min 0.0         max 5.0                default 2.0 \n"
    "[ps-smooth-magnify.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",

    // ---- spiral -------------------------------------------------------------
    "spiral.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv             = in.uv;
    float2 center         = float2(f[0], f[1]);
    float  spiralStrength = f[2];
    float  aspectRatio    = f[3];

    float2 dir     = uv - center;
    dir.y         /= aspectRatio;
    float  dist    = length(dir);
    float  angle   = atan2(dir.y, dir.x);
    float  newAngle = angle + spiralStrength * dist;
    float2 newDir  = float2(cos(newAngle), sin(newAngle));
    newDir.y      *= aspectRatio;
    float2 sp      = center + newDir * dist;
    bool   valid   = all(sp >= float2(0.0f)) && all(sp <= float2(1.0f));
    return valid ? sample0.sample(samp, sp) : float4(0.0f);
}
)msl",
    "[ps-spiral.ps] = spiral.ps\n"
    "[ps-spiral.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-spiral.ps][float][spiralStrength]    = min 0.0         max 20.0               default 10.0 \n"
    "[ps-spiral.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",

    // ---- tone mapping -------------------------------------------------------
    "tone mapping.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv             = in.uv;
    float2 vignetteCenter = float2(f[0], f[1]);
    float  defog          = f[2];
    float  exposure       = f[3];
    float  gamma          = f[4];
    float  vignetteRadius = f[5];
    float  vignetteAmount = f[6];
    float  blueShift      = f[7];
    float4 fogColor       = float4(f[8], f[9], f[10], f[11]);

    float4 c = sample0.sample(samp, uv);
    c.xyz    = max(float3(0.0f), c.xyz - defog * fogColor.xyz);
    c.xyz   *= pow(2.0f, exposure);
    c.xyz    = pow(c.xyz, float3(gamma));
    float2 tc = uv - vignetteCenter;
    float  v  = length(tc) / vignetteRadius;
    c.xyz    += pow(v, 4.0f) * vignetteAmount;
    float3 d  = c.xyz * float3(1.05f, 0.97f, 1.27f);
    c.xyz     = mix(c.xyz, d, float3(blueShift));
    return c;
}
)msl",
    "[ps-tone-mapping.ps] = tone mapping.ps\n"
    "[ps-tone-mapping.ps][vector2][vignetteCenter]  = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
    "[ps-tone-mapping.ps][float][defog]             = min 0.0         max 1.0                default 0.4 \n"
    "[ps-tone-mapping.ps][float][exposure]          = min -1.0        max 1.0                default -0.2 \n"
    "[ps-tone-mapping.ps][float][gamma]             = min 0.5         max 2.0                default 0.63 \n"
    "[ps-tone-mapping.ps][float][vignetteRadius]    = min 0.0         max 1.0                default 0.5 \n"
    "[ps-tone-mapping.ps][float][vignetteAmount]    = min -1.0        max 1.0                default 0.0 \n"
    "[ps-tone-mapping.ps][float][blueShift]         = min 0.0         max 10.0               default 10.0 \n"
    "[ps-tone-mapping.ps][rgba][fogColor]           = min 0 0 0 0     max 255 255 255 255    default 255 255 255 255 \n",

    // ---- toon ---------------------------------------------------------------
    "toon.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float  levels = f[0];
    float4 color  = sample0.sample(samp, in.uv);
    if (color.a == 0.0f) discard_fragment();
    color.xyz   /= color.w;
    int    result = int(floor(levels));
    color.xyz   *= float(result);
    color.xyz    = floor(color.xyz);
    color.xyz   /= float(result);
    color.xyz   *= color.w;
    return color;
}
)msl",
    "[ps-toon.ps] = toon.ps\n"
    "[ps-toon.ps][float][levels]         = min 0.0         max 15.0               default 5.0 \n",

    // ---- fade ---------------------------------------------------------------
    "fade.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv       = in.uv;
    float  progress = f[0];
    float4 c0       = sample0.sample(samp, uv);
    float4 c1       = sample1.sample(samp, uv);
    return mix(c1, c0, float4(progress / 100.0f));
}
)msl",
    "[ps-fade.ps] = fade.ps\n"
    "[ps-fade.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",

    // ---- fade radial --------------------------------------------------------
    "fade radial.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv    = in.uv;
    float  prog  = f[0] / 100.0f;
    float4 color = sample0.sample(samp, uv);
    if (color.w == 0.0f) return color;

    float2 center    = float2(0.5f, 0.5f);
    float2 normToUV  = uv - center;
    float  s         = prog * 0.02f;
    float4 c1        = float4(0.0f);
    for (int i = 0; i < 24; ++i)
        c1 += sample1.sample(samp, uv - (normToUV * s * float(i)));
    c1 /= 24.0f;
    return mix(c1, color, float4(prog));
}
)msl",
    "[ps-fade-radial.ps] = fade radial.ps\n"
    "[ps-fade-radial.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",

    // ---- fade ripple --------------------------------------------------------
    "fade ripple.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv            = in.uv;
    float  prog          = f[0] / 100.0f;
    float  frequency     = 20.0f;
    float  speed         = 10.0f;
    float  amplitude     = 0.05f;
    float2 center        = float2(0.5f, 0.5f);
    float2 toUV          = uv - center;
    float  distFromCenter = length(toUV);
    float2 normToUV      = toUV / distFromCenter;
    float  wave          = cos(frequency * distFromCenter - speed * prog);
    float2 newUV1 = center + normToUV * (distFromCenter + prog        * wave * amplitude);
    float2 newUV2 = center + normToUV * (distFromCenter + (1.0f-prog) * wave * amplitude);
    float4 c1     = sample1.sample(samp, newUV1);
    float4 c2     = sample0.sample(samp, newUV2);
    return mix(c1, c2, float4(prog));
}
)msl",
    "[ps-fade-ripple.ps] = fade ripple.ps\n"
    "[ps-fade-ripple.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",

    // ---- fade saturate ------------------------------------------------------
    "fade saturate.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv    = in.uv;
    float  prog  = f[0] / 100.0f;
    float4 color = sample0.sample(samp, uv);
    float4 c1    = sample1.sample(samp, uv);
    c1           = saturate(c1 * (2.0f * prog + 1.0f));
    if (prog > 0.8f) {
        float new_prog = (prog - 0.8f) * 5.0f;
        return mix(c1, color, float4(new_prog));
    }
    return c1;
}
)msl",
    "[ps-fade-saturate.ps] = fade saturate.ps\n"
    "[ps-fade-saturate.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",

    // ---- fade twist ---------------------------------------------------------
    "fade twist.ps",
    R"msl(
static inline float4 mbm_swb(float4 border, texture2d<float> tex, sampler samp, float2 uv) {
    float2 d = saturate(uv) - uv;
    if (any(d != float2(0.0f))) return border;
    return tex.sample(samp, uv);
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv          = in.uv;
    float  prog        = f[0] / 100.0f;
    float  twistAmount = f[1];
    float4 color       = sample0.sample(samp, uv);
    float2 center      = float2(0.5f, 0.5f);
    float2 toUV        = uv - center;
    float  dist        = length(toUV);
    float2 normToUV    = toUV / dist;
    float  angle       = atan2(normToUV.y, normToUV.x);
    angle             += dist * dist * twistAmount * prog;
    float2 newUV       = float2(cos(angle), sin(angle)) * dist + center;
    float4 c1          = mbm_swb(float4(0.0f), sample1, samp, newUV);
    return mix(c1, color, float4(prog));
}
)msl",
    "[ps-fade-twist.ps] = fade twist.ps\n"
    "[ps-fade-twist.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
    "[ps-fade-twist.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",

    // ---- fade twist grid ----------------------------------------------------
    "fade twist grid.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv          = in.uv;
    float  prog        = f[0] / 100.0f;
    float  twistAmount = f[1];
    float4 color       = sample0.sample(samp, uv);
    float  cellsize    = 0.1f;
    float2 cell        = floor(uv * 10.0f);
    float2 oddeven     = fmod(cell, float2(2.0f));
    float  cellTwist   = twistAmount;
    if (oddeven.x < 1.0f) cellTwist *= -1.0f;
    if (oddeven.y < 1.0f) cellTwist *= -1.0f;
    float2 newUV       = fract(uv * 10.0f);
    float2 center      = float2(0.5f, 0.5f);
    float2 toUV        = newUV - center;
    float  dist        = length(toUV);
    float2 normToUV    = toUV / dist;
    float  angle       = atan2(normToUV.y, normToUV.x);
    angle             += dist * dist * cellTwist * prog;
    float2 newUV2      = float2(cos(angle), sin(angle)) * dist + center;
    newUV2            *= cellsize;
    newUV2            += cell * cellsize;
    float4 c1          = sample1.sample(samp, newUV2);
    return mix(c1, color, float4(prog));
}
)msl",
    "[ps-fade-twist-grid.ps] = fade twist grid.ps\n"
    "[ps-fade-twist-grid.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
    "[ps-fade-twist-grid.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",

    // ---- fade wave ----------------------------------------------------------
    "fade wave.ps",
    R"msl(
static inline float4 mbm_swb_fw(float4 border, texture2d<float> tex, sampler samp, float2 uv) {
    float2 d = saturate(uv) - uv;
    if (any(d != float2(0.0f))) return border;
    return tex.sample(samp, uv);
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv    = in.uv;
    float  prog  = f[0] / 100.0f;
    float4 color = sample0.sample(samp, uv);
    float  mag   = 0.1f;
    float  phase = 14.0f;
    float  freq  = 20.0f;
    float2 newUV = uv + float2((mag * prog) * sin(freq * uv.y + phase * prog), 0.0f);
    float4 c1    = mbm_swb_fw(float4(0.0f), sample1, samp, newUV);
    return mix(c1, color, float4(prog));
}
)msl",
    "[ps-fade-wave.ps] = fade wave.ps\n"
    "[ps-fade-wave.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",

    // ---- blur zoom ----------------------------------------------------------
    "blur zoom.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv         = in.uv;
    float2 center     = float2(f[0], f[1]);
    float  blurAmount = f[2];
    float4 c          = float4(0.0f);
    uv -= center;
    for (int i = 0; i < 15; ++i) {
        float  scale = 1.0f + blurAmount * (float(i) / 14.0f);
        c += sample0.sample(samp, uv * scale + center);
    }
    return c / 15.0f;
}
)msl",
    "[ps-blur-zoom.ps] = blur zoom.ps\n"
    "[ps-blur-zoom.ps][vector2][center]           = min 0.0 0.0     max 1.0 1.0             default 0.62 0.67 \n"
    "[ps-blur-zoom.ps][float][blurAmount]         = min 0.0         max 2.0                 default 0.2 \n",

    // ---- texture map --------------------------------------------------------
    "texture map.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    texture2d<float> sample1 [[texture(1)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 uv               = in.uv;
    float  horizontalSize   = f[0];
    float  verticalSize     = f[1];
    float  horizontalOffset = f[2];
    float  verticalOffset   = f[3];
    float  strength         = f[4];

    float  ho     = fract(uv.x / horizontalSize + min(1.0f, horizontalOffset));
    float  vo     = fract(uv.y / verticalSize   + min(1.0f, verticalOffset));
    float2 offset = (sample1.sample(samp, float2(ho, vo)).xy * strength) - (strength / 8.0f);
    return sample0.sample(samp, fract(uv + offset));
}
)msl",
    "[ps-texture-map.ps] = texture map.ps\n"
    "[ps-texture-map.ps][float][horizontalSize]       = min 0.0         max 5.0                 default 1.0 \n"
    "[ps-texture-map.ps][float][verticalSize]         = min 0.0         max 5.0                 default 1.0 \n"
    "[ps-texture-map.ps][float][horizontalOffset]     = min 0.0         max 1.0                 default 0.0 \n"
    "[ps-texture-map.ps][float][verticalOffset]       = min 0.0         max 1.0                 default 0.0 \n"
    "[ps-texture-map.ps][float][strength]             = min 0.0         max 1.0                 default 0.3 \n",

    // ---- lit textured ------------------------------------------------------
    "lit textured.ps",
    kLitTexturedPixelShaderMetal.c_str(),
    "[ps-lit-textured.ps] = lit textured.ps\n",

    // ---- lit solid ---------------------------------------------------------
    "lit solid.ps",
    R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    constant int    &LightEnabled      [[buffer(4)]],
    constant float4 &AmbientColor      [[buffer(6)]],
    constant float3 &LightDirectionView [[buffer(7)]],
    constant float4 &LightColor        [[buffer(8)]],
    constant float4 &MaterialDiffuse   [[buffer(9)]],
    constant float4 &MaterialAmbient   [[buffer(10)]],
    constant float4 &MaterialSpecular  [[buffer(11)]],
    constant float4 &MaterialEmissive  [[buffer(12)]],
    constant float  &MaterialPower     [[buffer(13)]],
    constant int    &LightMode         [[buffer(14)]])
{
    float4 baseColor = float4(1.0f);
    if (LightEnabled == 0 || LightMode != 1)
        return baseColor;
    float3 normalView  = normalize(in.nor);
    float3 viewDir     = normalize(-in.positionView);
    float3 lightTravel = normalize(LightDirectionView);
    float  diffuse     = max(dot(normalView, -lightTravel), 0.0f);
    float3 base        = MaterialDiffuse.rgb;
    float3 light       = clamp((AmbientColor.rgb * MaterialAmbient.rgb) + (LightColor.rgb * diffuse), 0.0f, 1.0f);
    float3 specular    = float3(0.0f);
    if (diffuse > 0.0f && MaterialPower > 0.0f)
    {
        float3 lightDir = normalize(-lightTravel);
        float3 halfDir  = normalize(lightDir + viewDir);
        float  spec     = pow(max(dot(normalView, halfDir), 0.0f), MaterialPower);
        specular        = LightColor.rgb * MaterialSpecular.rgb * spec;
    }
    float3 litColor    = clamp((base * light) + MaterialEmissive.rgb + specular, 0.0f, 1.0f);
    return float4(litColor, MaterialDiffuse.a);
}
)msl",
    "[ps-lit-solid.ps] = lit solid.ps\n",

    /* =========================================================
       VERTEX SHADERS  (complete MSL programs: vertex + fragment)
       ========================================================= */

    // ---- simple texture (VS entry — complete MSL program) -------------------
    "simple texture.vs",
    R"msl(
#include <metal_stdlib>
using namespace metal;
struct MbmUniforms { float4x4 mvp; float4x4 mv; float4 color; };
struct VIn  { float3 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct VOut { float4 pos [[position]];     float2 uv; };

vertex VOut vert_main(VIn in [[stage_in]],
    constant MbmUniforms& u [[buffer(1)]])
{
    VOut o;
    o.pos = u.mvp * float4(in.pos, 1.0f);
    o.uv  = in.uv;
    return o;
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]])
{
    return sample0.sample(samp, in.uv);
}
)msl",
    "[vs-simple-texture.vs] = simple texture.vs\n",

    // ---- scale (VS entry — complete MSL program) ----------------------------
    "scale.vs",
    R"msl(
#include <metal_stdlib>
using namespace metal;
struct MbmUniforms { float4x4 mvp; float4x4 mv; float4 color; };
struct VIn  { float3 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct VOut { float4 pos [[position]];     float2 uv; };

vertex VOut vert_main(VIn in [[stage_in]],
    constant MbmUniforms& u  [[buffer(1)]],
    constant float*        vf [[buffer(3)]])
{
    float3 scale = float3(vf[0], vf[1], vf[2]);
    float4 pos   = float4(in.pos * scale, 1.0f);
    VOut o;
    o.pos = u.mvp * pos;
    o.uv  = in.uv;
    return o;
}

fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]])
{
    return sample0.sample(samp, in.uv);
}
)msl",
    "[vs-scale.vs] = scale.vs\n"
    "[vs-scale.vs][vector3][scale]       = min 0.0 0.0 0.0         max 10.0  10.0 10.0                default 1.0 1.0 1.0\n",

    /* sentinel */
    nullptr, nullptr, nullptr
    };

    const char** getShaderEngineBuiltIn()
    {
        return resourceShader;
    }

    // ---- LINE_MESH shaders (internal, not in resource table) ----------------
    // PS: compileShader() will prepend buildVertexHeader(fvf) before this code.

    const char* getCodePScolorFor_LINE_MESH()
    {
        // Fragment-only: outputs the flat colour from buffer(2) f[0..3].
        static const char code[] = R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    constant float* f [[buffer(2)]])
{
    return float4(f[0], f[1], f[2], f[3]);
}
)msl";
        return code;
    }

    const char* getCodeVScolorFor_LINE_MESH()
    {
        // Not used — compileShader() prioritises ptrPshader when both are set.
        static const char code[] = "";
        return code;
    }

    // ---- Particle shaders ---------------------------------------------------
    // The '?' placeholder is replaced by the blend operator (e.g. '*').
    // The '#' placeholder is replaced by optional extra code lines.

    const char* getParticlePSCode()
    {
        static const char code[] = R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float4 color               = float4(f[0], f[1], f[2], f[3]);
    float  enableAlphaFromColor = f[4];
    float4 texColor            = sample0.sample(samp, in.uv);
    float4 outColor;
    if (enableAlphaFromColor > 0.5f)
        outColor.a = color.a;
    else
        outColor.a = texColor.a;
    outColor.rgb = color.rgb ? texColor.rgb;
    #
    return outColor;
}
)msl";
        return code;
    }

    const char* getParticleVSCode()
    {
        // Not used — compileShader() uses ptrPshader + buildVertexHeader.
        static const char code[] = "";
        return code;
    }

    // ---- Steered-particle shaders -------------------------------------------

    const char* getSteeredParticlePSCode(bool hasColor)
    {
        if (hasColor)
        {
            static const char codeColor[] = R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float4 color    = float4(f[0], f[1], f[2], f[3]);
    float4 texColor = sample0.sample(samp, in.uv);
    return color * texColor;
}
)msl";
            return codeColor;
        }
        else
        {
            static const char codeNoColor[] = R"msl(
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> sample0 [[texture(0)]],
    sampler          samp    [[sampler(0)]])
{
    return sample0.sample(samp, in.uv);
}
)msl";
            return codeNoColor;
        }
    }

    const char* getSteeredParticleVSCode()
    {
        // Not used — compileShader() uses ptrPshader + buildVertexHeader.
        static const char code[] = "";
        return code;
    }

    static std::string PS_Version("metal_ps");
    static std::string VS_Version("metal_vs");

    const char* getPSVersion() { return PS_Version.c_str(); }
    const char* getVSVersion() { return VS_Version.c_str(); }

    void setPSVersion(const char* version)
    {
        PS_Version = version ? version : "";
    }
    void setVSVersion(const char* version)
    {
        VS_Version = version ? version : "";
    }

} // namespace mbm

#endif // USE_METAL
