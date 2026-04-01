#pragma once

#include <memory>
#include <vector>

class IPlugin;

std::vector<std::unique_ptr<IPlugin>> CreateBuiltinPlugins();
