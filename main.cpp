// Copyright (C) 2018-2023 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h


// I've moved out a tiny part of this example into a shared header for reuse, please open and read it.
#include "nbl/application_templates/MonoSystemMonoLoggerApplication.hpp"
//#include "SimpleWindowedApplication.hpp"

#include "SimpleWindowedApplication.hpp"
#include "InputSystem.hpp"
#include "CEventCallback.hpp"

#include "CCamera.hpp"
#include "SBasicViewParameters.hlsl"

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
#include "material_builder.h"
#include "mesh.h"
#include "resource_utils.h"

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

class CSwapchainFramebuffersAndDepth final : public nbl::video::CDefaultSwapchainFramebuffers
{
	using base_t = CDefaultSwapchainFramebuffers;

public:
	template<typename... Args>
	inline CSwapchainFramebuffersAndDepth(ILogicalDevice* device, const asset::E_FORMAT _desiredDepthFormat, Args&&... args) : CDefaultSwapchainFramebuffers(device, std::forward<Args>(args)...)
	{
		const IPhysicalDevice::SImageFormatPromotionRequest req = {
			.originalFormat = _desiredDepthFormat,
			.usages = {IGPUImage::EUF_RENDER_ATTACHMENT_BIT}
		};
		m_depthFormat = m_device->getPhysicalDevice()->promoteImageFormat(req, IGPUImage::TILING::OPTIMAL);

		const static IGPURenderpass::SCreationParams::SDepthStencilAttachmentDescription depthAttachments[] = {
			{{
				{
					.format = m_depthFormat,
					.samples = IGPUImage::ESCF_1_BIT,
					.mayAlias = false
				},
			/*.loadOp = */{IGPURenderpass::LOAD_OP::CLEAR},
			/*.storeOp = */{IGPURenderpass::STORE_OP::STORE},
			/*.initialLayout = */{IGPUImage::LAYOUT::UNDEFINED}, // because we clear we don't care about contents
			/*.finalLayout = */{IGPUImage::LAYOUT::ATTACHMENT_OPTIMAL} // transition to presentation right away so we can skip a barrier
		}},
		IGPURenderpass::SCreationParams::DepthStencilAttachmentsEnd
		};
		m_params.depthStencilAttachments = depthAttachments;

		static IGPURenderpass::SCreationParams::SSubpassDescription subpasses[] = {
			m_params.subpasses[0],
			IGPURenderpass::SCreationParams::SubpassesEnd
		};
		subpasses[0].depthStencilAttachment.render = { .attachmentIndex = 0,.layout = IGPUImage::LAYOUT::ATTACHMENT_OPTIMAL };
		m_params.subpasses = subpasses;
	}

protected:
	inline bool onCreateSwapchain_impl(const uint8_t qFam) override
	{
		auto device = const_cast<ILogicalDevice*>(m_renderpass->getOriginDevice());

		const auto depthFormat = m_renderpass->getCreationParameters().depthStencilAttachments[0].format;
		const auto& sharedParams = getSwapchain()->getCreationParameters().sharedParams;
		auto image = device->createImage({ IImage::SCreationParams{
			.type = IGPUImage::ET_2D,
			.samples = IGPUImage::ESCF_1_BIT,
			.format = depthFormat,
			.extent = {sharedParams.width,sharedParams.height,1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.depthUsage = IGPUImage::EUF_RENDER_ATTACHMENT_BIT
		} });

		device->allocate(image->getMemoryReqs(), image.get());

		m_depthBuffer = device->createImageView({
			.flags = IGPUImageView::ECF_NONE,
			.subUsages = IGPUImage::EUF_RENDER_ATTACHMENT_BIT,
			.image = std::move(image),
			.viewType = IGPUImageView::ET_2D,
			.format = depthFormat,
			.subresourceRange = {IGPUImage::EAF_DEPTH_BIT,0,1,0,1}
			});

		const auto retval = base_t::onCreateSwapchain_impl(qFam);
		m_depthBuffer = nullptr;
		return retval;
	}

	inline smart_refctd_ptr<IGPUFramebuffer> createFramebuffer(IGPUFramebuffer::SCreationParams&& params) override
	{
		params.depthStencilAttachments = &m_depthBuffer.get();
		return m_device->createFramebuffer(std::move(params));
	}

	E_FORMAT m_depthFormat;
	// only used to pass a parameter from `onCreateSwapchain_impl` to `createFramebuffer`
	smart_refctd_ptr<IGPUImageView> m_depthBuffer;
};

// For our Compute Shader
constexpr uint32_t WorkgroupSize = 256;
constexpr uint32_t WorkgroupCount = 2048;

// this time instead of defining our own `int main()` we derive from `nbl::system::IApplicationFramework` to play "nice" wil all platforms
class HelloComputeApp final : public examples::SimpleWindowedApplication
{
	using device_base_t = examples::SimpleWindowedApplication;
	using clock_t = std::chrono::steady_clock;

