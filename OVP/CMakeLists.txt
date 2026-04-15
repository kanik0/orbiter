# Copyright (c) Martin Schweiger
# Licensed under the MIT License

if(WIN32)
	add_subdirectory(GDIClient)
endif()

if(ORBITER_BUILD_D3D9CLIENT)
	add_subdirectory(D3D9Client)
endif()

if(NOT WIN32)
	add_subdirectory(OGLClient)
endif()
