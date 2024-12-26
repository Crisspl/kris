// Copyright (C) 2018-2023 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h


// I've moved out a tiny part of this example into a shared header for reuse, please open and read it.
#include "nbl/application_templates/MonoSystemMonoLoggerApplication.hpp"
//#include "SimpleWindowedApplication.hpp"
#include <Windows.h>

using namespace nbl;
using namespace core;
using namespace system;
using namespace asset;
using namespace video;

// kris
#include "resource_allocator.h"
#include "renderer.h"
#include "material.h"

struct GeometryCreator
{
#include "nbl/nblpack.h"
	struct CubeVertex
	{
		float pos[3];
		uint8_t color[4]; // normalized
		uint8_t uv[2];
		int8_t normal[3];
		uint8_t dummy[3];

		void setPos(float x, float y, float z) { pos[0] = x; pos[1] = y; pos[2] = z; }
		void translate(float dx, float dy, float dz) { pos[0] += dx; pos[1] += dy; pos[2] += dz; }
		void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { color[0] = r; color[1] = g; color[2] = b; color[3] = a; }
		void setNormal(int8_t x, int8_t y, int8_t z) { normal[0] = x; normal[1] = y; normal[2] = z; }
		void setUv(uint8_t u, uint8_t v) { uv[0] = u; uv[1] = v; }
	} PACK_STRUCT;

	struct return_type
	{
		nbl::asset::SVertexInputParams inputParams;
		nbl::asset::SPrimitiveAssemblyParams assemblyParams;
		nbl::asset::SBufferBinding<nbl::asset::ICPUBuffer> bindings[nbl::asset::ICPUMeshBuffer::MAX_ATTR_BUF_BINDING_COUNT];
		nbl::asset::SBufferBinding<nbl::asset::ICPUBuffer> indexBuffer;
		nbl::asset::E_INDEX_TYPE indexType;
		uint32_t indexCount;
		nbl::core::aabbox3df bbox;
	};

