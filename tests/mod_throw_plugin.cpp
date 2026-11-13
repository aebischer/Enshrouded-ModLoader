#include "../src/mod_api.h"
#include <stdexcept>
extern "C" __declspec(dllexport) int EnshroudedModInitialize(const EnshroudedModContext*) {throw std::runtime_error("controlled init failure");}
