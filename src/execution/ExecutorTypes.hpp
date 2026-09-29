#ifndef EXECUTOR_TYPES_HPP
#define EXECUTOR_TYPES_HPP

#include <memory>

#include "render/RenderTypes.hpp"

namespace anasa
{

struct RenderTask
{
    std::shared_ptr<RenderJob> job;
    int tileIndex = 0;
};

} // namespace anasa

#endif // EXECUTOR_TYPES_HPP