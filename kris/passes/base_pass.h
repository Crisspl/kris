#pragma once

#include "pass_common.h"
#include "../kris_common.h"
#include "../material.h"
#include "../resource_allocator.h"

namespace kris::base_pass
{
	bool createPassResources(PassResources* resources, 
		nbl::video::ILogicalDevice* device, 
		ResourceAllocator* alctr, 
		ImageResource* colorimages[FramesInFlight],
		ImageResource* depthimage,
		uint32_t width, uint32_t height);
}