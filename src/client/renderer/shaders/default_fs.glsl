/**
 * @brief Default fragment shader.
 */

#version 330

#define FRAGMENT_SHADER

#include "include/uniforms.glsl"
#include "include/fog.glsl"
#include "include/noise3d.glsl"
#include "include/tint.glsl"

#define MAX_LIGHTS $r_max_lights
#define LIGHT_SCALE $r_modulate
#define DEBUG_LIGHTMAP_LAYER_INDEX $r_draw_bsp_lightmaps

#if MAX_LIGHTS
struct LightParameters {
	vec3 ORIGIN[MAX_LIGHTS];
	vec3 COLOR[MAX_LIGHTS];
	float RADIUS[MAX_LIGHTS];
};

uniform LightParameters LIGHTS;
#endif

struct CausticParameters {
	bool ENABLE;
	vec3 COLOR;
};

uniform CausticParameters CAUSTIC;

uniform bool DIFFUSE;
uniform bool LIGHTMAP;
uniform bool DELUXEMAP;
uniform bool NORMALMAP;
uniform bool GLOSSMAP;
uniform bool STAINMAP;

uniform float BUMP;
uniform float PARALLAX;
uniform float HARDNESS;
uniform float SPECULAR;

uniform sampler2D SAMPLER0;
uniform sampler2DArray SAMPLER1;
uniform sampler2D SAMPLER2;
uniform sampler2D SAMPLER3;
uniform sampler2D SAMPLER4;
uniform sampler2D SAMPLER8;

uniform float ALPHA_THRESHOLD;

in VertexData {
	vec3 modelpoint;
	vec2 texcoords[2];
	vec4 color;
	vec3 point;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec3 eye;
};

const vec3 two = vec3(2.0);
const vec3 negHalf = vec3(-0.5);

vec3 eyeDir;

out vec4 fragColor;

/**
 * @brief Yield the parallax offset for the texture coordinate.
 */
vec2 BumpTexcoord(in float height) {
	return vec2(height * 0.04 - 0.02) * PARALLAX * eyeDir.xy;
}

/**
 * @brief Yield the diffuse modulation from bump-mapping.
 */
void BumpFragment(in vec3 deluxemap, in vec3 normalmap, in vec3 glossmap, out float lightmapBumpScale, out float lightmapSpecularScale) {
	float glossFactor = clamp(dot(glossmap, vec3(0.299, 0.587, 0.114)), 0.0078125, 1.0);

	lightmapBumpScale = clamp(dot(deluxemap, normalmap), 0.0, 1.0);
	lightmapSpecularScale = (HARDNESS * glossFactor) * pow(clamp(-dot(eyeDir, reflect(deluxemap, normalmap)), 0.0078125, 1.0), (16.0 * glossFactor) * SPECULAR);
}

/**
 * @brief Yield the final sample color after factoring in dynamic light sources.
 */
void LightFragment(in vec4 diffuse, in vec3 lightmap, in vec3 normalmap, in float lightmapBumpScale, in float lightmapSpecularScale) {

	vec3 light = vec3(0.0);

#if MAX_LIGHTS
	/*
	 * Iterate the hardware light sources, accumulating dynamic lighting for
	 * this fragment. A light radius of 0.0 means break.
	 */
	for (int i = 0; i < MAX_LIGHTS; i++) {

		if (LIGHTS.RADIUS[i] == 0.0)
			break;

		vec3 delta = LIGHTS.ORIGIN[i] - point;
		float len = length(delta);

		if (len < LIGHTS.RADIUS[i]) {

			float lambert = dot(normalmap, normalize(delta));
			if (lambert > 0.0) {

				// windowed inverse square falloff
				float dist = len/LIGHTS.RADIUS[i];
				float falloff = clamp(1.0 - dist * dist * dist * dist, 0.0, 1.0);
				falloff = falloff * falloff;
				falloff = falloff / (dist * dist + 1.0);

				light += LIGHTS.COLOR[i] * falloff * lambert;
			}
		}
	}
#endif

	// now modulate the diffuse sample with the modified lightmap
	float lightmapLuma = dot(lightmap.rgb, vec3(0.299, 0.587, 0.114));

	float blackPointLuma = 0.015625;
	float l = exp2(lightmapLuma) - blackPointLuma;
	float lightmapDiffuseBumpedLuma = l * lightmapBumpScale;
	float lightmapSpecularBumpedLuma = l * lightmapSpecularScale;

	vec3 diffuseLightmapColor = lightmap.rgb * lightmapDiffuseBumpedLuma;
	vec3 specularLightmapColor = (lightmapLuma + lightmap.rgb) * 0.5 * lightmapSpecularBumpedLuma;

	fragColor.rgb = diffuse.rgb * ((diffuseLightmapColor + specularLightmapColor) + light);

	// lastly modulate the alpha channel by the color
	fragColor.a = diffuse.a * color.a;
}