	static return_type createCubeMesh(const nbl::core::vector3df& size)
	{
		return_type retval;

		constexpr size_t vertexSize = sizeof(CubeVertex);
		retval.inputParams = { 0b1111u,0b1u,{
												{0u,nbl::asset::EF_R32G32B32_SFLOAT,offsetof(CubeVertex,pos)},
												{0u,nbl::asset::EF_R8G8B8A8_UNORM,offsetof(CubeVertex,color)},
												{0u,nbl::asset::EF_R8G8_USCALED,offsetof(CubeVertex,uv)},
												{0u,nbl::asset::EF_R8G8B8_SSCALED,offsetof(CubeVertex,normal)}
											},{vertexSize,nbl::asset::SVertexInputBindingParams::EVIR_PER_VERTEX} };

		// Create indices
		{
			retval.indexCount = 36u;
			auto indices = nbl::asset::ICPUBuffer::create({ sizeof(uint16_t) * retval.indexCount });
			indices->addUsageFlags(nbl::asset::IBuffer::EUF_INDEX_BUFFER_BIT);
			auto u = reinterpret_cast<uint16_t*>(indices->getPointer());
			for (uint32_t i = 0u; i < 6u; ++i)
			{
				u[i * 6 + 0] = 4 * i + 0;
				u[i * 6 + 1] = 4 * i + 1;
				u[i * 6 + 2] = 4 * i + 3;
				u[i * 6 + 3] = 4 * i + 1;
				u[i * 6 + 4] = 4 * i + 2;
				u[i * 6 + 5] = 4 * i + 3;
			}
			retval.indexBuffer = { 0ull,std::move(indices) };
		}

		// Create vertices
		auto vertices = nbl::asset::ICPUBuffer::create({ 24u * vertexSize });
		vertices->addUsageFlags(nbl::asset::IBuffer::EUF_VERTEX_BUFFER_BIT);
		CubeVertex* ptr = (CubeVertex*)vertices->getPointer();

		const core::vector3d<int8_t> normals[6] =
		{
			nbl::core::vector3d<int8_t>(0, 0, 1),
			nbl::core::vector3d<int8_t>(1, 0, 0),
			nbl::core::vector3d<int8_t>(0, 0, -1),
			nbl::core::vector3d<int8_t>(-1, 0, 0),
			nbl::core::vector3d<int8_t>(0, 1, 0),
			nbl::core::vector3d<int8_t>(0, -1, 0)
		};
		const core::vector3df pos[8] =
		{
			nbl::core::vector3df(-0.5f,-0.5f, 0.5f) * size,
			nbl::core::vector3df(0.5f,-0.5f, 0.5f) * size,
			nbl::core::vector3df(0.5f, 0.5f, 0.5f) * size,
			nbl::core::vector3df(-0.5f, 0.5f, 0.5f) * size,
			nbl::core::vector3df(0.5f,-0.5f,-0.5f) * size,
			nbl::core::vector3df(-0.5f, 0.5f,-0.5f) * size,
			nbl::core::vector3df(-0.5f,-0.5f,-0.5f) * size,
			nbl::core::vector3df(0.5f, 0.5f,-0.5f) * size
		};
		const nbl::core::vector2d<uint8_t> uvs[4] =
		{
			nbl::core::vector2d<uint8_t>(0, 1),
			nbl::core::vector2d<uint8_t>(1, 1),
			nbl::core::vector2d<uint8_t>(1, 0),
			nbl::core::vector2d<uint8_t>(0, 0)
		};

		for (size_t f = 0ull; f < 6ull; ++f)
		{
			const size_t v = f * 4ull;

			for (size_t i = 0ull; i < 4ull; ++i)
			{
				const nbl::core::vector3d<int8_t>& n = normals[f];
				const nbl::core::vector2d<uint8_t>& uv = uvs[i];
				ptr[v + i].setColor(255, 255, 255, 255);
				ptr[v + i].setNormal(n.X, n.Y, n.Z);
				ptr[v + i].setUv(uv.X, uv.Y);
			}

			switch (f)
			{
			case 0:
				ptr[v + 0].setPos(pos[0].X, pos[0].Y, pos[0].Z);
				ptr[v + 1].setPos(pos[1].X, pos[1].Y, pos[1].Z);
				ptr[v + 2].setPos(pos[2].X, pos[2].Y, pos[2].Z);
				ptr[v + 3].setPos(pos[3].X, pos[3].Y, pos[3].Z);
				break;
			case 1:
				ptr[v + 0].setPos(pos[1].X, pos[1].Y, pos[1].Z);
				ptr[v + 1].setPos(pos[4].X, pos[4].Y, pos[4].Z);
				ptr[v + 2].setPos(pos[7].X, pos[7].Y, pos[7].Z);
				ptr[v + 3].setPos(pos[2].X, pos[2].Y, pos[2].Z);
				break;
			case 2:
				ptr[v + 0].setPos(pos[4].X, pos[4].Y, pos[4].Z);
				ptr[v + 1].setPos(pos[6].X, pos[6].Y, pos[6].Z);
				ptr[v + 2].setPos(pos[5].X, pos[5].Y, pos[5].Z);
				ptr[v + 3].setPos(pos[7].X, pos[7].Y, pos[7].Z);
				break;
			case 3:
				ptr[v + 0].setPos(pos[6].X, pos[6].Y, pos[6].Z);
				ptr[v + 2].setPos(pos[3].X, pos[3].Y, pos[3].Z);
				ptr[v + 1].setPos(pos[0].X, pos[0].Y, pos[0].Z);
				ptr[v + 3].setPos(pos[5].X, pos[5].Y, pos[5].Z);
				break;
			case 4:
				ptr[v + 0].setPos(pos[3].X, pos[3].Y, pos[3].Z);
				ptr[v + 1].setPos(pos[2].X, pos[2].Y, pos[2].Z);
				ptr[v + 2].setPos(pos[7].X, pos[7].Y, pos[7].Z);
				ptr[v + 3].setPos(pos[5].X, pos[5].Y, pos[5].Z);
				break;
			case 5:
				ptr[v + 0].setPos(pos[0].X, pos[0].Y, pos[0].Z);
				ptr[v + 1].setPos(pos[6].X, pos[6].Y, pos[6].Z);
				ptr[v + 2].setPos(pos[4].X, pos[4].Y, pos[4].Z);
				ptr[v + 3].setPos(pos[1].X, pos[1].Y, pos[1].Z);
				break;
			}
		}
		retval.bindings[0] = { 0ull,std::move(vertices) };

		// Recalculate bounding box
		retval.indexType = nbl::asset::EIT_16BIT;
		retval.bbox = nbl::core::aabbox3df(-size * 0.5f, size * 0.5f);

		return retval;
	}
};

