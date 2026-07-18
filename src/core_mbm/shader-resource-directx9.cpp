/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined (USE_DIRECTX9)

#include <core_mbm/core-exports.h>
#include <core_mbm/light.h>
#include <string>
#include <stdio.h>

namespace mbm
{
    static std::string buildLitTexturedPixelShaderD3D9()
    {
        const std::string supportedMaxLights = std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS);
        return
            "int LightEnabled;\n"
            "int LightMode;\n"
            "int HasNormalMap;\n"
            "float4 AmbientColor;\n"
            "float3 LightDirectionView;\n"
            "float3 LightPositionView[" + supportedMaxLights + "];\n"
            "float LightRadius[" + supportedMaxLights + "];\n"
            "float4 LightColor[" + supportedMaxLights + "];\n"
            "int LightCount;\n"
            "float4 MaterialDiffuse;\n"
            "float4 MaterialAmbient;\n"
            "float4 MaterialSpecular;\n"
            "float4 MaterialEmissive;\n"
            "float MaterialPower;\n"
            "sampler2D TextureDiffuse : register(s0);\n"
            "sampler2D TextureNormal : register(s2);\n"
            "\n"
            "float4 main(float2 texCoord : TEXCOORD0, float3 normalViewIn : TEXCOORD1, float3 positionViewIn : TEXCOORD2) : COLOR\n"
            "{\n"
            "    float4 texColor = tex2D(TextureDiffuse, texCoord);\n"
            "    if (LightEnabled == 0 || LightMode == 0)\n"
            "        return texColor;\n"
            "    float3 base = texColor.rgb * MaterialDiffuse.rgb;\n"
            "    float3 light = AmbientColor.rgb * MaterialAmbient.rgb;\n"
            "    float3 specular = float3(0, 0, 0);\n"
            "    if (LightMode == 1)\n"
            "    {\n"
            "        float3 normalView = normalize(normalViewIn);\n"
            "        float3 viewDir = normalize(-positionViewIn);\n"
            "        float3 lightTravel = normalize(LightDirectionView);\n"
            "        float diffuse = max(dot(normalView, -lightTravel), 0);\n"
            "        light += LightColor[0].rgb * diffuse;\n"
            "        if (diffuse > 0.0f && MaterialPower > 0.0f)\n"
            "        {\n"
            "            float3 lightDir = normalize(-lightTravel);\n"
            "            float3 halfDir = normalize(lightDir + viewDir);\n"
            "            float spec = pow(max(dot(normalView, halfDir), 0), MaterialPower);\n"
            "            specular += LightColor[0].rgb * MaterialSpecular.rgb * spec;\n"
            "        }\n"
            "    }\n"
            "    else\n"
            "    {\n"
            "        float3 normalView = float3(0, 0, 1);\n"
            "        if (HasNormalMap != 0) normalView = normalize((tex2D(TextureNormal, texCoord).xyz * 2.0f) - 1.0f);\n"
            "        for (int i = 0; i < " + supportedMaxLights + "; ++i)\n"
            "        {\n"
            "            if (i >= LightCount) break;\n"
            "            float3 toLight = LightPositionView[i] - positionViewIn;\n"
            "            float dist = length(toLight);\n"
            "            if (LightRadius[i] > 0.0001f)\n"
            "            {\n"
            "                float3 lightDir = toLight / max(dist, 0.0001f);\n"
            "                float diffuse = max(dot(normalView, lightDir), 0);\n"
            "                float attenuation = 1.0f - saturate(dist / LightRadius[i]);\n"
            "                attenuation *= attenuation;\n"
            "                light += LightColor[i].rgb * diffuse * attenuation;\n"
            "                if (diffuse > 0.0f && MaterialPower > 0.0f)\n"
            "                {\n"
            "                    float3 viewDir = normalize(-positionViewIn);\n"
            "                    float3 halfDir = normalize(lightDir + viewDir);\n"
            "                    float spec = pow(max(dot(normalView, halfDir), 0), MaterialPower);\n"
            "                    specular += LightColor[i].rgb * MaterialSpecular.rgb * spec * attenuation;\n"
            "                }\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "    light = saturate(light);\n"
            "    float3 litColor = saturate((base * light) + MaterialEmissive.rgb + specular);\n"
            "    return float4(litColor, texColor.a * MaterialDiffuse.a);\n"
            "}\n";
    }

    static const std::string kLitTexturedPixelShaderD3D9 = buildLitTexturedPixelShaderD3D9();
    static const char* resourceShader[] = {// Organized in groups of 3: filename, shader code, and CFG configurations.


        /* Alpha It -----------------------------------------------------------------------------------------------------*/

        "edge gradient magnitude.ps",

        "sampler2D TextureDiffuse : register(s0);\n"
        "float2 imageSize;\n"
        "float tolerance;\n"
        "\n"
        "struct PS_INPUT\n"
        "{\n"
        "    float2 vTexCoord : TEXCOORD0;\n"
        "};\n"
        "\n"
        "float4 xlat_main(in float2 uv) \n"
        "{\n"
        "    float2 offsetTexture;\n"
        "    float2 pixel_Right;\n"
        "    float2 pixel_Left;\n"
        "    float2 pixel_Top;\n"
        "    float2 pixel_Bottom;\n"
        "    float2 gradient;\n"
        "    float a;\n"
        "\n"
        "    offsetTexture = (1.00000 / imageSize);\n"
        "    pixel_Right = (uv.xy + float2(offsetTexture.x, 0.000000));\n"
        "    pixel_Left = (uv.xy + float2((-offsetTexture.x), 0.000000));\n"
        "    pixel_Top = (uv.xy + float2(0.000000, offsetTexture.y));\n"
        "    pixel_Bottom = (uv.xy + float2(0.000000, (-offsetTexture.y)));\n"
        "    gradient = float2(length((tex2D(TextureDiffuse, pixel_Right).xyz - tex2D(TextureDiffuse, pixel_Left).xyz)), length((tex2D(TextureDiffuse, pixel_Top).xyz - tex2D(TextureDiffuse, pixel_Bottom).xyz)));\n"
        "    a = length(gradient);\n"
        "    return float4(a, a, a, 1.00000);\n"
        "}\n"
        "\n"
        "float4 main(PS_INPUT input) : COLOR0\n"
        "{\n"
        "   float a = tex2D(TextureDiffuse, input.vTexCoord).a;\n"
        "   if (a == 0.0)\n"
        "   {\n"
        "       clip(-1);\n"
        "       return float4(0, 0, 0, 0);\n"
        "   }\n"
        "    else\n"
        "    {\n"
        "        float4 color = xlat_main(input.vTexCoord);\n"
        "        if(color.r <= tolerance && color.g <= tolerance && color.b <= tolerance)\n"
        "        {\n"
        "           clip(-1);\n"
        "           return float4(0, 0, 0, 0);\n"
        "        }\n"
        "        else\n"
        "           return xlat_main(input.vTexCoord);\n"
        "    }\n"
        "}\n",

        "[edge-gradient-magnitude.ps] = edge gradient magnitude.ps\n"
        "[edge-gradient-magnitude.ps][vector2][imageSize] = min 0 0 max 1024 1024 default 256 256 \n"
        "[edge-gradient-magnitude.ps][float][tolerance] = min 0.0 max 1.0 default 0.0 \n",

        //AlphaIt *********************
        "alpharit.ps",

        "float alpha : register(C0); \n"
        "float ray : register(C1); \n"
        "float2 center : register(C2); \n"
        "float2 prop : register(C3); \n"
        "sampler2D TextureDiffuse : register(S0); \n"
        "float4 main(float2 uv : TEXCOORD) : COLOR \n"
        "{ \n"
        "	float4 color = tex2D(TextureDiffuse, uv); \n"
        "	float2 v2    = float2((uv.x - center.x) / prop.x,(uv.y - center.y) / prop.y) ;\n"
        "	float dist   = length(v2); \n"
        "	if(dist < ray) \n"
        "	{\n"
        "		color.a -= alpha; \n"
        "	}\n"
        "	return color; \n"
        "}",

        "[ps-alpharit.ps] = alpharit.ps\n"
        "[ps-alpharit.ps][float][ray] = min 0.000000 max 1.000000 default 0.05000000\n"
        "[ps-alpharit.ps][vector2][center] = min 0.000000 0.000000 max 1.000000 1.000000 default 0.500000 0.500000\n"
        "[ps-alpharit.ps][vector2][prop] = min 0.000000 0.000000 max 100.000000 100.000000 default 1.000000 1.33333\n"
        "[ps-alpharit.ps][float][alpha] = min 0.000000 max 1.000000 default 1.000000",

        // tint **********************
        "tint.ps",

        "sampler2D TextureDiffuse : register(s0);\n"
        "float3 color;\n"
        "struct PS_INPUT\n"
        "{\n"
        "   float2 vTexCoord : TEXCOORD0;\n"
        "};\n"
        "\n"
        "float4 main(PS_INPUT input) : COLOR0\n"
        "{\n"
        "   float4 tex = tex2D(TextureDiffuse, input.vTexCoord.xy);\n"
        "   return float4(max(tex.r, color.r), max(tex.g, color.g), max(tex.b, color.b), tex.a);\n"
        "}\n",

        "[ps-tint.ps] = tint.ps\n"
        "[ps-tint.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",

        // color it **********************
        "color it.ps",

        "sampler2D TextureDiffuse : register(S0);\n"
        "float3 color : register(C0);\n"
        "float enable : register(C1);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR \n"
        "{\n"
        "   float4 c = tex2D( TextureDiffuse, uv.xy );\n"
        "   if(enable > 0.5)\n"
        "      return float4(color.r,color.g,color.b,c.a);\n"
        "   else\n"
        "      return c;\n"
        "}\n",

        "[ps-color-it.ps] = color it.ps\n"
        "[ps-color-it.ps][float][enable]        = min 0.0   max 1.0   default 1.0 \n"
        "[ps-color-it.ps][rgb][color]           = min 0.0 0.0 0.0     max 1.0 1.0 1.0     default 1.0 0.0 0.0 \n",


        "pie.ps",

        "sampler2D TextureDiffuse : register(s0);\n"
        "float clockwise          : register(c0);\n"
        "float angle_start_in_deg : register(c1);\n"
        "float percent            : register(c2);\n"
        "\n"
        "struct PS_INPUT\n"
        "{\n"
        "    float2 vTexCoord : TEXCOORD0;\n"
        "};\n"
        "\n"
        "float4 main(PS_INPUT input) : COLOR0\n"
        "{\n"
        "    // Normalize fragment angle to [0, 1) where 0 = east / 3-o'clock, clockwise.\n"
        "    float2 c       = input.vTexCoord - 0.5;\n"
        "    float  a_rad   = atan2(c.y, c.x);\n"
        "    float  a01     = fmod(a_rad / (2.0 * 3.14159265) + 1.0, 1.0);\n"
        "    float  start01 = fmod(angle_start_in_deg / 360.0, 1.0);\n"
        "\n"
        "    // Arc distance from start to fragment, in requested winding direction.\n"
        "    float delta;\n"
        "    if (clockwise >= 0.5)\n"
        "        delta = fmod(a01 - start01 + 1.0, 1.0);\n"
        "    else\n"
        "        delta = fmod(start01 - a01 + 1.0, 1.0);\n"
        "\n"
        "    if (delta < percent)\n"
        "        return tex2D(TextureDiffuse, input.vTexCoord);\n"
        "    else\n"
        "        return float4(0, 0, 0, 0);\n"
        "}\n",

        "[ps-pie.ps] = pie.ps\n"
        "[ps-pie.ps][float][clockwise]          = min 0.0    max 1.0   default 1.0 \n"
        "[ps-pie.ps][float][angle_start_in_deg] = min -360.0 max 360.0 default 0.0 \n"
        "[ps-pie.ps][float][percent]            = min 0.0    max 1.0   default 0.5 \n",

        // pie *********************
            
        // luminance *********************

        "luminance.ps",

        "sampler2D TextureDiffuse : register(s0);\n"
        "float4 color;\n"
        "\n"
        "struct PS_INPUT\n"
        "{\n"
        "    float2 vTexCoord : TEXCOORD0;\n"
        "};\n"
        "\n"
        "float4 xlat_main(in float2 uv) \n"
        "{\n"
        "    float4 texColor;\n"
        "    float luminance;\n"
        "    float4 xlat_var_output;\n"
        "    float4 white = float4(1.00000, 1.00000, 1.00000, 1.00000);\n"
        "\n"
        "    texColor = tex2D(TextureDiffuse, uv);\n"
        "    luminance = dot(texColor, float4(0.212600, 0.715200, 0.0722000, 0.000000));\n"
        "    xlat_var_output = float4(0.000000, 0.000000, 0.000000, 0.000000);\n"
        "    if ((luminance < 0.500000)){\n"
        "        xlat_var_output = ((2.00000 * texColor) * color);\n"
        "    }\n"
        "    else{\n"
        "        xlat_var_output = (white - ((2.00000 * (white - texColor)) * (white - color)));\n"
        "    }\n"
        "    xlat_var_output.w = (texColor.w * color.w);\n"
        "    return xlat_var_output;\n"
        "}\n"
        "\n"
        "float4 main(PS_INPUT input) : COLOR0\n"
        "{\n"
        "    return xlat_main(input.vTexCoord);\n"
        "}\n",

        "[luminance.ps] = luminance.ps\n"
        "[luminance.ps][rgba][color] = min 0 0 0 0 max 1.0 1.0 1.0 1.0 default 0.5 0.5 0.5 0.5 \n",

        //blend *********************
        "blend.ps",

        "sampler2D TextureDiffuse : register(S0);\n"
        "sampler2D TextureAnimationEffect : register(S1);\n"
        "float3 colorAdd : register(C0);\n"
        "float4 junctionRemove : register(c1);\n"
        "float invertSample : register(C2);\n"
        "float disableSample1 : register(C3);\n\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 c0,c1;\n"
        "	float4 output;\n"
        "	float4 original;\n"
        "	if (invertSample > 0.5)\n"
        "	{\n"
        "		c0 = tex2D(TextureAnimationEffect, uv.xy);//TextureAnimationEffect precisa ter alpha\n"
        "		c1 = tex2D(TextureDiffuse, uv.xy);//TextureDiffuse nao precisa ter alpha\n"
        "	}\n"
        "	else\n"
        "	{\n"
        "		c0 = tex2D(TextureDiffuse, uv.xy);//TextureDiffuse nao precisa ter alpha\n"
        "		c1 = tex2D(TextureAnimationEffect, uv.xy);//TextureAnimationEffect precisa ter alpha\n"
        "	}\n"
        "	if(disableSample1 > 0.5)\n"
        "	{\n"
        "		c0.rgb += colorAdd;\n"
        "		return c0;\n"
        "	}\n"
        "	c1 -= junctionRemove;\n"
        "	original = c0;\n"
        "	output.a = c0.a; \n"
        "	c1.rgb = c1.rgb * c0.a;\n"
        "	output.rgb = ((c0.rgb * (1 - c1.a)) + c1.rgb); \n"
        "	output = lerp(original,output,c1.a);\n"
        "	output.rgb += colorAdd;\n"
        "	return output;\n"
        "}",

        "[ps-blend.ps] = blend.ps\n"
        "[ps-blend.ps][float][invertSample] = min 0 max 1 default 0 \n"
        "[ps-blend.ps][float][disableSample1] = min 0 max 1 default 0 \n"
        "[ps-blend.ps][rgb][colorAdd] = min 0 0 0 max 255 255 255 default 255 0 0 \n"
        "[ps-blend.ps][rgba][junctionRemove] = min 0 0 0 0 max 255 255 255 255 default 0.2 0.2 0.2 0.0  \n",
        //blend *********************



        //font *********************
        "font.ps",

        "float3 colorFont			: register(C0);\n"
        "sampler2D TextureDiffuse	: register(S0);\n"

        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D( TextureDiffuse, uv );\n"
        "	float3 c2 = float3(1.0 - colorFont.r,1.0 - colorFont.g,1.0 - colorFont.b);\n"
        "	color.rgb -= c2;\n"
        "	return color;\n"
        "}\n",

        "[ps-font.ps] = font.ps\n"
        "[ps-font.ps][rgb][colorFont]             = min 0 0 0   max 1.0 1.0 1.0 default 1.0 1.0 1.0   \n",
        //Font *********************

        //Color Keying *********************
        "color keying.ps",

        "float4 colorSrc : register(C0);\n"
        "float4 colorDst : register(C1);\n"
        "float tolerance : register(C2);\n"
        "float granThen : register(C3);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	if (color.a == 0.0)\n"
        "		return color;\n"
        "	if (granThen > 0.5f)\n"
        "	{\n"
        "		if (all(abs(color.rgb - colorSrc.rgb) < tolerance))\n"
        "		{\n"
        "			color.rgba = colorDst;\n"
        "		}\n"
        "	}\n"
        "	else\n"
        "	{\n"
        "		if (all(abs(color.rgb - colorSrc.rgb) > tolerance))\n"
        "		{\n"
        "			color.rgba = colorDst;\n"
        "		}\n"
        "	}\n"
        "	return color;\n"
        "}\n",

        "[ps-color-keying.ps] = color keying.ps\n"
        "[ps-color-keying.ps][float][granThen]             = min 0.0              max 1.0              default 1.0             #boolean\n"
        "[ps-color-keying.ps][float][tolerance]            = min 0.0              max 1.0              default 0.3             #float\n"
        "[ps-color-keying.ps][rgba][colorDst]              = min 0.1 0.2 0.3 1.0  max 1.0 1.0 1.0 1.0  default 1.0 0.0 1.0 1.0 #cor RGBA definida como float\n"
        "[ps-color-keying.ps][rgba][colorSrc]              = min 10 20 30  255    max 125 128 250 255  default 125 128 50 255  #cor RGBA\n",
        //Color Keying *********************

        //transparent *********************
        "transparent.ps",

        "float alpha : register(C0);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	color.a -= alpha;\n"
        "	return color;\n"
        "}\n",

        "[ps-transparent.ps] = transparent.ps\n"
        "[ps-transparent.ps][float][alpha]                = min 0.0              max 1.0              default 1.0\n",
        //transparent *********************

        //outline *************************
        "outline.ps",

        "float3 color : register(C0);\n"
        "float thickness : register(C1);\n"
        "float4 main(float3 normalView : TEXCOORD0, float3 positionView : TEXCOORD1) : COLOR\n"
        "{\n"
        "    float facing = abs(dot(normalize(normalView), normalize(-positionView)));\n"
        "    if (facing > thickness) discard;\n"
        "    return float4(color, 1.0);\n"
        "}\n",

        "[ps-outline.ps] = outline.ps\n"
        "[ps-outline.ps][rgb][color] = min 0.0 0.0 0.0 max 1.0 1.0 1.0 default 1.0 0.9 0.1\n"
        "[ps-outline.ps][float][thickness] = min 0.01 max 0.5 default 0.12\n",
        //outline *************************

        //saturate *********************
        "saturate.ps",

        "float3 color : register(C0);\n"
        "\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 c1;\n"
        "	c1 = tex2D(TextureDiffuse, uv.xy);\n"
        "	c1.rgb *= color.rgb;\n"
        "	return c1;\n"
        "}\n",

        "[ps-saturate.ps] = saturate.ps\n"
        "[ps-saturate.ps][rgb][color]                = min 0.0 0.0 0.0           max 1.0 1.0 1.0           default 1.0 1.0 1.0\n",
        //saturate *********************

        //Night Vision **********************
        "night vision.ps",

        "sampler TextureDiffuse : register(s0);\n"
        "float fInverseViewportWidth;\n"
        "float fInverseViewportHeight;\n"
        "const float4 samples[9] =\n"
        "{\n"
        "	-1.0,\n"
        "	-1.0,\n"
        "	0,\n"
        "	1.0 / 16.0,\n"
        "	-1.0,\n"
        "	1.0,\n"
        "	0,\n"
        "	1.0 / 16.0,\n"
        "	1.0,\n"
        "	-1.0,\n"
        "	0,\n"
        "	1.0 / 16.0,\n"
        "	1.0,\n"
        "	1.0,\n"
        "	0,\n"
        "	1.0 / 16.0,\n"
        "	-1.0,\n"
        "	0.0,\n"
        "	0,\n"
        "	2.0 / 16.0,\n"
        "	1.0,\n"
        "	0.0,\n"
        "	0,\n"
        "	2.0 / 16.0,\n"
        "	0.0,\n"
        "	-1.0,\n"
        "	0,\n"
        "	2.0 / 16.0,\n"
        "	0.0,\n"
        "	1.0,\n"
        "	0,\n"
        "	2.0 / 16.0,\n"
        "	0.0,\n"
        "	0.0,\n"
        "	0,\n"
        "	4.0 / 16.0\n"
        "};\n"
        "\n"
        "float4 main(float2 uv : TEXCOORD0, float4 color0 : COLOR0) : COLOR\n"
        "{\n"
        "	float4 col = tex2D(TextureDiffuse, uv);\n"
        "	for (int i = 0; i < 9; ++i)\n"
        "	{\n"
        "		float2 offset = float2(samples[i].x * fInverseViewportWidth, samples[i].y * fInverseViewportHeight);\n"
        "		float4 newColor = tex2D(TextureDiffuse, uv + offset);\n"
        "		col += samples[i].w * newColor;\n"
        "	}\n"
        "	col = 0.299 * col.r + 0.587 * col.g + 0.184 * col.b;\n"
        "	col = float4(col.xxx, col.a);\n"
        "	col.g *= 3;\n"
        "	col = col * color0 * 0.5f;\n"
        "	return col;\n"
        "}\n"
        "\n",

        "[ps-night-vision.ps] = night vision.ps\n"
        "[ps-night-vision.ps][float][fInverseViewportWidth]   = min 0.00001    max 1.0      default 1.0\n"
        "[ps-night-vision.ps][float][fInverseViewportHeight]  = min 0.00001    max 1.0      default 1.0\n",
        //Night Vision **********************

        //Night Vision blur **********************
        "night vision blur.ps",

        "sampler TextureDiffuse;\n"
        "float brightness : register(C0);\n"
        "float contrast : register(C1);\n"
        "float4 main(float2 texCoord : TEXCOORD0) : COLOR0\n"
        "{\n"
        "	float4 pixelColor = tex2D(TextureDiffuse, texCoord);\n"
        "	float4 color0 = pixelColor;\n"
        "	pixelColor = 0.299 * pixelColor.r + 0.587 * pixelColor.g + 0.184 * pixelColor.b;\n"
        "	pixelColor.rgb /= pixelColor.a;\n"
        "	pixelColor.rgb = ((pixelColor.rgb - 0.5f) * max(contrast, 0)) + 0.5f;\n"
        "	pixelColor.g += brightness;\n"
        "	pixelColor.rgb *= pixelColor.a;\n"
        "	return pixelColor * color0 * 0.25f;\n"
        "}\n",

        "[ps-night-vision-blur.ps] = night vision blur.ps\n"
        "[ps-night-vision-blur.ps][float][brightness]   = min 0.00001    max 100.0     default 10\n"
        "[ps-night-vision-blur.ps][float][contrast]     = min 0.00001    max 10.0      default 6.8\n",
        //Night Vision blur **********************

        //Multi textura **********************
        "multi textura.ps",

        "float gamma : register(C0);\n"

        "sampler2D TextureDiffuse;\n"
        "sampler2D TextureAnimationEffect;\n"
        "\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color1;\n"
        "	float4 color2;\n"
        "	float4 blendColor;\n"
        "\n"
        "	// Get the pixel color from the first texture.\n"
        "	color1 = tex2D(TextureDiffuse, uv.xy);\n"
        "\n"
        "	// Get the pixel color from the second texture.\n"
        "	color2 = tex2D(TextureAnimationEffect, uv.xy);\n"
        "\n"
        "	// Blend the two pixels together and multiply by the gamma value.\n"
        "	blendColor = color1 * color2 * gamma;\n"
        "	\n"
        "	// Saturate the final color.\n"
        "	blendColor = saturate(blendColor);\n"
        "\n"
        "	return blendColor;\n"
        "}\n",

        "[ps-multi-textura.ps] = multi textura.ps\n"
        "[ps-multi-textura.ps][float][gamma]   	= min 0.0    max 100.0     default 2.0\n",
        //Multi Textura **********************

        //Wave **********************
        "wave.ps",

        "sampler2D TextureDiffuse : register(s0);\n"
        "float effectTime : register(C0);\n"
        "float sizeWave: register(C1);\n"
        "float dist(float a, float b, float c, float d)\n"
        "{\n"
        "   return sqrt((a - c) * (a - c) + (b - d) * (b - d));\n"
        "}\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0\n"
        "{   \n"
        "   float4 Color = 0;\n"
        "   float f = sin(dist(uv.x + effectTime, uv.y, 0.128, 0.128)*sizeWave)\n"
        "				  + sin(dist(uv.x, uv.y, 0.64, 0.64)*sizeWave)\n"
        "				  + sin(dist(uv.x, uv.y + effectTime / 7, 0.192, 0.64)*sizeWave);\n"
        "  uv.xy = uv.xy+((f/sizeWave));\n"
        "   Color= tex2D( TextureDiffuse , uv.xy);\n"
        "   return Color;   \n"
        "}\n",

        "[ps-wave.ps] = wave.ps\n"
        "[ps-wave.ps][float][effectTime]   = min 0.0    max 10.0      default 0.0\n"
        "[ps-wave.ps][float][sizeWave]     = min 0.0    max 100.0     default 10\n",
        //Wave **********************

        //Bands **********************
        "bands.ps",

        "float bandDensity : register(C0);\n"
        "float bandIntensity : register(C1);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color;\n"
        "	color = tex2D(TextureDiffuse, uv.xy);\n"
        "	color.rgb += tan(uv.x * bandDensity) * bandIntensity;\n"
        "	return color;\n"
        "}\n",

        "[ps-bands.ps] = bands.ps\n"
        "[ps-bands.ps][float][bandDensity]   = min 0.0   max 150.0   default 65.0\n"
        "[ps-bands.ps][float][bandIntensity] = min 0.001 max 0.56    default 0.56\n",
        //Bands **********************

        //Bloom **********************
        "bloom.ps",

        "float BloomIntensity : register(C0);\n"
        "float BaseIntensity : register(C1);\n"
        "float BloomSaturation : register(C2);\n"
        "float BaseSaturation : register(C3);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float3 AdjustSaturation(float3 color, float saturation)\n"
        "{\n"
        "	float grey = dot(color, float3(0.3, 0.59, 0.11));\n"
        "	return lerp(grey, color.rgb, saturation);\n"
        "}\n"

        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float BloomThreshold = 0.25f;\n"

        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	float3 base = color.rgb / color.a;\n"
        "	float3 bloom = saturate((base - BloomThreshold) / (1 - BloomThreshold));\n"
        // Adjust color saturation and intensity.
    "	bloom = AdjustSaturation(bloom, BloomSaturation) * BloomIntensity;\n"
    "	base = AdjustSaturation(base, BaseSaturation) * BaseIntensity;\n"
        // Darken down the base image in areas where there is a lot of bloom,
        // to prevent things looking excessively burned-out.
    "	base *= (1 - saturate(bloom));\n"
        // Combine the two images.
    "	return float4((base + bloom) * color.a, color.a);\n"
    "}\n",

    "[ps-bloom.ps] = bloom.ps\n"
    "[ps-bloom.ps][float][BloomIntensity]   = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseIntensity]    = min 0.0   max 2.0   default 0.5\n"
    "[ps-bloom.ps][float][BloomSaturation]  = min 0.0   max 2.0   default 1.0\n"
    "[ps-bloom.ps][float][BaseSaturation]   = min 0.0   max 2.0   default 0.5\n",
        //Bloom **********************

        //Bright Extract **********************
        "bright extract.ps",

        "float threshold : register(C0);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 originalColor = tex2D(TextureDiffuse, uv);\n"
        // Undo pre-multiplied alpha.
    "	float3 rgb = originalColor.rgb / originalColor.a;\n"
        // Adjust RGB to keep only values brighter than the specified threshold.
    "	rgb = saturate((rgb - threshold) / (1 - threshold));\n"
        // Re-apply alpha.
    "	return float4(rgb * originalColor.a, originalColor.a);\n"
    "}\n",

    "[ps-bright-extract.ps] = bright extract.ps\n"
    "[ps-bright-extract.ps][float][threshold]   = min 0.0   max 1.0   default 0.5\n",
        //Bright Extract **********************

        //Color Tone **********************
        "color tone.ps",

        "float desaturation : register(C0);\n"
        "float toned : register(C1);\n"
        "float4 lightColor : register(C2);\n"
        "float4 darkColor : register(C3);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	float3 scnColor = lightColor * (color.rgb / color.a);\n"
        "	float gray = dot(float3(0.3, 0.59, 0.11), scnColor);\n"

        "	float3 muted = lerp(scnColor, gray.xxx, desaturation);\n"
        "	float3 middle = lerp(darkColor, lightColor, gray);\n"

        "	scnColor = lerp(muted, middle, toned);\n"
        "	return float4(scnColor * color.a, color.a);\n"
        "}",

        "[ps-color-tone.ps] = color tone.ps\n"
        "[ps-color-tone.ps][float][desaturation] = min 0.0      max 1.0              default 0.5\n"
        "[ps-color-tone.ps][float][toned]        = min 0.0      max 1.0              default 0.5\n"
        "[ps-color-tone.ps][rgba][lightColor]    = min 0 0 0 0  max 255 255 255 255  default 255 255 255 255\n"
        "[ps-color-tone.ps][rgba][darkColor]     = min 0 0 0 0  max 255 255 255 255  default 255 255 0 0.7\n",
        //Color Tone **********************

        //Brightness **********************
        "brightness.ps",

        "float brightness : register(C0);\n"
        "float contrast : register(C1);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 pixelColor = tex2D(TextureDiffuse, uv);\n"
        "	pixelColor.rgb /= pixelColor.a;\n"
        // Apply contrast.
    "	pixelColor.rgb = ((pixelColor.rgb - 0.5f) * max(contrast, 0)) + 0.5f;\n"
        // Apply brightness.
    "	pixelColor.rgb += brightness;\n"
        // Return final pixel color.
    "	pixelColor.rgb *= pixelColor.a;\n"
    "	return pixelColor;\n"
    "}\n",

    "[ps-brightness.ps] = brightness.ps\n"
    "[ps-brightness.ps][float][brightness] = min 0.0      max 1.0              default 0.5\n"
    "[ps-brightness.ps][float][contrast]   = min 0.0      max 2.0              default 1.5\n",
        //Brightness **********************

        //Blur Directional **********************
        "blur directional.ps",

        "float angle : register(C0);\n"
        "float blurAmount : register(C1);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	float4 c = 0;\n"
        "	float rad = angle * 0.0174533f;\n"
        "	float xOffset = cos(rad);\n"
        "	float yOffset = sin(rad);\n"
        "	for (int i = 0; i < 16; ++i)\n"
        "	{\n"
        "		uv.x = uv.x - blurAmount * xOffset;\n"
        "		uv.y = uv.y - blurAmount * yOffset;\n"
        "		c += tex2D(TextureDiffuse, uv);\n"
        "	}\n"
        "	c /= 16;\n"
        "	return c;\n"
        "}\n",

        "[ps-blur-directional.ps] = blur directional.ps\n"
        "[ps-blur-directional.ps][float][angle]      = min 0.0      max 360.0      default 0.0\n"
        "[ps-blur-directional.ps][float][blurAmount] = min 0.000    max 0.01       default 0.000\n",
        //Blur Direcional **********************

        //Embossed **********************
        "embossed.ps",

        "float amount : register(C0);\n"
        "float width : register(C1);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 color = tex2D(TextureDiffuse, uv);\n"
        "	if (color.a == 0.0)\n"
        "		return color;\n"
        "	float4 outC =\n"
        "	{\n"
        "		0.5,\n"
        "		0.5,\n"
        "		0.5,\n"
        "		1.0\n"
        "	};\n"
        "	outC -= tex2D(TextureDiffuse, uv - width) * amount;\n"
        "	outC += tex2D(TextureDiffuse, uv + width) * amount;\n"
        "	outC.rgb = (outC.r + outC.g + outC.b) / 3.0f;\n"
        "	return outC;\n"
        "}\n",

        "[ps-embossed.ps] = embossed.ps\n"
        "[ps-embossed.ps][float][amount]      = min 0.0      max 1.0      default 0.5\n"
        "[ps-embossed.ps][float][width]       = min 0.0      max 0.1      default 0.0022999998\n",
        //Embossed **********************

        //Frosty out line **********************
        "frosty out line.ps",

        "float width : register(C0);\n"
        "float height : register(C1);\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 middle : TEXCOORD) : COLOR\n"
        "{\n"
        "	float2 topLeft;\n"
        "	float2 left;\n"
        "	float2 bottomLeft;\n"
        "	float2 top;\n"
        "	float2 bottom;\n"
        "	float2 topRight;\n"
        "	float2 right;\n"
        "	float2 bottomRight;\n"
        "	topLeft.x = middle.x - 1/width;\n"
        "	topLeft.y = middle.y - 1/height;\n"
        "	top.x = middle.x;\n"
        "	top.y = middle.y - 1/height;\n"
        "	topRight.x = middle.x + 1/width;\n"
        "	topRight.y = middle.y - 1/height;\n"
        "	left.x = middle.x - 1/width;\n"
        "	left.y = middle.y;\n"
        "	right.x = middle.x + 1/width;\n"
        "	right.y = middle.y;\n"
        "	bottomLeft.x = middle.x - 1/width;\n"
        "	bottomLeft.y = middle.y + 1/height;\n"
        "	bottom.x = middle.x;\n"
        "	bottom.y = middle.y + 1/height;\n"
        "	bottomRight.x = middle.x + 1/width;\n"
        "	bottomRight.y = middle.y + 1/height;\n"

        "	float4 m = tex2D (TextureDiffuse , middle);\n"
        "	float4 tl = tex2D (TextureDiffuse, topLeft);\n"
        "	float4 l = tex2D (TextureDiffuse, left);\n"
        "	float4 bl = tex2D (TextureDiffuse, bottomLeft);\n"
        "	float4 t = tex2D (TextureDiffuse, top);\n"
        "	float4 b = tex2D (TextureDiffuse, bottom);\n"
        "	float4 tr = tex2D (TextureDiffuse, topRight);\n"
        "	float4 r = tex2D (TextureDiffuse, right);\n"
        "	float4 br = tex2D (TextureDiffuse, bottomRight);\n"

        "	float4 color = (-tl-t-tr) + (-l+8*m-r) + (-bl-b-br);\n"
        "	float4 color2 = tex2D(TextureDiffuse,middle);\n"
        "	float avg=color.r+color.g+color.b;\n"
        "	avg/=3;\n"
        "	color.rgb=avg;\n"
        "	color.a = 1;\n"
        "	return color2+color;\n"
        "}\n",

        "[ps-frosty-out-line.ps] = frosty out line.ps\n"
        "[ps-frosty-out-line.ps][float][width]      = min 0.0      max 650.0      default 300.0\n"
        "[ps-frosty-out-line.ps][float][height]     = min 0.0      max 500.0      default 300.0\n",
        //Frosty Out Line **********************

        //Glass Tile **********************
        "glass tile.ps",

        "float tiles : register(C0);\n"
        "float bevelWidth : register(C1);\n"
        "float4 groutColor : register(C2);\n"
        "float offset: register(C3);\n"
        "sampler2D TextureDiffuse : register(s0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float2 newUV1;\n"
        "	newUV1.xy = uv.xy + tan((tiles*2.5)*uv.xy + offset)*(bevelWidth/100);\n"
        "	float4 c1 = tex2D(TextureDiffuse, newUV1); \n"
        "	if(newUV1.x<0 || newUV1.x>1 || newUV1.y<0 || newUV1.y>1)\n"
        "	{\n"
        "		c1 = groutColor;\n"
        "	}\n"
        "	c1.a=1;\n"
        "	return c1;\n"
        "}\n",

        "[ps-glass-tile.ps] = glass tile.ps\n"
        "[ps-glass-tile.ps][float][tiles]        = min 0.0           max 20.0                 default 5.0\n"
        "[ps-glass-tile.ps][float][bevelWidth]   = min 1.0           max 10.0                 default 300.0\n"
        "[ps-glass-tile.ps][float][offset]       = min 0.0           max 3.0                  default 300.0\n"
        "[ps-glass-tile.ps][rgba][groutColor]    = min 0 0 0 0       max 255 255 255 255      default 0 0 0 0 \n",
        //Glass Tile **********************

        //Poisson **********************
        "poisson.ps",

        "float poisson : register(C0);\n"
        "float2 inputSize : register(C1);\n"

        "static const float2 poissonArray[12] = \n"
        "{\n"
        "		float2(-0.326212f, -0.40581f),\n"
        "		float2(-0.840144f, -0.07358f),\n"
        "		float2(-0.695914f, 0.457137f),\n"
        "		float2(-0.203345f, 0.620716f),\n"
        "		float2(0.96234f, -0.194983f),\n"
        "		float2(0.473434f, -0.480026f),\n"
        "		float2(0.519456f, 0.767022f),\n"
        "		float2(0.185461f, -0.893124f),\n"
        "		float2(0.507431f, 0.064425f),\n"
        "		float2(0.89642f, 0.412458f),\n"
        "		float2(-0.32194f, -0.932615f),\n"
        "		float2(-0.791559f, -0.59771f)\n"
        "};\n"
        "sampler2D TextureDiffuse : register(S0);\n"
        "float4 main(float2 uv : TEXCOORD) : COLOR\n"
        "{\n"
        "	float4 cOut;\n"
        // center tap
    "	cOut = tex2D(TextureDiffuse, uv);\n"
    "	for(int tap = 0; tap < 12; tap++)\n"
    "	{\n"
    "		float2 coord= uv.xy + (poissonArray[tap] / inputSize * poisson);\n"
        // Sample pixel
"		cOut += tex2D(TextureDiffuse, coord);\n"
"	}\n"

"	return(cOut / 13.0f);\n"
"}\n",

"[ps-poisson.ps] = poisson.ps\n"
"[ps-poisson.ps][float][poisson]       = min 1.0           max 10.0                 default 3.0\n"
"[ps-poisson.ps][vector2][inputSize]   = min 1.0 1.0       max 1000.0 1000.0        default 600.0 400.0\n",
//Poisson **********************

//Invert Color **********************
"invert color.ps",

"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"   float4 color = tex2D( TextureDiffuse, uv );\n"
"   float4 invertedColor = float4(color.a - color.rgb, color.a);\n"
"   return invertedColor;\n"
"}\n",

"[ps-invert-color.ps] = invert color.ps\n",
//Invert Color **********************

//out of bounds **********************
"out of bounds.ps",

"sampler2D TextureDiffuse : register(S0);\n"
"float3 color : register(C1);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"   float4 colorRet = tex2D( TextureDiffuse, uv );\n"
"   if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)\n"
"      colorRet.rgb *= color.rgb;\n"
"   return colorRet;\n"
"}\n",

"[ps-out-of-bounds.ps] = out of bounds.ps\n"
"[ps-out-of-bounds.ps][rgb][color]           = min 0 0 0     max 1.0 0.0 1.0     default 1.0 0 0 \n",
//out of bounds **********************

//Light streak **********************
"light streak.ps",

"float brightThreshold : register(C0);\n"
"float scale : register(C1);\n"
"float attenuation : register(C2);\n"
"float2 direction : register(C3);\n"
"float2 inputSize : register(C4);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	const static int numSamples = 2;\n"
"	int Iteration = 1;\n"

"	float4 pixelColor = tex2D(TextureDiffuse, uv);\n"
"	float3 rgb = pixelColor.rgb / pixelColor.a;\n"
"	float3 bright = saturate((rgb - brightThreshold) / (1 - brightThreshold));\n"

"	rgb += bright;\n"

"	float weightIter = pow(numSamples, Iteration);\n"

"	for (int sample = 0; sample < numSamples; sample++)\n"
"	{\n"
"		float weight = pow(attenuation, weightIter * sample);\n"
"		float2 texCoord = uv + (direction * weightIter * float2(sample, sample) / inputSize);\n"
"		float4 sampleColor = tex2D(TextureDiffuse, texCoord);\n"
"		rgb += saturate(weight) * sampleColor.rgb / sampleColor.a;\n"
"	}\n"

"	return float4(rgb * scale * pixelColor.a, pixelColor.a);\n"
"}\n",

"[ps-light-streak.ps] = light streak.ps\n"
"[ps-light-streak.ps][float][brightThreshold]  = min 0.0           max 1.0                 default 0.5\n"
"[ps-light-streak.ps][float][scale]            = min 0.0           max 1.0                 default 0.5\n"
"[ps-light-streak.ps][vector2][direction]      = min -1.0 -1.0     max 1.0 1.0             default 0.5 1.0\n"
"[ps-light-streak.ps][vector2][attenuation]    = min 1.0 1.0       max 1000.0 1000.0       default 800.0 600.0\n",
//Light streak **********************

//Magnifying glass **********************
"magnifying glass.ps",

"float2 center : register(C0);\n"
"float radius : register(C1);\n"
"float magnification : register(C2);\n"
"float aspectRatio : register(C4);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	float2 centerToPixel = uv - center;\n"
"	float dist = length(centerToPixel / float2(1, aspectRatio));\n"
"	float2 samplePoint = uv;\n"
"	if (dist < radius)\n"
"	{\n"
"		samplePoint = center + centerToPixel / magnification;\n"
"	}\n"
"	return tex2D(TextureDiffuse, samplePoint);\n"
"}\n",

"[ps-magnifying-glass.ps] = magnifying glass.ps\n"
"[ps-magnifying-glass.ps][vector2][center]       = min 0.0 0.0       max 1.0 1.0             default 0.5 0.5\n"
"[ps-magnifying-glass.ps][float][radius]         = min 0.0           max 1.0                 default 0.25\n"
"[ps-magnifying-glass.ps][float][magnification]  = min 0.0           max 5.0                 default 2.0\n"
"[ps-magnifying-glass.ps][float][aspectRatio]    = min 0.5           max 2.0                 default 1.33\n",
//Light streak **********************

//Old Movie **********************
"old movie.ps",

"float scratchAmount : register(C0);\n"
"float noiseAmount : register(C1);\n"
"float frame : register(C4);\n"
"static float ScratchAmountInv = 1.0 / scratchAmount;\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	float2 sc = frame * float2(0.001f, 0.4f);\n"
"	sc.x = frac(uv.x + sc.x);\n"
"	float scratch = sc.r;\n"
"	scratch = 2 * scratch * ScratchAmountInv;\n"
"	scratch = 1 - abs(1 - scratch);\n"
"	scratch = max(0, scratch);\n"
"	color.rgb += scratch.rrr;\n"
"	float2 rCoord = (uv) * 0.33;\n"
"	float3 rand = tex2D(TextureDiffuse, rCoord);\n"
"	if (noiseAmount > rand.r)\n"
"	{\n"
"		color.rgb = 0.1 + rand.b * 0.4;\n"
"	}\n"
"	float gray = dot(color, float4(0.3, 0.59, 0.11, 0));\n"
"	color = float4(gray * float3(0.9, 0.8, 0.6), 1);\n"
"	float2 dist = 0.5 - uv;\n"
"	color.rgb *= (0.4 - dot(dist, dist)) * 2.8;\n"
"	return color;\n"
"}\n",

"[ps-old-movie.ps] = old movie.ps\n"
"[ps-old-movie.ps][float][scratchAmount]       = min 0.00001     max 0.01            default 0.0044\n"
"[ps-old-movie.ps][float][noiseAmount]         = min 0.000001    max 1.0             default 0.000001\n"
"[ps-old-movie.ps][float][frame]               = min 0.0         max 2.0             default 1.0\n",
//Old Movie **********************

//Pinch mouse **********************
"pinch mouse.ps",

"float2 center : register(C0);"
"float radius : register(C1);"
"float strength : register(C2);"
"float aspectRatio : register(C3);"
"sampler2D TextureDiffuse : register(S0);"
"float4 main(float2 uv : TEXCOORD) : COLOR"
"{"
"	float2 newCenter;"
"	newCenter.x = 1.0-(center.x / 794.0);"
"	newCenter.y = 1.0-(center.y / 678.0);"
"	float2 dir = newCenter - uv;"
"	float2 scaledDir = dir;"
"	scaledDir.y /= aspectRatio;"
"	float dist = length(scaledDir);"
"	float range = saturate(1 - (dist / (abs(-sin(radius * 8) * radius) + 0.00000001F)));"
"	float2 samplePoint = uv + dir * range * strength;"
"	return tex2D(TextureDiffuse, samplePoint);"
"}",

"[ps-pinch-mouse.ps] = pinch mouse.ps\n"
"[ps-pinch-mouse.ps][vector2][center]         = min 0.0 0.0     max 794.0 678.0  default 0.5 0.5\n"
"[ps-pinch-mouse.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
"[ps-pinch-mouse.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
"[ps-pinch-mouse.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",
//Pinch mouse**********************


//Pinch **********************
"pinch.ps",

"float2 center : register(C0);\n"
"float radius : register(C1);\n"
"float strength : register(C2);\n"
"float aspectRatio : register(C3);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float2 dir = center - uv;\n"
"	float2 scaledDir = dir;\n"
"	scaledDir.y /= aspectRatio;\n"
"	float dist = length(scaledDir);\n"
"	float range = saturate(1 - (dist / (abs(-sin(radius * 8) * radius) + 0.00000001F)));\n"
"	float2 samplePoint = uv + dir * range * strength;\n"
"	return tex2D(TextureDiffuse, samplePoint);\n"
"}\n",

"[ps-pinch.ps] = pinch.ps\n"
"[ps-pinch.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
"[ps-pinch.ps][float][radius]           = min 0.0         max 1            default 0.25\n"
"[ps-pinch.ps][float][strength]         = min 0.0         max 2            default 1.0\n"
"[ps-pinch.ps][float][aspectRatio]      = min 0.5         max 2            default 1.0\n",
//Pinch **********************

//Ripple **********************
"ripple.ps",

"float2 center : register(C0);\n"
"float amplitude : register(C1);\n"
"float frequency: register(C2);\n"
"float phase: register(C3);\n"
"float aspectRatio : register(C4);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"

"	float2 dir = uv - center;" // vector from center to pixel
"	dir.y /= aspectRatio;\n"
"	float dist = length(dir);\n"
"	dir /= dist;\n"
"	dir.y *= aspectRatio;\n"

"	float2 wave;\n"
"	sincos(frequency * dist + phase, wave.x, wave.y);\n"

"	float falloff = saturate(1 - dist);\n"
"	falloff *= falloff;\n"

"	dist += amplitude * wave.x * falloff;\n"
"	float2 samplePoint = center + dist * dir;\n"
"	float4 color = tex2D(TextureDiffuse, samplePoint);\n"

"	float lighting = 1 - amplitude * 0.2 * (1 - saturate(wave.y * falloff));\n"
"	color.rgb *= lighting;\n"
"	return color;\n"
"}\n",

"[ps-ripple.ps] = ripple.ps\n"
"[ps-ripple.ps][vector2][center]         = min 0.0 0.0     max 1.0 1.0      default 0.5 0.5\n"
"[ps-ripple.ps][float][amplitude]        = min 0.0         max 1.0          default 0.1\n"
"[ps-ripple.ps][float][frequency]        = min 0.0         max 100.0        default 70.0\n"
"[ps-ripple.ps][float][phase]            = min -20.0       max 20.0         default 0.0\n"
"[ps-ripple.ps][float][aspectRatio]      = min 0.5         max 2.0          default 1.34\n",
//Ripple **********************

//Sharpen **********************
"sharpen.ps",

"float amount : register(C0);\n"
"float2 inputSize : register(C1);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float2 offset = 1 / inputSize;\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	color.rgb += tex2D(TextureDiffuse, uv - offset) * amount;\n"
"	color.rgb -= tex2D(TextureDiffuse, uv + offset) * amount;\n"
"	return color;\n"
"}\n",

"[ps-sharpen.ps] = sharpen.ps\n"
"[ps-sharpen.ps][vector2][inputSize]     = min 1.0 1.0     max 1000.0 1000.0      default 800.0 600.0\n"
"[ps-sharpen.ps][float][amount]          = min 0.0         max 2.0                default 1.0 \n",
//sharpen **********************


//Sketch **********************
"sketch.ps",

"float brushSize : register(C0);\n"
"sampler Image : register(s0);\n"
"float4 main(float2 texCoord: TEXCOORD,uniform float scale,uniform float pixelSize) : COLOR\n"
"{\n"
"	float4 color = tex2D( Image, texCoord );  \n"
"	float2 samples[4] = {0, -1,	-1, 0, 1, 0, 0, 1 };\n"
"	float4 laplace = -4 * color;\n"

"	for (int i = 0; i < 4; ++i)\n"
"	{\n"
"		laplace += tex2D(Image, texCoord + brushSize * samples[i]);\n"
"		laplace.r=laplace.rgb;\n"
"		laplace.g=laplace.rgb;\n"
"		laplace.b=laplace.rgb;\n"
"	}\n"

"	laplace =(1/ laplace);\n"
"	float4 complement;\n"
"	complement.rgb=1-laplace.rgb;\n"
"	complement.a = color.a;\n"
"	if(complement.r>1)\n"
"	{\n"
"		float gray = complement.r * 0.3 + complement.g * 0.59 + complement.b *0.11;     \n"
"		complement.r = gray;\n"
"		complement.g = gray; "
"		complement.b = gray;\n"
"		return complement;\n"
"	}\n"
"	else\n"
"	{\n"
"		float gray = color.r * 0.3 + color.g * 0.59 + color.b *0.11;     \n"
"		color.r =  gray;\n"
"		color.g =  gray;  \n"
"		color.b = gray;\n"
"		return color;\n"
"	}\n"
"}\n",

"[ps-sketch.ps] = sketch.ps\n"
"[ps-sketch.ps][float][brushSize]          = min 0.0006         max 1.0                default 0.003 \n",
//Sketch **********************

//Smooth Magnify **********************
"smooth magnify.ps",

"float2 center : register(C0);\n"
"float innerRadius: register(C1);\n"
"float outerRadius : register(C2);\n"
"float magnification : register(C3);\n"
"float aspectRatio : register(C4);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float2 centerToPixel = uv - center;\n"
"	float dist = length(centerToPixel / float2(1, aspectRatio));\n"
"	float ratio = smoothstep(innerRadius, max(innerRadius, outerRadius), dist);\n"
"	float2 samplePoint = lerp(center + centerToPixel / magnification, uv, ratio);\n"
"	return tex2D(TextureDiffuse, samplePoint);\n"
"}\n",

"[ps-smooth-magnify.ps] = smooth magnify.ps\n"
"[ps-smooth-magnify.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
"[ps-smooth-magnify.ps][float][innerRadius]       = min 0.0         max 1.0                default 0.2 \n"
"[ps-smooth-magnify.ps][float][outerRadius]       = min 0.0         max 1.0                default 0.4 \n"
"[ps-smooth-magnify.ps][float][magnification]     = min 0.0         max 5.0                default 2.0 \n"
"[ps-smooth-magnify.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",
//Smooth Magnify **********************


//Spiral **********************
"spiral.ps",

"float2 center : register(C0);\n"
"float spiralStrength : register(C1);\n"
"float aspectRatio : register(C2);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float2 dir = uv - center;\n"
"	dir.y /= aspectRatio;\n"
"	float dist = length(dir);\n"
"	float angle = atan2(dir.y, dir.x);\n"

"	float newAngle = angle + spiralStrength * dist;\n"
"	float2 newDir;\n"
"	sincos(newAngle, newDir.y, newDir.x);\n"
"	newDir.y *= aspectRatio;\n"

"	float2 samplePoint = center + newDir * dist;\n"
"	bool isValid = all(samplePoint >= 0 && samplePoint <= 1);\n"
"	return isValid ? tex2D(TextureDiffuse, samplePoint) : float4(0, 0, 0, 0);\n"
"}\n",

"[ps-spiral.ps] = spiral.ps\n"

"[ps-spiral.ps][vector2][center]          = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
"[ps-spiral.ps][float][spiralStrength]    = min 0.0         max 20.0               default 10.0 \n"
"[ps-spiral.ps][float][aspectRatio]       = min 0.5         max 2.0                default 1.4 \n",
//Spiral **********************

//Tone Mapping **********************
"tone mapping.ps",

"float defog : register(C0);\n"
"float4 fogColor : register(C1);\n"
"float exposure : register(C2);\n"
"float gamma : register(C3);\n"
"float2 vignetteCenter : register(C4);\n"
"float vignetteRadius : register(C5);\n"
"float vignetteAmount : register(C6);\n"
"float blueShift : register(C7);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 c = tex2D(TextureDiffuse, uv);\n"
"	c.rgb = max(0, c.rgb - defog * fogColor.rgb);\n"
"	c.rgb *= pow(2.0f, exposure);\n"
"	c.rgb = pow(c.rgb, gamma);\n"

"	float2 tc = uv - vignetteCenter;\n"
"	float v = length(tc) / vignetteRadius;\n"
"	c.rgb += pow(v, 4) * vignetteAmount;\n"

"	float3 d = c.rgb * float3(1.05f, 0.97f, 1.27f);\n"
"	c.rgb = lerp(c.rgb, d, blueShift);\n"

"	return c;\n"
"}\n",

"[ps-tone-mapping.ps] = tone mapping.ps\n"
"[ps-tone-mapping.ps][vector2][vignetteCenter]  = min 0.0 0.0     max 1.0 1.0            default 0.5 0.5 \n"
"[ps-tone-mapping.ps][float][defog]             = min 0.0         max 1.0                default 0.4 \n"
"[ps-tone-mapping.ps][float][exposure]          = min -1.0        max 1.0                default -0.2 \n"
"[ps-tone-mapping.ps][float][gamma]             = min 0.5         max 2.0                default 0.63 \n"
"[ps-tone-mapping.ps][float][vignetteRadius]    = min 0.0         max 1.0                default 0.5 \n"
"[ps-tone-mapping.ps][float][vignetteAmount]    = min -1.0        max 1.0                default 0.0 \n"
"[ps-tone-mapping.ps][float][blueShift]         = min 0.0         max 10.0               default 10.0 \n"
"[ps-tone-mapping.ps][rgba][fogColor]           = min 0 0 0 0     max 255 255 255 255    default 255 255 255 255 \n",
//Tone Mapping **********************

//Toon **********************
"toon.ps",

"float levels : register(C0);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 color = tex2D( TextureDiffuse, uv );\n"
"	color.rgb /= color.a;\n"

"	int result = floor(levels);\n"
"	color.rgb *= result;\n"
"	color.rgb = floor(color.rgb);\n"
"	color.rgb /= result;\n"
"	color.rgb *= color.a;\n"
"	return color;\n"
"}\n",

"[ps-toon.ps] = toon.ps\n"
"[ps-toon.ps][float][levels]         = min 0.0         max 15.0               default 5.0 \n",
//Toon **********************

//Fade **********************
"fade.ps",

"float progress : register(C0);\n"
"\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"struct PS_INPUT\n"
"{\n"
"	float4 position : POSITION;\n"
"	float2 uv : TEXCOORD;\n"
"};\n"
"\n"
"float4 Fade(float2 uv, float progress, float4 c2)\n"
"{\n"
"	float4 c1 = tex2D(TextureAnimationEffect, uv);\n"
"	return lerp(c1, c2, progress);\n"
"}\n"
"\n"
"float4 main(PS_INPUT input) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, input.uv);\n"
"	return Fade(input.uv, progress / 100.0, color);\n"
"}\n",

"[ps-fade.ps] = fade.ps\n"
"[ps-fade.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
//Fade **********************

//Fade Radial **********************
"fade radial.ps",

"float progress : register(C0);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"static const int count = 24;\n"
"\n"
"float4 RadialBlur(float4 color, float progress, float2 uv)\n"
"{\n"
"	float2 center = float2(0.5, 0.5);\n"
"	float2 toUV = uv - center;\n"
"	float2 normToUV = toUV;\n"
"	float4 c1 = float4(0, 0, 0, 0);\n"
"\n"
"	float s = progress * 0.02;\n"
"	for (int i = 0; i < count; ++i)\n"
"	{\n"
"		c1 += tex2D(TextureAnimationEffect, uv - normToUV * s * i);\n"
"	}\n"
"	c1 /= count;\n"
"	return lerp(c1, color, progress);\n"
"}\n"
"\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	return RadialBlur(color, progress / 100, uv);\n"
"}\n",

"[ps-fade-radial.ps] = fade radial.ps\n"
"[ps-fade-radial.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
//Fade Radial **********************


//Fade Ripple **********************
"fade ripple.ps",

"float progress : register(C0);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"float4 Ripple(float progress,float2 uv)\n"
"{\n"
"	float frequency = 20;\n"
"	float speed = 10;\n"
"	float amplitude = 0.05;\n"
"	float2 center = float2(0.5, 0.5);\n"
"	float2 toUV = uv - center;\n"
"	float distanceFromCenter = length(toUV);\n"
"	float2 normToUV = toUV / distanceFromCenter;\n"
"	float wave = cos(frequency * distanceFromCenter - speed * progress);\n"
"	float offset1 = progress * wave * amplitude;\n"
"	float offset2 = (1.0 - progress) * wave * amplitude;\n"
"	float2 newUV1 = center + normToUV * (distanceFromCenter + offset1);\n"
"	float2 newUV2 = center + normToUV * (distanceFromCenter + offset2);\n"
"	float4 c1 = tex2D(TextureAnimationEffect, newUV1);\n"
"	float4 c2 = tex2D(TextureDiffuse, newUV2);\n"
"	return lerp(c1, c2, progress);\n"
"}\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse,uv);\n"
"	return Ripple(progress / 100,uv);\n"
"}\n",

"[ps-fade-ripple.ps] = fade ripple.ps\n"
"[ps-fade-ripple.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
//Fade Ripple **********************

//Fade Saturate **********************
"fade saturate.ps",

"float progress : register(C0);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"float4 Saturate(float2 uv, float progress,float4 c2)\n"
"{\n"
"	float4 c1 = tex2D(TextureAnimationEffect, uv);\n"
"	c1 = saturate(c1 * (2 * progress + 1));\n"
"	if (progress > 0.8)\n"
"	{\n"
"		float new_progress = (progress - 0.8) * 5.0;\n"
"		return lerp(c1, c2, new_progress);\n"
"	}\n"
"	else\n"
"	{\n"
"		return c1;\n"
"	}\n"
"}\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse,uv);\n"
"	return Saturate(uv, progress / 100,color);\n"
"}\n",

"[ps-fade-saturate.ps] = fade saturate.ps\n"
"[ps-fade-saturate.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
//Fade Saturate **********************


//Fade Twist **********************
"fade twist.ps",

"float progress : register(C0);\n"
"float twistAmount : register(C1);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"static const float4 vzero =\n"
"{\n"
"	0,\n"
"	0,\n"
"	0,\n"
"	0\n"
"};\n"
"\n"
"float4 SampleWithBorder(float4 border, sampler2D tex, float2 uv)\n"
"{\n"
"	if (any(saturate(uv) - uv))\n"
"		return border;\n"
"	else\n"
"		return tex2D(tex, uv);\n"
"}\n"
"\n"
"float4 Swirl(float2 uv, float progress, float4 color)\n"
"{\n"
"	float2 center = float2(0.5, 0.5);\n"
"	float2 toUV = uv - center;\n"
"	float distanceFromCenter = length(toUV);\n"
"	float2 normToUV = toUV / distanceFromCenter;\n"
"	float angle = atan2(normToUV.y, normToUV.x);\n"
"	angle += distanceFromCenter * distanceFromCenter * twistAmount * progress;\n"
"	float2 newUV;\n"
"	sincos(angle, newUV.y, newUV.x);\n"
"	newUV *= distanceFromCenter;\n"
"	newUV += center;\n"
"	float4 c1 = SampleWithBorder(vzero, TextureAnimationEffect, newUV);\n"
"	return lerp(c1, color, progress);\n"
"}\n"
"\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	return Swirl(uv, progress / 100, color);\n"
"}\n",

"[ps-fade-twist.ps] = fade twist.ps\n"
"[ps-fade-twist.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
"[ps-fade-twist.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",
//Fade Twist **********************

//Fade Twist Grid **********************
"fade twist grid.ps",

"float progress : register(C0);\n"
"float twistAmount : register(C1);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"float4 SampleWithBorder(float4 border, sampler2D tex, float2 uv)\n"
"{\n"
"	if (any(saturate(uv) - uv))\n"
"	{\n"
"		return border;\n"
"	}\n"
"	else\n"
"	{\n"
"		return tex2D(tex, uv);\n"
"	}\n"
"}\n"
"float4 SwirlGrid(float2 uv, float progress,float4 color)\n"
"{\n"
"	float cellsize = 1.0 / 10;\n"
"	float2 cell = floor(uv * 10);\n"
"	float2 oddeven = fmod(cell, 2.0);\n"
"	float cellTwistAmount = twistAmount;\n"
"	if (oddeven.x < 1.0)\n"
"	{\n"
"		cellTwistAmount *= -1;\n"
"	}\n"
"	if (oddeven.y < 1.0)\n"
"	{\n"
"		cellTwistAmount *= -1;\n"
"	}\n"
"	float2 newUV = frac(uv * 10);\n"
"	float2 center = float2(0.5, 0.5);\n"
"	float2 toUV = newUV - center;\n"
"	float distanceFromCenter = length(toUV);\n"
"	float2 normToUV = toUV / distanceFromCenter;\n"
"	float angle = atan2(normToUV.y, normToUV.x);\n"
"	angle += distanceFromCenter * distanceFromCenter * cellTwistAmount * progress;\n"
"	float2 newUV2;\n"
"	sincos(angle, newUV2.y, newUV2.x);\n"
"	newUV2 *= distanceFromCenter;\n"
"	newUV2 += center;\n"
"	newUV2 *= cellsize;\n"
"	newUV2 += cell * cellsize;\n"
"	float4 c1 = tex2D(TextureAnimationEffect, newUV2);\n"
"	return lerp(c1, color, progress);\n"
"}\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse,uv);\n"
"	return SwirlGrid(uv, progress / 100,color);\n"
"}\n",

"[ps-fade-twist-grid.ps] = fade twist grid.ps\n"
"[ps-fade-twist-grid.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n"
"[ps-fade-twist-grid.ps][float][twistAmount]      = min -70.0       max 70.0                default 30.0 \n",
//Fade Twist Grid **********************

//Fade Wave **********************
"fade wave.ps",

"float progress : register(C0);\n"
"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s1);\n"
"\n"
"float4 SampleWithBorder(float4 border, sampler2D tex, float2 uv)\n"
"{\n"
"	if (any(saturate(uv) - uv))\n"
"	{\n"
"		return border;\n"
"	}\n"
"	else\n"
"	{\n"
"		return tex2D(tex, uv);\n"
"	}\n"
"}\n"
"float4 Wave(float2 uv, float progress,float4 color)\n"
"{\n"
"	float mag = 0.1;\n"
"	float phase = 14;\n"
"	float freq = 20;\n"
"	float2 newUV = uv + float2(mag * progress * sin(freq * uv.y + phase * progress), 0);\n"
"	float4 c1 = SampleWithBorder(0, TextureAnimationEffect, newUV);\n"
"	return lerp(c1, color, progress);\n"
"}\n"
"\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	return Wave(uv, progress / 100,color);\n"
"}\n",

"[ps-fade-wave.ps] = fade wave.ps\n"
"[ps-fade-wave.ps][float][progress]         = min 0.0         max 100.0               default 30.0 \n",
//Fade Wave **********************


//Blur com Zoom **********************
"blur zoom.ps",

"float2 center : register(C0);\n"
"float blurAmount : register(C1);\n"
"sampler2D TextureDiffuse : register(S0);\n"
"float4 main(float2 uv : TEXCOORD) : COLOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	float4 c = 0;\n"
"	uv -= center;\n"
"	for (int i = 0; i < 15; ++i)\n"
"	{\n"
"		float scale = 1.0 + blurAmount * (i / 14.0);\n"
"		c += tex2D(TextureDiffuse, uv * scale + center);\n"
"	}\n"
"	c /= 15;\n"
"	return c;\n"
"}\n",

"[ps-blur-zoom.ps] = blur zoom.ps\n"
"[ps-blur-zoom.ps][vector2][center]           = min 0.0 0.0     max 1.0 1.0             default 30.0 30.0 \n"
"[ps-blur-zoom.ps][float][blurAmount]         = min 0.0         max 2.0                 default 0.2 \n",
//Blur com Zoom **********************


//Texture Map **********************
"texture map.ps",

"sampler2D TextureDiffuse : register(s0);\n"
"sampler2D TextureAnimationEffect : register(s2);\n"
"float horizontalSize : register(c0);\n"
"float verticalSize : register(c3);\n"
"float verticalOffset : register(C1);\n"
"float horizontalOffset : register(C4);\n"
"float strength : register(c5);\n"
"\n"
"float4 main(float2 uv : TEXCOORD) : COlOR\n"
"{\n"
"	float4 color = tex2D(TextureDiffuse, uv);\n"
"	float horzOffset = frac(uv.x / horizontalSize + min(1, horizontalOffset));\n"
"	float vOffset = frac(uv.y / verticalSize + min(1, verticalOffset));\n"
"	float2 offset = tex2D(TextureAnimationEffect, float2(horzOffset, vOffset)).xy * strength - (strength / 8);\n"
"	float4 c1 = tex2D(TextureDiffuse, frac(uv + offset));\n"
"	return c1;\n"
"}\n",

"[ps-texture-map.ps] = texture map.ps\n"
"[ps-texture-map.ps][float][horizontalSize]       = min 0.0         max 5.0                 default 1.0 \n"
"[ps-texture-map.ps][float][verticalSize]         = min 0.0         max 5.0                 default 1.0 \n"
"[ps-texture-map.ps][float][horizontalOffset]     = min 0.0         max 1.0                 default 0.0 \n"
"[ps-texture-map.ps][float][verticalOffset]       = min 0.0         max 1.0                 default 0.0 \n"
"[ps-texture-map.ps][float][strength]             = min 0.0         max 10.0                default 1.0 \n",
//Texture Map **********************

//Lit Textured **********************
"lit textured.ps",
kLitTexturedPixelShaderD3D9.c_str(),

"[ps-lit-textured.ps] = lit textured.ps\n",
//Lit Textured **********************

//Lit Solid **********************
"lit solid.ps",

"int LightEnabled;\n"
"int LightMode;\n"
"float4 AmbientColor;\n"
"float3 LightDirectionView;\n"
"float4 LightColor;\n"
"float4 MaterialDiffuse;\n"
"float4 MaterialAmbient;\n"
"float4 MaterialSpecular;\n"
"float4 MaterialEmissive;\n"
"float MaterialPower;\n"
"\n"
"float4 main(float3 normalViewIn : TEXCOORD1, float3 positionViewIn : TEXCOORD2) : COLOR\n"
"{\n"
"    float4 baseColor = float4(1,1,1,1);\n"
"    if (LightEnabled == 0 || LightMode != 1)\n"
"        return baseColor;\n"
"    float3 normalView = normalize(normalViewIn);\n"
"    float3 viewDir = normalize(-positionViewIn);\n"
"    float3 lightTravel = normalize(LightDirectionView);\n"
"    float diffuse = max(dot(normalView, -lightTravel), 0);\n"
"    float3 base = MaterialDiffuse.rgb;\n"
"    float3 light = saturate((AmbientColor.rgb * MaterialAmbient.rgb) + (LightColor.rgb * diffuse));\n"
"    float3 specular = float3(0, 0, 0);\n"
"    if (diffuse > 0.0f && MaterialPower > 0.0f)\n"
"    {\n"
"        float3 lightDir = normalize(-lightTravel);\n"
"        float3 halfDir = normalize(lightDir + viewDir);\n"
"        float spec = pow(max(dot(normalView, halfDir), 0), MaterialPower);\n"
"        specular = LightColor.rgb * MaterialSpecular.rgb * spec;\n"
"    }\n"
"    float3 litColor = saturate((base * light) + MaterialEmissive.rgb + specular);\n"
"    return float4(litColor, MaterialDiffuse.a);\n"
"}\n",

"[ps-lit-solid.ps] = lit solid.ps\n",
//Lit Solid **********************




/* VERTEX SHADER -----------------------------------------------------------------------------------------------------*/




//Outline **********************
"outline.vs",

"float4x4 mvpMatrix : register(c0);\n"
"float4x4 mvMatrix;\n"
"struct VS_INPUT\n"
"{\n"
"    float4 position : POSITION;\n"
"    float3 normal : NORMAL;\n"
"};\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 position : POSITION;\n"
"    float3 normalView : TEXCOORD0;\n"
"    float3 positionView : TEXCOORD1;\n"
"};\n"
"VS_OUTPUT main(VS_INPUT input)\n"
"{\n"
"    VS_OUTPUT output;\n"
"    output.position = mul(input.position, mvpMatrix);\n"
"    output.normalView = mul(float4(input.normal, 0.0), mvMatrix).xyz;\n"
"    output.positionView = mul(input.position, mvMatrix).xyz;\n"
"    return output;\n"
"}\n",

"[vs-outline.vs] = outline.vs\n",
//Outline **********************

//Textura Simples **********************
"simple texture.vs",

"struct VS_OUTPUT\n"
"{\n"
"	float4 pos : POSITION;\n"
"	float2 uv : TEXCOORD0;\n"
"};\n"
"\n"
"float4x4 mvpMatrix;\n"
"\n"
"VS_OUTPUT main(in float4 pos : POSITION, in float2 uv : TEXCOORD0)\n"
"{\n"
"	VS_OUTPUT ret;\n"
"	ret.pos = mul(pos, mvpMatrix);\n"
"	ret.uv = uv;\n"
"	return ret;\n"
"}\n",

"[vs-simple-texture.vs] = simple texture.vs\n",
//Textura Simples **********************


//Escala simples **********************
"scale.vs",
"struct VS_OUTPUT\n"
"{\n"
"	float4 pos : POSITION;\n"
"	float2 uv : TEXCOORD0;\n"
"};\n"
"\n"
"float4x4 mvpMatrix;\n"
"float2 scale;\n"
"\n"
"VS_OUTPUT main(in float4 pos : POSITION, in float2 uv : TEXCOORD0)\n"
"{\n"
"	VS_OUTPUT ret;\n"
"	pos.x *= scale.x;\n"
"	pos.y *= scale.y;\n"
"	ret.pos = mul(pos, mvpMatrix);\n"
"	ret.uv = uv;\n"
"	return ret;\n"
"}\n",

"[vs-scale.vs] = scale.vs\n"
"[vs-scale.vs][vector2][scale]       = min -10.0 -10.0         max 10.0  10.0                default 1.0 1.0\n",
//Escala simples **********************


    //Escala Diff **********************
    "scale diff.vs",

    "float4x4 mvpMatrix;\n"
    "float scale;\n"
    "float maxHeight;\n"
    "float height;\n"
    "\n"
    "struct VS_OUTPUT\n"
    "{\n"
    "	float4 pos : POSITION;\n"
    "	float2 uv : TEXCOORD0;\n"
    "};\n"
    "\n"
    "VS_OUTPUT main(in float4 pos : POSITION, in float2 uv : TEXCOORD0)\n"
    "{\n"
    "	VS_OUTPUT ret;\n"
    "	if (pos.y > height)\n"
    "	{\n"
    "		const float num = scale * (abs(pos.y) / maxHeight);\n"
    "		pos.x *= num;\n"
    "		pos.z *= num;\n"
    "	}\n"
    "	else\n"
    "	{\n"
    "		const float num = (1.0 - scale) * (1.0 - (abs(pos.y) / maxHeight));\n"
    "		pos.x *= num;\n"
    "		pos.z *= num;\n"
    "	}\n"
    "	ret.pos = mul(pos, mvpMatrix);\n"
    "	ret.uv = uv;\n"
    "	return (ret);\n"
    "}\n",

    "[vs-scale-diff.vs] = scale diff.vs\n"
    "[vs-scale-diff.vs][float][scale]       = min 0.0         max 3.0                 default 1.0 \n"
    "[vs-scale-diff.vs][float][height]      = min -100.0      max 100.0               default 0.0 \n"
    "[vs-scale-diff.vs][float][maxHeight]   = min -100.0      max 100.0               default 0.0 \n",
    //Escala Diff **********************

    //Escala Diff by Y **********************
    "scale diff by y.vs",

    "float4x4 mvpMatrix;\n"
    "float scale;\n"
    "float maxHeight;\n"
    "float height;\n"
    "\n"
    "struct VS_OUTPUT\n"
    "{\n"
    "	float4 pos : POSITION;\n"
    "	float2 uv : TEXCOORD0;\n"
    "};\n"
    "\n"
    "\n"
    "VS_OUTPUT main(in float4 pos : POSITION, in float2 uv : TEXCOORD0)\n"
    "{\n"
    "	VS_OUTPUT ret;\n"
    "	if (pos.y > height)\n"
    "	{\n"
    "		const float num = scale * (abs(pos.y) / maxHeight);\n"
    "		pos.y *= num;\n"
    "		pos.z *= num;\n"
    "	}\n"
    "	else\n"
    "	{\n"
    "		const float num = (1.0 - scale) * (1.0 - (abs(pos.y) / maxHeight));\n"
    "		pos.y *= num;\n"
    "		pos.z *= num;\n"
    "	}\n"
    "	ret.pos = mul(pos, mvpMatrix);\n"
    "	ret.uv = uv;\n"
    "	return (ret);\n"
    "}\n",

    "[vs-scale-diff-by-y.vs] = scale diff by y.vs\n"
    "[vs-scale-diff-by-y.vs][float][scale]       = min 0.0         max 3.0                 default 1.0 \n"
    "[vs-scale-diff-by-y.vs][float][height]      = min -100.0      max 100.0               default 0.0 \n"
    "[vs-scale-diff-by-y.vs][float][maxHeight]   = min -100.0      max 100.0               default 0.0 \n",
    //Escala Diff by Y **********************


    //Fluttering **********************
    "fluttering.vs",

    "float4x4 mvpMatrix;\n"
    "float wave;\n"
    "float deepth;\n"
    "\n"
    "struct VS_OUTPUT\n"
    "{\n"
    "	float4 pos : POSITION;\n"
    "	float2 uv : TEXCOORD0;\n"
    "};\n"
    "\n"
    "VS_OUTPUT main(in float4 pos : POSITION, in float4 nor : NORMAL, in float2 uv : TEXCOORD0)\n"
    "{\n"
    "	VS_OUTPUT ret;\n"
    "	ret.pos = pos;\n"
    "	float angle = (wave % 360) * 2;\n"
    "	ret.pos.z = sin(ret.pos.x + angle);\n"
    "	ret.pos.z += sin(ret.pos.y / 2 + angle);\n"
    "	ret.pos.z *= (ret.pos.x * 0.09f) + nor.z * deepth;\n"
    "	ret.pos = ret.pos + mul(ret.pos, mvpMatrix);\n"
    "	ret.uv = uv;\n"
    "	return ret;\n"
    "}\n",

    "[vs-fluttering.vs] = fluttering.vs\n"
    "[vs-fluttering.vs][float][wave]       = min 0.0         max 1.0                 default 1.0 \n"
    "[vs-fluttering.vs][float][deepth]     = min 0.0         max 10.0                default 1.0 \n",
    //Fluttering **********************

    nullptr,nullptr,nullptr };

    const char** getShaderEngineBuiltIn()
    {
        return resourceShader;
    }

    const char* getCodePScolorFor_LINE_MESH()
    {
        // Pixel Shader for line LINE_MESH (we do not expose to user, that is why is not part of resourceShader)
        static const char* codePScolor_LINE_MESH = "float4 color : register(c0);\n"
            "float4 main() : COLOR\n"
            "{\n"
            "    return color;\n"
            "}\n";

        return codePScolor_LINE_MESH;
    }
    const char* getCodeVScolorFor_LINE_MESH()
    {
        // Vertex Shader for line LINE_MESH (we do not expose to user, that is why is not part of resourceShader)
        static const char* codeVsColor_LINE_MESH = "float4x4 mvpMatrix : register(c0);\n"
            "float4 main(float4 aPosition : POSITION) : POSITION\n"
            "{\n"
            "    return mul(aPosition, mvpMatrix);\n"
            "}\n";

        return codeVsColor_LINE_MESH;
    }

    API_IMPL const char* getParticlePSCode()
    {
        static const char* psParticleCode = 
            "float4 color : register(c0);\n"
            "float enableAlphaFromColor : register(c1);\n"
            "sampler2D TextureDiffuse : register(s0);\n"
            "\n"
            "float4 main(float2 vTexCoord : TEXCOORD0) : COLOR\n"
            "{\n"
            "    float4 texColor = tex2D(TextureDiffuse, vTexCoord);\n"
            "    float4 outColor;\n"
            "    \n"
            "    if(enableAlphaFromColor > 0.5)\n"
            "        outColor.a = color.a;\n"
            "    else\n"
            "        outColor.a = texColor.a;\n"
            "    \n"
            "    outColor.rgb = color.rgb ? texColor.rgb;\n"
            "    #\n"
            "    return outColor;\n"
            "}\n";
        return psParticleCode;
    }

    const char* getParticleVSCode()
    {
        static const char* vsParticleCode =
            "float4x4 mvpMatrix : register(c0);\n"
            "\n"
            "struct VS_INPUT\n"
            "{\n"
            "    float4 aPosition : POSITION;\n"
            "    float2 aTextCoord : TEXCOORD0;\n"
            "};\n"
            "\n"
            "struct VS_OUTPUT\n"
            "{\n"
            "    float4 position : POSITION;\n"
            "    float2 vTexCoord : TEXCOORD0;\n"
            "};\n"
            "\n"
            "VS_OUTPUT main(VS_INPUT input)\n"
            "{\n"
            "    VS_OUTPUT output;\n"
            "    output.position = mul(input.aPosition, mvpMatrix);\n"
            "    output.vTexCoord = input.aTextCoord;\n"
            "    return output;\n"
            "}\n";
        return vsParticleCode;
    }

    const char* getSteeredParticlePSCode(bool hasColor)
    {
        if (hasColor)
        {
            return  "float4 color   : register(c4);"
                    "sampler2D TextureDiffuse : register(s0);"
                    ""
                    "struct PSInput {"
                    "    float2 vTexCoord : TEXCOORD0;"
                    "};"
                    ""
                    "float4 main(PSInput input) : COLOR0 {"
                    "    float4 texColor = tex2D(TextureDiffuse, input.vTexCoord);"
                    "    return color * texColor;"
                    "}";
        }
        else
        {
            return  "sampler2D TextureDiffuse : register(s0);"
                    ""
                    "struct PSInput {"
                    "    float2 vTexCoord : TEXCOORD0;"
                    "};"
                    ""
                    "float4 main(PSInput input) : COLOR0 {"
                    "    float4 texColor = tex2D(TextureDiffuse, input.vTexCoord);"
                    "    return texColor;"
                    "}";
        }
    }
    const char* getSteeredParticleVSCode()
    {
        return "float4x4 mvpMatrix : register(c0);"
                ""
                "struct VSInput {"
                "    float4 aPosition : POSITION;"
                "    float2 aTextCoord : TEXCOORD0;"
                "};"
                ""
                "struct VSOutput {"
                "    float4 gl_Position : POSITION;"
                "    float2 vTexCoord : TEXCOORD0;"
                "};"
                ""
                "VSOutput main(VSInput input) {"
                "    VSOutput o;"
                "    o.gl_Position = mul(input.aPosition, mvpMatrix);"
                "    o.vTexCoord = input.aTextCoord;"
                "    return o;"
                "}";
    }

    static std::string PS_Vesrion("ps_2_0");
    static std::string VS_Vesrion("vs_2_0");

    const char* getPSVersion()
    {
        return PS_Vesrion.c_str();
    }
    const char* getVSVersion()
    {
        return VS_Vesrion.c_str();
    }
    void setPSVersion(const char* version)
    {
        if (version == nullptr)
            version = "";
        PS_Vesrion = version;
    }
    void setVSVersion(const char* version)
    {
        if (version == nullptr)
            version = "";
        VS_Vesrion = version;
    }
}


#endif