/**
 * @brief Render caustics
 */
void CausticFragment(in vec3 lightmap) {
	if (CAUSTIC.ENABLE) {
		vec3 model_scale = vec3(0.024, 0.024, 0.016);
		float time_scale = 0.0006;
		float caustic_thickness = 0.02;
		float caustic_glow = 8.0;
		float caustic_intensity = 0.3;

		// grab raw 3d noise
		float factor = noise3d((modelpoint * model_scale) + (TIME * time_scale));

		// scale to make very close to -1.0 to 1.0 based on observational data
		factor = factor * (0.3515 * 2.0);

		// make the inner edges stronger, clamp to 0-1
		factor = clamp(pow((1 - abs(factor)) + caustic_thickness, caustic_glow), 0, 1);

		// start off with simple color * 0-1
		vec3 caustic_out = CAUSTIC.COLOR * factor * caustic_intensity;

		// multiply caustic color by lightmap, clamping so it doesn't go pure black
		caustic_out *= clamp((lightmap * 1.6) - 0.5, 0.1, 1.0);

		// add it up
		fragColor.rgb = clamp(fragColor.rgb + caustic_out, 0.0, 1.0);
	}
}

#define M_PI 3.1415926535897932384626433832795

/**
 * @brief Evalutes the 2-band H-Basis coefficients in the given direction.
 */
vec3 EvaluateH4Color(in vec3 direction, in vec3 h0, in vec3 h1, in vec3 h2, in vec3 h3)
{
	if (dot(direction, vec3(0.0, 0.0, 1.0)) < 0) {
		return vec3(0.0);
	}
	
	vec3 color = vec3(0.0);

	// Band 0
	color += h0 * (1.0 / sqrt(2.0 * M_PI));

	// Band 1
	color += h1 * -sqrt(1.5 / M_PI) * direction.y;
	color += h2 * sqrt(1.5 / M_PI) * (2.0 * direction.z - 1.0);
	color += h3 * -sqrt(1.5 / M_PI) * direction.x;

	return color;
}

/**
 * @brief Shader entry point.
 */