// For our Compute Shader
constexpr uint32_t WorkgroupSize = 256;
constexpr uint32_t WorkgroupCount = 2048;

// this time instead of defining our own `int main()` we derive from `nbl::system::IApplicationFramework` to play "nice" wil all platforms
class HelloComputeApp final : public nbl::application_templates::MonoSystemMonoLoggerApplication
{
		using base_t = application_templates::MonoSystemMonoLoggerApplication;
	public:
		// Generally speaking because certain platforms delay initialization from main object construction you should just forward and not do anything in the ctor
		using base_t::base_t;

		// we stuff all our work here because its a "single shot" app
		bool onAppInitialized(smart_refctd_ptr<ISystem>&& system) override
		{
			// Remember to call the base class initialization!
			if (!base_t::onAppInitialized(std::move(system)))
				return false;
			// `system` could have been null (see the comments in `MonoSystemMonoLoggerApplication::onAppInitialized` as for why)
			// use `MonoSystemMonoLoggerApplication::m_system` throughout the example instead!
			
			// Only Timeline Semaphores are supported in Nabla, there's no fences or binary semaphores.
			// Swapchains run on adaptors with empty submits that make them look like they work with Timeline Semaphores,
			// which has important side-effects we'll cover in another example.
			constexpr auto FinishedValue = 45;
			smart_refctd_ptr<ISemaphore> progress;

			// A `nbl::video::DeviceMemoryAllocator` is an interface to implement anything that can dish out free memory range to bind to back a `nbl::video::IGPUBuffer` or a `nbl::video::IGPUImage`
			// The Logical Device itself implements the interface and behaves as the most simple allocator, it will create a new `nbl::video::IDeviceMemoryAllocation` every single time.
			// We will cover allocators and suballocation in a later example.
			nbl::video::IDeviceMemoryAllocator::SAllocation allocation = {};

			// You should already know Vulkan and come here to save on the boilerplate, if you don't know what instances and instance extensions are, then find out.
			{
				// You generally want to default initialize any parameter structs
				nbl::video::IAPIConnection::SFeatures apiFeaturesToEnable = {};
				// generally you want to make your life easier during development
				apiFeaturesToEnable.validations = true;
				apiFeaturesToEnable.synchronizationValidation = true;
				// want to make sure we have this so we can name resources for vieweing in RenderDoc captures
				apiFeaturesToEnable.debugUtils = true;
				// create our Vulkan instance
				if (!(m_api=CVulkanConnection::create(smart_refctd_ptr(m_system),0,_NBL_APP_NAME_,smart_refctd_ptr(base_t::m_logger),apiFeaturesToEnable)))
					return logFail("Failed to crate an IAPIConnection!");
			}

			// We won't go deep into performing physical m_device selection in this example, we'll take any m_device with a compute queue.
			// Nabla has its own set of required baseline Vulkan features anyway, it won't report any m_device that doesn't meet them.
			nbl::video::IPhysicalDevice* physDev = nullptr;
			ILogicalDevice::SCreationParams params = {};
			for (auto physDevIt= m_api->getPhysicalDevices().begin(); physDevIt!= m_api->getPhysicalDevices().end(); physDevIt++)
			{
				const auto familyProps = (*physDevIt)->getQueueFamilyProperties();
				// this is the only "complicated" part, we want to create a queue that supports compute pipelines
				for (auto i=0; i<familyProps.size(); i++)
				if (familyProps[i].queueFlags.hasFlags(IQueue::FAMILY_FLAGS::COMPUTE_BIT))
				{
					physDev = *physDevIt;
					m_queueFamily = i;
					params.queueParams[m_queueFamily].count = 1;
					break;
				}
			}
			if (!physDev)
				return logFail("Failed to find any Physical Devices with Compute capable Queue Families!");

			// logical devices need to be created form physical devices which will actually let us create vulkan objects and use the physical m_device
			m_device = physDev->createLogicalDevice(std::move(params));
			if (!m_device)
				return logFail("Failed to create a Logical Device!");

			// A word about `nbl::asset::IAsset`s, whenever you see an `nbl::asset::ICPUSomething` you can be sure an `nbl::video::IGPUSomething exists, and they both inherit from `nbl::asset::ISomething`.
			// The convention is that an `ICPU` object represents a potentially Mutable (and in the past, Serializable) recipe for creating an `IGPU` object, and later examples will show automated systems for doing that.
			// The Assets always form a Directed Acyclic Graph and our type system enforces that property at compile time (i.e. an `IBuffer` cannot reference an `IImageView` even indirectly).
			// Another reason for the 1:1 pairing of types is that one can use a CPU-to-GPU associative cache (asset manager has a default one) and use the pointers to the CPU objects as UUIDs.
			// The ICPUShader is just a mutable container for source code (can be high level like HLSL needing compilation to SPIR-V or SPIR-V itself) held in an `nbl::asset::ICPUBuffer`.
			// They can be created: from buffers of code, by compilation from some other source code, or loaded from files (next example will do that).
			smart_refctd_ptr<nbl::asset::ICPUShader> cpuShader;
			{
				// Normally we'd use the ISystem and the IAssetManager to load shaders flexibly from (virtual) files for ease of development (syntax highlighting and Intellisense),
				// but I want to show the full process of assembling a shader from raw source code at least once.
				smart_refctd_ptr<nbl::asset::IShaderCompiler> compiler = make_smart_refctd_ptr<nbl::asset::CHLSLCompiler>(smart_refctd_ptr(m_system));

				// A simple shader that writes out the Global Invocation Index to the position it corresponds to in the buffer
				// Note the injection of a define from C++ to keep the workgroup size in sync.
				// P.S. We don't have an entry point name compiler option because we expect that future compilers should support multiple entry points, so for now there must be a single entry point called "main".
				constexpr const char* source = R"===(
					#pragma wave shader_stage(compute)

					[[vk::binding(0,0)]] RWStructuredBuffer<uint32_t> buff;

					[numthreads(WORKGROUP_SIZE,1,1)]
					void main(uint32_t3 ID : SV_DispatchThreadID)
					{
						buff[ID.x] = ID.x;
					}
				)===";

				// Yes we know workgroup sizes can come from specialization constants, however DXC has a problem with that https://github.com/microsoft/DirectXShaderCompiler/issues/3092
				const string WorkgroupSizeAsStr = std::to_string(WorkgroupSize);
				const IShaderCompiler::SMacroDefinition WorkgroupSizeDefine = {"WORKGROUP_SIZE",WorkgroupSizeAsStr};

				CHLSLCompiler::SOptions options = {};
				// really we should set it to `ESS_COMPUTE` since we know, but we'll test the `#pragma` handling fur teh lulz
				options.stage = asset::IShader::E_SHADER_STAGE::ESS_UNKNOWN;
				// want as much debug as possible
				options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_LINE_BIT;
				// this lets you source-level debug/step shaders in renderdoc
				if (physDev->getLimits().shaderNonSemanticInfo)
					options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_NON_SEMANTIC_BIT;
				// if you don't set the logger and source identifier you'll have no meaningful errors
				options.preprocessorOptions.sourceIdentifier = "embedded.comp.hlsl";
				options.preprocessorOptions.logger = m_logger.get();
				options.preprocessorOptions.extraDefines = {&WorkgroupSizeDefine,&WorkgroupSizeDefine+1};
				if (!(cpuShader=compiler->compileToSPIRV(source,options)))
					return logFail("Failed to compile following HLSL Shader:\n%s\n",source);
			}

