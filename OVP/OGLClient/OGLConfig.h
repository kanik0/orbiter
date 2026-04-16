// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLConfig - Configuration parameters for OGLClient rendering

#ifndef __OGLCONFIG_H
#define __OGLCONFIG_H

#ifndef _WIN32

namespace ogl {

struct OGLConfig {
	// Surface rendering
	int surfaceMaxLevel;       // max LOD level for tile rendering (4-19, default 14)
	bool surfaceReflect;       // specular water reflections
	bool surfaceLights;        // night city lights
	float surfaceLightBrt;     // city light brightness (0-1)

	// Atmosphere
	bool atmHaze;              // render atmospheric haze
	bool clouds;               // render cloud layers
	bool cloudShadows;         // project cloud shadows

	// Effects
	bool vesselShadows;        // render vessel ground shadows
	bool postProcessing;       // enable HDR bloom/flare pipeline
	float bloomThreshold;      // bright pixel extraction threshold
	float bloomIntensity;      // bloom strength
	bool lensFlare;            // sun lens flare effect
	float exposure;            // HDR exposure value

	// Performance
	int ambientLevel;          // ambient light (0-255)
	bool limitFramerate;       // cap framerate
	int maxFramerate;          // max FPS when limited

	// PBR
	bool useNormalMaps;        // enable normal map rendering
	bool useEnvMaps;           // enable environment reflections

	OGLConfig();               // set defaults
	void Load(const char *cfgFile);  // load from file
	void Save(const char *cfgFile);  // save to file
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLCONFIG_H