	constexpr static inline uint32_t WIN_W = 1280, WIN_H = 720;

	public:
		inline HelloComputeApp(const path& _localInputCWD, const path& _localOutputCWD, const path& _sharedInputCWD, const path& _sharedOutputCWD)
			: IApplicationFramework(_localInputCWD, _localOutputCWD, _sharedInputCWD, _sharedOutputCWD) {
		}

		virtual SPhysicalDeviceFeatures getRequiredDeviceFeatures() const override
		{
			auto retval = device_base_t::getRequiredDeviceFeatures();
			//retval.geometryShader = true;
			return retval;
		}

		inline core::vector<video::SPhysicalDeviceFilter::SurfaceCompatibility> getSurfaces() const override
		{
			if (!m_surface)
			{
				{
					auto windowCallback = core::make_smart_refctd_ptr<CEventCallback>(smart_refctd_ptr(m_inputSystem), smart_refctd_ptr(m_logger));
					nbl::ui::IWindow::SCreationParams params = {};
					params.callback = core::make_smart_refctd_ptr<nbl::video::ISimpleManagedSurface::ICallback>();
					params.width = WIN_W;
					params.height = WIN_H;
					params.x = 32;
					params.y = 32;
					params.flags = ui::IWindow::ECF_HIDDEN | nbl::ui::IWindow::ECF_BORDERLESS | nbl::ui::IWindow::ECF_RESIZABLE;
					params.windowCaption = "GeometryCreatorApp";
					params.callback = windowCallback;
					const_cast<std::remove_const_t<decltype(m_window)>&>(m_window) = m_winMgr->createWindow(std::move(params));
				}

				auto surface = CSurfaceVulkanWin32::create(smart_refctd_ptr(m_api), smart_refctd_ptr_static_cast<nbl::ui::IWindowWin32>(m_window));
				const_cast<std::remove_const_t<decltype(m_surface)>&>(m_surface) = nbl::video::CSimpleResizeSurface<CSwapchainFramebuffersAndDepth>::create(std::move(surface));
			}

			if (m_surface)
				return { {m_surface->getSurface()/*,EQF_NONE*/} };

			return {};
		}