			// Note how each ILogicalDevice method takes a smart-pointer r-value, so that the GPU objects refcount their dependencies
			smart_refctd_ptr<nbl::video::IGPUShader> shader = m_device->createShader(cpuShader.get());
			if (!shader)
				return logFail("Failed to create a GPU Shader, seems the Driver doesn't like the SPIR-V we're feeding it!\n");

			// the simplest example would have used push constants and BDA, but RenderDoc's debugging of that sucks, so I'll demonstrate "classical" binding of buffers with descriptors
			nbl::video::IGPUDescriptorSetLayout::SBinding bindings[1] = {
				{
					.binding=0,
					.type=nbl::asset::IDescriptor::E_TYPE::ET_STORAGE_BUFFER,
					.createFlags=IGPUDescriptorSetLayout::SBinding::E_CREATE_FLAGS::ECF_NONE, // not is not the time for descriptor indexing
					.stageFlags=IGPUShader::E_SHADER_STAGE::ESS_COMPUTE,
					.count=1,
				}
			};
			smart_refctd_ptr<IGPUDescriptorSetLayout> dsLayout = m_device->createDescriptorSetLayout(bindings);
			if (!dsLayout)
				return logFail("Failed to create a Descriptor Layout!\n");

			// Nabla actually has facilities for SPIR-V Reflection and "guessing" pipeline layouts for a given SPIR-V which we'll cover in a different example
			smart_refctd_ptr<nbl::video::IGPUPipelineLayout> pplnLayout = m_device->createPipelineLayout({},smart_refctd_ptr(dsLayout));
			if (!pplnLayout)
				return logFail("Failed to create a Pipeline Layout!\n");