void main(void) {
	vec4 lightmapColorHDR;

	vec4 normalmap = vec4(normal, 1.0);

	if (NORMALMAP) {
		// resolve the initial normalmap sample
		normalmap = texture(SAMPLER3, texcoords[0]);

		normalmap.xyz = normalize(two * (normalmap.xyz + negHalf));
		normalmap.xyz = normalize(vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));
	}

	lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], 0));
	vec3 h0 = lightmapColorHDR.rgb * lightmapColorHDR.a;

	lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], 1));
	vec3 h1 = lightmapColorHDR.rgb * lightmapColorHDR.a;

	lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], 2));
	vec3 h2 = lightmapColorHDR.rgb * lightmapColorHDR.a;

	lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], 3));
	vec3 h3 = lightmapColorHDR.rgb * lightmapColorHDR.a;

	eyeDir = normalize(eye);

	vec4 diffuse = texture(SAMPLER0, texcoords[0]);
	float processedGrayscaleDiffuse = dot(diffuse.rgb * diffuse.a, vec3(0.299, 0.587, 0.114)) * 0.875 + 0.125;
	float guessedGlossValue = clamp(pow(processedGrayscaleDiffuse * 3.0, 4.0), 0.0, 1.0) * 0.875 + 0.125;

	vec3 eyeDirMod = eyeDir;

	vec3 lightmap = EvaluateH4Color(normalmap.xyz, h0, h1, h2, h3);
	vec3 lightmapSpecular = (HARDNESS * guessedGlossValue) * pow(clamp(dot(eyeDir, reflect(-eyeDirMod, normalmap.xyz)), 0.0078125, 1.0), (16.0 * guessedGlossValue) * SPECULAR) * EvaluateH4Color(normalize(reflect(-eyeDirMod, normalmap.xyz)), h0, h1, h2, h3);

	float VdotH = max(dot(reflect(-eyeDir, normalmap.xyz), vec3(0.0, 0.0, 1.0)), 0);

	lightmapSpecular = EvaluateH4Color(normalize(reflect(-eyeDir, normalmap.xyz)), h0, h1, h2, h3) * pow(1.0 - VdotH, 4.0);

	fragColor.rgb = lightmapSpecular;
	fragColor.a = diffuse.a;

	/*
	// first resolve the flat shading
	vec3 lightmap = color.rgb;
	vec3 deluxemap = vec3(0.0, 0.0, 1.0);

	if (LIGHTMAP) {
#if DEBUG_LIGHTMAP_LAYER_INDEX > 0
		vec4 lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], DEBUG_LIGHTMAP_LAYER_INDEX - 1));
#else
		vec4 lightmapColorHDR = texture(SAMPLER1, vec3(texcoords[1], 0));
#endif // DEBUG_LIGHTMAP_LAYER_INDEX
		lightmap = lightmapColorHDR.rgb * lightmapColorHDR.a;

		if (STAINMAP) {
			vec4 stain = texture(SAMPLER8, texcoords[1]);
			lightmap = mix(lightmap.rgb, stain.rgb, stain.a).rgb;
		}
	}

	// then resolve any bump mapping
	vec4 normalmap = vec4(normal, 1.0);
	vec2 parallax = vec2(0.0);
	
	float lightmapBumpScale = 1.0;
	float lightmapSpecularScale = 0.0;

	if (NORMALMAP) {
		eyeDir = normalize(eye);

		if (DELUXEMAP) {
			vec4 deluxeColorHDR = texture(SAMPLER1, vec3(texcoords[1], 1));
			deluxemap = deluxeColorHDR.rgb * deluxeColorHDR.a;

			deluxemap = normalize(two * (deluxemap + negHalf));
		}

		// resolve the initial normalmap sample
		normalmap = texture(SAMPLER3, texcoords[0]);

		// resolve the parallax offset from the heightmap
		parallax = BumpTexcoord(normalmap.w);

		// resample the normalmap at the parallax offset
		normalmap = texture(SAMPLER3, texcoords[0] + parallax);

		normalmap.xyz = normalize(two * (normalmap.xyz + negHalf));
		normalmap.xyz = normalize(vec3(normalmap.x * BUMP, normalmap.y * BUMP, normalmap.z));

		vec3 glossmap = vec3(0.5);

		if (GLOSSMAP) {
			glossmap = texture(SAMPLER4, texcoords[0] + parallax).rgb;
		} else if (DIFFUSE) {
			vec4 diffuse = texture(SAMPLER0, texcoords[0] + parallax);
			float processedGrayscaleDiffuse = dot(diffuse.rgb * diffuse.a, vec3(0.299, 0.587, 0.114)) * 0.875 + 0.125;
			float guessedGlossValue = clamp(pow(processedGrayscaleDiffuse * 3.0, 4.0), 0.0, 1.0) * 0.875 + 0.125;

			glossmap = vec3(guessedGlossValue);
		}

		// resolve the bumpmap modulation
		BumpFragment(deluxemap, normalmap.xyz, glossmap, lightmapBumpScale, lightmapSpecularScale);

		// and then transform the normalmap to model space for lighting
		normalmap.xyz = normalize(
			normalmap.x * normalize(tangent) +
			normalmap.y * normalize(bitangent) +
			normalmap.z * normalize(normal)
		);
	}

	vec4 diffuse = vec4(1.0);

	if (DIFFUSE) { // sample the diffuse texture, honoring the parallax offset
		diffuse = texture(SAMPLER0, texcoords[0] + parallax);

		// see if diffuse can be discarded because of alpha test
		if (diffuse.a < ALPHA_THRESHOLD)
			discard;

		TintFragment(diffuse, texcoords[0] + parallax);
	}

	// add any dynamic lighting to yield the final fragment color
	LightFragment(diffuse, lightmap, normalmap.xyz, lightmapBumpScale, lightmapSpecularScale);

	// underliquid caustics
	CausticFragment(lightmap);

#if DEBUG_LIGHTMAP_LAYER_INDEX == 0
	// tonemap
	fragColor.rgb *= exp(fragColor.rgb);
	fragColor.rgb /= fragColor.rgb + 0.825;

	// apply lightscale afterwards, because it should be done AFTER tonemapping
	fragColor.rgb *= LIGHT_SCALE;
#endif // DEBUG_LIGHTMAP_LAYER_INDEX == 0
	*/

	fragColor.rgb *= LIGHT_SCALE;

	// and fog
	FogFragment(length(point), fragColor);
}