		// we stuff all our work here because its a "single shot" app
		bool onAppInitialized(smart_refctd_ptr<ISystem>&& system) override
		{
			m_inputSystem = make_smart_refctd_ptr<InputSystem>(logger_opt_smart_ptr(smart_refctd_ptr(m_logger)));

			if (!device_base_t::onAppInitialized(smart_refctd_ptr(system)))
				return false;

			ISwapchain::SCreationParams swapchainParams = { .surface = m_surface->getSurface() };
			if (!swapchainParams.deduceFormat(m_physicalDevice))
				return logFail("Could not choose a Surface Format for the Swapchain!");

			// Subsequent submits don't wait for each other, hence its important to have External Dependencies which prevent users of the depth attachment overlapping.
			const static IGPURenderpass::SCreationParams::SSubpassDependency dependencies[] = {
			// wipe-transition of Color to ATTACHMENT_OPTIMAL
			{
				.srcSubpass = IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.dstSubpass = 0,
				.memoryBarrier = {
					// last place where the depth can get modified in previous frame
					.srcStageMask = PIPELINE_STAGE_FLAGS::LATE_FRAGMENT_TESTS_BIT,
					// only write ops, reads can't be made available
					.srcAccessMask = ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					// destination needs to wait as early as possible
					.dstStageMask = PIPELINE_STAGE_FLAGS::EARLY_FRAGMENT_TESTS_BIT,
					// because of depth test needing a read and a write
					.dstAccessMask = ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_READ_BIT
				}
				// leave view offsets and flags default
			},
			// color from ATTACHMENT_OPTIMAL to PRESENT_SRC
			{
				.srcSubpass = 0,
				.dstSubpass = IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.memoryBarrier = {
					// last place where the depth can get modified
					.srcStageMask = PIPELINE_STAGE_FLAGS::COLOR_ATTACHMENT_OUTPUT_BIT,
					// only write ops, reads can't be made available
					.srcAccessMask = ACCESS_FLAGS::COLOR_ATTACHMENT_WRITE_BIT
					// spec says nothing is needed when presentation is the destination
				}
				// leave view offsets and flags default
			},
			IGPURenderpass::SCreationParams::DependenciesEnd
			};

			// TODO: promote the depth format if D16 not supported, or quote the spec if there's guaranteed support for it
			auto scResources = std::make_unique<CSwapchainFramebuffersAndDepth>(m_device.get(), EF_D16_UNORM, swapchainParams.surfaceFormat.format, dependencies);

			auto* renderpass = scResources->getRenderpass();

			if (!renderpass)
				return logFail("Failed to create Renderpass!");

			auto gQueue = getGraphicsQueue();
			if (!m_surface || !m_surface->init(gQueue, std::move(scResources), swapchainParams.sharedParams))
				return logFail("Could not create Window & Surface or initialize the Surface!");

			m_winMgr->setWindowSize(m_window.get(), WIN_W, WIN_H);
			m_surface->recreateSwapchain();

			m_Renderer.init(kris::refctd<nbl::video::ILogicalDevice>(m_device), kris::refctd<nbl::video::IGPURenderpass>(renderpass), 
				gQueue->getFamilyIndex(), &m_ResourceAlctr, m_physicalDevice->getHostVisibleMemoryTypeBits());

#define RMAP_OUTPUT_BUF kris::FirstUsableResourceMapSlot
			static_assert(RMAP_OUTPUT_BUF >= kris::FirstUsableResourceMapSlot);

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

				m_buffAllocation = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(params), m_physicalDevice->getHostVisibleMemoryTypeBits());
				smart_refctd_ptr<IGPUBuffer> outputBuff = kris::refctd<nbl::video::IGPUBuffer>(m_buffAllocation->getBuffer());
				if (!outputBuff)
					return logFail("Failed to create a GPU Buffer of size %d!\n", params.size);

				// Naming objects is cool because not only errors (such as Vulkan Validation Layers) will show their names, but RenderDoc captures too.
				outputBuff->setObjectDebugName("My Output Buffer");
			}

			//auto renderpass = m_Renderer.createRenderpass(m_device.get(), nbl::asset::EF_R8G8B8A8_UNORM, nbl::asset::EF_D32_SFLOAT);

			kris::MaterialBuilder mtlbuilder(m_system.get()); 
			
			{
				m_cubedata = GeometryCreator::createCubeMesh({ 0.5f, 0.5f, 0.5f });
				
				kris::ResourceUtils utils(m_device.get(), getTransferUpQueue()->getFamilyIndex(), &m_ResourceAlctr);
				utils.beginTransferPass();

				kris::refctd<kris::BufferResource> vtxbuf;
				{
					auto& vtxbuf_data = m_cubedata.bindings[0].buffer;

					nbl::video::IGPUBuffer::SCreationParams ci = {};
					ci.size = vtxbuf_data->getSize();
					ci.usage = nbl::core::bitflag(nbl::asset::IBuffer::EUF_VERTEX_BUFFER_BIT) |
						nbl::video::IGPUBuffer::EUF_TRANSFER_DST_BIT;
					vtxbuf = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(ci), m_physicalDevice->getDeviceLocalMemoryTypeBits());
					utils.uploadBufferData(vtxbuf.get(), 0U, vtxbuf->getSize(), vtxbuf_data->getPointer());
				}
				