			// We use strong typing on the pipelines (Compute, Graphics, Mesh, RT), since there's no reason to polymorphically switch between different pipelines
			smart_refctd_ptr<nbl::video::IGPUComputePipeline> pipeline;
			{
				IGPUComputePipeline::SCreationParams params = {};
				params.layout = pplnLayout.get();
				// Theoretically a blob of SPIR-V can contain multiple named entry points and one has to be chosen, in practice most compilers only support outputting one (and glslang used to require it be called "main")
				params.shader.entryPoint = "main";
				params.shader.shader = shader.get();
				// we'll cover the specialization constant API in another example
				if (!m_device->createComputePipelines(nullptr,{&params,1},&pipeline))
					return logFail("Failed to create pipelines (compile & link shaders)!\n");
			}

#define RMAP_INDEX_OUTPUT_BUF kris::FirstUsableResourceMapSlot
			static_assert(RMAP_INDEX_OUTPUT_BUF != kris::DefaultResourceMapSlot);

			m_Renderer.init(kris::refctd<nbl::video::ILogicalDevice>(m_device), m_queueFamily, &m_ResourceAlctr, physDev->getHostVisibleMemoryTypeBits());

			// Our Descriptor Sets track (refcount) resources written into them, so you can pretty much drop and forget whatever you write into them.
			// A later Descriptor Indexing example will test that this tracking is also correct for Update-After-Bind Descriptor Set bindings too.
			//smart_refctd_ptr<nbl::video::IGPUDescriptorSet> ds;
				
