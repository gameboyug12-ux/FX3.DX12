#pragma once
#include "fx3/Assets.hpp"
#include <Windows.h>

namespace fx3 {
int RunRenderer(HINSTANCE instance, int showCommand, const Mesh& mesh, const Texture& texture);
}
