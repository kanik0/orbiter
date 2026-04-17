// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLConfig.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace ogl {

OGLConfig::OGLConfig()
	: surfaceMaxLevel(14),
	  surfaceReflect(true),
	  surfaceLights(true),
	  surfaceLightBrt(0.5f),
	  atmHaze(true),
	  clouds(true),
	  cloudShadows(true),
	  vesselShadows(true),
	  postProcessing(true),
	  bloomThreshold(0.8f),
	  bloomIntensity(0.5f),
	  lensFlare(true),
	  exposure(1.0f),
	  ambientLevel(15),
	  limitFramerate(false),
	  maxFramerate(60),
	  useNormalMaps(true),
	  useEnvMaps(true)
{
}

void OGLConfig::Load(const char *cfgFile)
{
	if (!cfgFile) return;
	std::ifstream f(cfgFile);
	if (!f.is_open()) return;

	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;

		char key[64]; char val[64];
		if (sscanf(line.c_str(), "%63s = %63s", key, val) != 2) continue;

		if (strcmp(key, "SurfaceMaxLevel") == 0) surfaceMaxLevel = atoi(val);
		else if (strcmp(key, "SurfaceReflect") == 0) surfaceReflect = (atoi(val) != 0);
		else if (strcmp(key, "SurfaceLights") == 0) surfaceLights = (atoi(val) != 0);
		else if (strcmp(key, "SurfaceLightBrt") == 0) surfaceLightBrt = (float)atof(val);
		else if (strcmp(key, "AtmHaze") == 0) atmHaze = (atoi(val) != 0);
		else if (strcmp(key, "Clouds") == 0) clouds = (atoi(val) != 0);
		else if (strcmp(key, "CloudShadows") == 0) cloudShadows = (atoi(val) != 0);
		else if (strcmp(key, "VesselShadows") == 0) vesselShadows = (atoi(val) != 0);
		else if (strcmp(key, "PostProcessing") == 0) postProcessing = (atoi(val) != 0);
		else if (strcmp(key, "BloomThreshold") == 0) bloomThreshold = (float)atof(val);
		else if (strcmp(key, "BloomIntensity") == 0) bloomIntensity = (float)atof(val);
		else if (strcmp(key, "LensFlare") == 0) lensFlare = (atoi(val) != 0);
		else if (strcmp(key, "Exposure") == 0) exposure = (float)atof(val);
		else if (strcmp(key, "AmbientLevel") == 0) ambientLevel = atoi(val);
		else if (strcmp(key, "LimitFramerate") == 0) limitFramerate = (atoi(val) != 0);
		else if (strcmp(key, "MaxFramerate") == 0) maxFramerate = atoi(val);
		else if (strcmp(key, "UseNormalMaps") == 0) useNormalMaps = (atoi(val) != 0);
		else if (strcmp(key, "UseEnvMaps") == 0) useEnvMaps = (atoi(val) != 0);
	}

	fprintf(stderr, "[OGLConfig] Loaded from '%s': LOD=%d bloom=%.1f post=%d\n",
		cfgFile, surfaceMaxLevel, bloomIntensity, postProcessing);
}

void OGLConfig::Save(const char *cfgFile)
{
	if (!cfgFile) return;
	FILE *f = fopen(cfgFile, "w");
	if (!f) return;

	fprintf(f, "; OGLClient Configuration\n\n");
	fprintf(f, "SurfaceMaxLevel = %d\n", surfaceMaxLevel);
	fprintf(f, "SurfaceReflect = %d\n", surfaceReflect ? 1 : 0);
	fprintf(f, "SurfaceLights = %d\n", surfaceLights ? 1 : 0);
	fprintf(f, "SurfaceLightBrt = %.2f\n", surfaceLightBrt);
	fprintf(f, "AtmHaze = %d\n", atmHaze ? 1 : 0);
	fprintf(f, "Clouds = %d\n", clouds ? 1 : 0);
	fprintf(f, "CloudShadows = %d\n", cloudShadows ? 1 : 0);
	fprintf(f, "VesselShadows = %d\n", vesselShadows ? 1 : 0);
	fprintf(f, "PostProcessing = %d\n", postProcessing ? 1 : 0);
	fprintf(f, "BloomThreshold = %.2f\n", bloomThreshold);
	fprintf(f, "BloomIntensity = %.2f\n", bloomIntensity);
	fprintf(f, "LensFlare = %d\n", lensFlare ? 1 : 0);
	fprintf(f, "Exposure = %.2f\n", exposure);
	fprintf(f, "AmbientLevel = %d\n", ambientLevel);
	fprintf(f, "LimitFramerate = %d\n", limitFramerate ? 1 : 0);
	fprintf(f, "MaxFramerate = %d\n", maxFramerate);
	fprintf(f, "UseNormalMaps = %d\n", useNormalMaps ? 1 : 0);
	fprintf(f, "UseEnvMaps = %d\n", useEnvMaps ? 1 : 0);

	fclose(f);
}

} // namespace ogl

#endif // !_WIN32
