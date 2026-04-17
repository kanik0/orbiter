#version 410 core
// Depth-only fragment program — GL writes gl_FragDepth automatically; we
// only need the stage to exist so the FBO's depth attachment receives
// rasterised values.
void main() {}