			// Allocate the memory
			{
				constexpr size_t BufferSize = sizeof(uint32_t)*WorkgroupSize*WorkgroupCount;

				// Always default the creation parameters, there's a lot of extra stuff for DirectX/CUDA interop and slotting into external engines you don't usually care about. 
				nbl::video::IGPUBuffer::SCreationParams params = {};
				params.size = BufferSize;
				// While the usages on `ICPUBuffers` are mere hints to our automated CPU-to-GPU conversion systems which need to be patched up anyway,
				// the usages on an `IGPUBuffer` are crucial to specify correctly.
				params.usage = IGPUBuffer::EUF_STORAGE_BUFFER_BIT;

				m_buffAllocation = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(params), physDev->getHostVisibleMemoryTypeBits());
				smart_refctd_ptr<IGPUBuffer> outputBuff = kris::refctd<nbl::video::IGPUBuffer>(m_buffAllocation->getBuffer());
				if (!outputBuff)
					return logFail("Failed to create a GPU Buffer of size %d!\n", params.size);

				// Naming objects is cool because not only errors (such as Vulkan Validation Layers) will show their names, but RenderDoc captures too.
				outputBuff->setObjectDebugName("My Output Buffer");
			}

#if 0
			auto renderpass = m_Renderer.createRenderpass(m_device.get(), nbl::asset::EF_R8G8B8A8_UNORM, nbl::asset::EF_D32_SFLOAT);
			kris::refctd<nbl::video::IGPUGraphicsPipeline> gfx;
			auto gfxlayout = m_device->createPipelineLayout({});
			{
				auto cubedata = GeometryCreator::createCubeMesh({ 0.5f, 0.5f, 0.5f });
				
				kris::refctd<kris::BufferResource> vtxbuf;
				{
					auto& vtxbuf_data = cubedata.bindings[0].buffer;

					nbl::video::IGPUBuffer::SCreationParams ci = {};
					ci.size = vtxbuf_data->getSize();
					ci.usage = nbl::asset::IBuffer::EUF_VERTEX_BUFFER_BIT;
					vtxbuf = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(ci), physDev->getHostVisibleMemoryTypeBits());
					void* ptr = vtxbuf->map(nbl::video::IDeviceMemoryAllocation::EMCAF_WRITE);
					memcpy(ptr, vtxbuf_data->getPointer(), vtxbuf_data->getSize());
					vtxbuf->unmap();
				}
				kris::refctd<kris::BufferResource> idxbuf;
				{
					auto& idxbuf_data = cubedata.indexBuffer.buffer;

					nbl::video::IGPUBuffer::SCreationParams ci = {};
					ci.size = idxbuf_data->getSize();
					ci.usage = nbl::asset::IBuffer::EUF_INDEX_BUFFER_BIT;
					idxbuf = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(ci), physDev->getHostVisibleMemoryTypeBits());
					void* ptr = idxbuf->map(nbl::video::IDeviceMemoryAllocation::EMCAF_WRITE);
					memcpy(ptr, idxbuf_data->getPointer(), idxbuf_data->getSize());
					idxbuf->unmap();
				}

				nbl::asset::SBlendParams blendParams = {};
				blendParams.blendParams[0u].srcColorFactor = nbl::asset::EBF_ONE;
				blendParams.blendParams[0u].dstColorFactor = nbl::asset::EBF_ZERO;
				blendParams.blendParams[0u].colorBlendOp = nbl::asset::EBO_ADD;
				blendParams.blendParams[0u].srcAlphaFactor = nbl::asset::EBF_ONE;
				blendParams.blendParams[0u].dstAlphaFactor = nbl::asset::EBF_ZERO;
				blendParams.blendParams[0u].alphaBlendOp = nbl::asset::EBO_ADD;
				blendParams.blendParams[0u].colorWriteMask = 0xfU;
				nbl::asset::SRasterizationParams rasterParams;
				rasterParams.polygonMode = nbl::asset::EPM_FILL;
				rasterParams.faceCullingMode = nbl::asset::EFCM_NONE;
				rasterParams.depthWriteEnable = true;

				const char* vs_source = 
					R"(