				kris::refctd<kris::BufferResource> idxbuf;
				{
					auto& idxbuf_data = m_cubedata.indexBuffer.buffer;

					nbl::video::IGPUBuffer::SCreationParams ci = {};
					ci.size = idxbuf_data->getSize();
					ci.usage = nbl::core::bitflag(nbl::asset::IBuffer::EUF_INDEX_BUFFER_BIT) |
						nbl::video::IGPUBuffer::EUF_TRANSFER_DST_BIT;
					idxbuf = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(ci), m_physicalDevice->getDeviceLocalMemoryTypeBits());
					utils.uploadBufferData(idxbuf.get(), 0U, idxbuf->getSize(), idxbuf_data->getPointer());
				}
				utils.endPassAndSubmit(getTransferUpQueue());
				utils.blockForSubmit();


				m_mesh = nbl::core::make_smart_refctd_ptr<kris::Mesh>();
				m_mesh->m_mtl = mtlbuilder.buildGfxMaterial(&m_Renderer, m_logger.get(), localInputCWD / "materials/cube.mat");
				m_mesh->m_vtxBuf = std::move(vtxbuf);
				m_mesh->m_idxBuf = std::move(idxbuf);
				m_mesh->m_idxCount = m_cubedata.indexCount;
				m_mesh->m_idxtype = m_cubedata.indexType;
				m_mesh->m_vtxinput = m_cubedata.inputParams;
			}


			{
				// set rmap
				m_Renderer.resourceMap[RMAP_OUTPUT_BUF] = m_buffAllocation.get();

				m_mtl = mtlbuilder.buildComputeMaterial(&m_Renderer, m_logger.get(), localInputCWD / "materials/hellocompute.mat");
			}


			// There's just one caveat, the Queues tracking what resources get used in a submit do it via an event queue that needs to be polled to clear.
			// The tracking causes circular references from the resource back to the m_device, so unless we poll at the end of the application, they resources used by last submit will leak.
			// We could of-course make a very lazy thread that wakes up every second or so and runs this GC on the queues, but we think this is enough book-keeping for the users.
			//m_device->waitIdle();

			// camera
			{
				core::vectorSIMDf cameraPosition(-5.81655884, 2.58630896, -4.23974705);
				core::vectorSIMDf cameraTarget(-0.349590302, -0.213266611, 0.317821503);
				matrix4SIMD projectionMatrix = matrix4SIMD::buildProjectionMatrixPerspectiveFovLH(core::radians(60.0f), float(WIN_W) / WIN_H, 0.1, 10000);
				camera = Camera(cameraPosition, cameraTarget, projectionMatrix, 1.069f, 0.4f);
			}

			m_winMgr->show(m_window.get());
			oracle.reportBeginFrameRecord();

			return true;
		}

		// Platforms like WASM expect the main entry point to periodically return control, hence if you want a crossplatform app, you have to let the framework deal with your "game loop"
		void workLoopBody() override 
		{
			m_inputSystem->getDefaultMouse(&mouse);
			m_inputSystem->getDefaultKeyboard(&keyboard);

			auto updatePresentationTimestamp = [&]()
				{
					m_currentImageAcquire = m_surface->acquireNextImage();

					oracle.reportEndFrameRecord();
					const auto timestamp = oracle.getNextPresentationTimeStamp();
					oracle.reportBeginFrameRecord();

					return timestamp;
				};

			const auto nextPresentationTimestamp = updatePresentationTimestamp();

			if (!m_currentImageAcquire)
				return;



			if (!m_buffAllocation->map(IDeviceMemoryAllocation::EMCAF_READ))
			{
				logFail("Failed to map the Device Memory!\n");
				return;
			}

			camera.beginInputProcessing(nextPresentationTimestamp);
			mouse.consumeEvents([&](const nbl::ui::IMouseEventChannel::range_t& events) -> void { camera.mouseProcess(events); }, m_logger.get());
			keyboard.consumeEvents([&](const nbl::ui::IKeyboardEventChannel::range_t& events) -> void { camera.keyboardProcess(events); }, m_logger.get());
			camera.endInputProcessing(nextPresentationTimestamp);

			m_Renderer.beginFrame(&camera);

			{
				kris::CommandRecorder cmdrec = m_Renderer.createCommandRecorder(kris::Material::BasePass);

				cmdrec.cmdbuf->beginDebugMarker("My Compute Dispatch", core::vectorSIMDf(0, 1, 0, 1));
				//cmdrec.cmdbuf->bindComputePipeline(pipeline.get());
				//cmdrec.cmdbuf->bindDescriptorSets(nbl::asset::EPBP_COMPUTE, pplnLayout.get(), 0, 1, &ds.get());
				//cmdrec.bindDescriptorSet(nbl::asset::EPBP_COMPUTE, pplnLayout.get(), 0U, &ds);
				//cmdrec.cmdbuf->dispatch(WorkgroupCount, 1, 1);
				cmdrec.dispatch(m_device.get(), kris::Material::BasePass, m_mtl.get(), WorkgroupCount, 1, 1);
				cmdrec.cmdbuf->endDebugMarker();

				asset::SViewport viewport;
				{
					viewport.minDepth = 1.f;
					viewport.maxDepth = 0.f;
					viewport.x = 0u;
					viewport.y = 0u;
					viewport.width = m_window->getWidth();
					viewport.height = m_window->getHeight();
				}
				cmdrec.cmdbuf->setViewport(0u, 1u, &viewport);

				VkRect2D scissor =
				{
					.offset = { 0, 0 },
					.extent = { m_window->getWidth(), m_window->getHeight() },
				};
				cmdrec.cmdbuf->setScissor(0u, 1u, &scissor);


				{
					const VkRect2D currentRenderArea =
					{
						.offset = {0,0},
						.extent = {m_window->getWidth(),m_window->getHeight()}
					};

					const IGPUCommandBuffer::SClearColorValue clearValue = { .float32 = {1.f,0.f,0.f,1.f} };
					const IGPUCommandBuffer::SClearDepthStencilValue depthValue = { .depth = 0.f };
					auto scRes = static_cast<CDefaultSwapchainFramebuffers*>(m_surface->getSwapchainResources());
					const IGPUCommandBuffer::SRenderpassBeginInfo info =
					{
						.framebuffer = scRes->getFramebuffer(m_currentImageAcquire.imageIndex),
						.colorClearValues = &clearValue,
						.depthStencilClearValues = &depthValue,
						.renderArea = currentRenderArea
					};

					cmdrec.cmdbuf->beginRenderPass(info, IGPUCommandBuffer::SUBPASS_CONTENTS::INLINE);
				}

				cmdrec.drawMesh(m_device.get(), kris::Material::BasePass, m_mesh.get());

				cmdrec.cmdbuf->endRenderPass();

				m_Renderer.consumeAsPass(kris::Material::BasePass, std::move(cmdrec));
			}

			//m_api->startCapture();
			auto rendered = m_Renderer.submit(getGraphicsQueue(),
				nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS>(nbl::asset::PIPELINE_STAGE_FLAGS::COMPUTE_SHADER_BIT) | nbl::asset::PIPELINE_STAGE_FLAGS::ALL_GRAPHICS_BITS);
			//m_api->endCapture();

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

			m_surface->present(m_currentImageAcquire.imageIndex, { &rendered, 1U });
		}

		inline bool keepRunning() override
		{
			if (m_surface->irrecoverable())
				return false;

			return true;
		}

		inline bool onAppTerminated() override
		{
			m_device->waitIdle();
			return device_base_t::onAppTerminated();
		}

	private:
		smart_refctd_ptr<nbl::ui::IWindow> m_window;
		smart_refctd_ptr<CSimpleResizeSurface<CSwapchainFramebuffersAndDepth>> m_surface;

		ISimpleManagedSurface::SAcquireResult m_currentImageAcquire = {};

		core::smart_refctd_ptr<InputSystem> m_inputSystem;
		InputSystem::ChannelReader<nbl::ui::IMouseEventChannel> mouse;
		InputSystem::ChannelReader<nbl::ui::IKeyboardEventChannel> keyboard;

		Camera camera = Camera(core::vectorSIMDf(0, 0, 0), core::vectorSIMDf(0, 0, 0), core::matrix4SIMD());
		video::CDumbPresentationOracle oracle;

		GeometryCreator::return_type m_cubedata;

		kris::ResourceAllocator m_ResourceAlctr;
		kris::Renderer m_Renderer;

		kris::refctd<kris::Mesh> m_mesh;

		kris::refctd<kris::BufferResource> m_buffAllocation;
		kris::refctd<kris::ComputeMaterial> m_mtl;
};


NBL_MAIN_FUNC(HelloComputeApp)