#pragma wave shader_stage(vertex)

// set 1, binding 0
[[vk::binding(0, 1)]]
cbuffer CameraData
{
    SBasicViewParameters params;
};

struct VSInput
{
	[[vk::location(0)]] float3 position : POSITION;
	[[vk::location(1)]] float4 color : COLOR;
	[[vk::location(2)]] float2 uv : TEXCOORD;
	[[vk::location(3)]] float3 normal : NORMAL;
};

struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(params.MVP, float4(input.position, 1.0));
    output.color = input.color;
    
    return output;
})";
				const char* ps_source =
					R"(
#pragma wave shader_stage(fragment)

struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET
{
	return input.color;
}
)";
				kris::refctd<nbl::video::IGPUShader> vs_shader;
				kris::refctd<nbl::video::IGPUShader> ps_shader;
				{
					kris::refctd<nbl::asset::ICPUShader> vs_cpu;
					kris::refctd<nbl::asset::ICPUShader> ps_cpu;

					kris::refctd<nbl::asset::IShaderCompiler> compiler = nbl::core::make_smart_refctd_ptr<nbl::asset::CHLSLCompiler>(smart_refctd_ptr(m_system));

					CHLSLCompiler::SOptions options = {};
					// really we should set it to `ESS_COMPUTE` since we know, but we'll test the `#pragma` handling fur teh lulz
					options.stage = nbl::asset::IShader::E_SHADER_STAGE::ESS_VERTEX;
					// want as much debug as possible
					options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_LINE_BIT;
					// this lets you source-level debug/step shaders in renderdoc
					if (physDev->getLimits().shaderNonSemanticInfo)
						options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_NON_SEMANTIC_BIT;
					// if you don't set the logger and source identifier you'll have no meaningful errors
					options.preprocessorOptions.sourceIdentifier = "embedded.comp.hlsl";
					options.preprocessorOptions.logger = m_logger.get();
					//options.preprocessorOptions.extraDefines = { &WorkgroupSizeDefine,&WorkgroupSizeDefine + 1 };

					vs_cpu = compiler->compileToSPIRV(vs_source, options);

					options.stage = nbl::asset::IShader::E_SHADER_STAGE::ESS_FRAGMENT;
					ps_cpu = compiler->compileToSPIRV(ps_source, options);

					vs_shader = m_device->createShader(vs_cpu.get());
					ps_shader = m_device->createShader(ps_cpu.get());
				}

				IGPUShader::SSpecInfo specInfo[2] = {
				{.shader = vs_shader.get()},
				{.shader = ps_shader.get()},
				};

				nbl::video::IGPUGraphicsPipeline::SCreationParams params[1] = {};
				params[0].layout = gfxlayout.get();
				params[0].shaders = specInfo;
				params[0].cached = {
					.vertexInput = cubedata.inputParams,
					.primitiveAssembly = cubedata.assemblyParams,
					.rasterization = rasterParams,
					.blend = blendParams,
				};
				params[0].renderpass = renderpass.get();

				m_device->createGraphicsPipelines(nullptr, params, &gfx);
			}
#endif

			// set rmap
			{
				m_Renderer.resourceMap[RMAP_INDEX_OUTPUT_BUF] = m_buffAllocation.get();
			}

			m_mtl = nbl::core::make_smart_refctd_ptr<kris::Material>(1U << kris::Material::BasePass);
			// set up material
			{
				for (uint32_t i = 0U; i < kris::FramesInFlight; ++i)
				{
					kris::DescriptorSet ds = m_Renderer.createDescriptorSet(dsLayout.get());
					m_mtl->m_ds3[i] = std::move(ds);
				}

				m_mtl->m_computePso[kris::Material::BasePass] = pipeline;

				m_mtl->m_bindings[0].rmapIx = RMAP_INDEX_OUTPUT_BUF;
				m_mtl->m_bindings[0].descCategory = nbl::asset::IDescriptor::EC_BUFFER;
				m_mtl->m_bindings[0].info.buffer.format = nbl::asset::EF_UNKNOWN;
				m_mtl->m_bindings[0].info.buffer.offset = 0U;
				m_mtl->m_bindings[0].info.buffer.size = kris::Size_FullRange;
			}

			// There's just one caveat, the Queues tracking what resources get used in a submit do it via an event queue that needs to be polled to clear.
			// The tracking causes circular references from the resource back to the m_device, so unless we poll at the end of the application, they resources used by last submit will leak.
			// We could of-course make a very lazy thread that wakes up every second or so and runs this GC on the queues, but we think this is enough book-keeping for the users.
			//m_device->waitIdle();

			return true;
		}

		// Platforms like WASM expect the main entry point to periodically return control, hence if you want a crossplatform app, you have to let the framework deal with your "game loop"
		void workLoopBody() override 
		{
			if (!m_buffAllocation->map(IDeviceMemoryAllocation::EMCAF_READ))
			{
				logFail("Failed to map the Device Memory!\n");
				return;
			}

			m_Renderer.beginFrame();

			{
				kris::CommandRecorder cmdrec = m_Renderer.createCommandRecorder();

				cmdrec.cmdbuf->beginDebugMarker("My Compute Dispatch", core::vectorSIMDf(0, 1, 0, 1));
				//cmdrec.cmdbuf->bindComputePipeline(pipeline.get());
				//cmdrec.cmdbuf->bindDescriptorSets(nbl::asset::EPBP_COMPUTE, pplnLayout.get(), 0, 1, &ds.get());
				//cmdrec.bindDescriptorSet(nbl::asset::EPBP_COMPUTE, pplnLayout.get(), 0U, &ds);
				//cmdrec.cmdbuf->dispatch(WorkgroupCount, 1, 1);
				cmdrec.dispatch(m_device.get(), m_Renderer.getCurrentFrameIx(), kris::Material::BasePass, &m_Renderer.resourceMap, m_mtl.get(), WorkgroupCount, 1, 1);
				cmdrec.cmdbuf->endDebugMarker();

				m_Renderer.consumeAsPass(kris::Material::BasePass, std::move(cmdrec));
			}

			m_api->startCapture();
			m_Renderer.submit(m_device->getQueue(m_queueFamily, 0), asset::PIPELINE_STAGE_FLAGS::COMPUTE_SHADER_BIT);
			m_api->endCapture();

			m_Renderer.blockForCurrentFrame(); // wait to read CS result on CPU

			m_Renderer.endFrame();

			m_buffAllocation->invalidate(m_device.get());
			auto buffData = reinterpret_cast<const uint32_t*>(m_buffAllocation->getMappedPtr());

			for (auto i = 0; i < WorkgroupSize * WorkgroupCount; i++)
			{
				if (buffData[i] != i)
				{
					logFail("DWORD at position %d doesn't match!\n", i);
					break;
				}
			}

			m_buffAllocation->unmap();
		}

		// Whether to keep invoking the above. In this example because its headless GPU compute, we do all the work in the app initialization.
		bool keepRunning() override {return true;}

	private:
		kris::refctd<nbl::video::ILogicalDevice> m_device;
		uint8_t m_queueFamily = 0;
		kris::ResourceAllocator m_ResourceAlctr;
		kris::Renderer m_Renderer;
		kris::refctd<kris::BufferResource> m_buffAllocation;
		kris::refctd<kris::Material> m_mtl;

		smart_refctd_ptr<nbl::video::CVulkanConnection> m_api;
};


NBL_MAIN_FUNC(HelloComputeApp)