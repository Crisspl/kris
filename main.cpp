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
#include "kris/resource_allocator.h"
#include "kris/renderer.h"
#include "kris/material.h"
#include "kris/material_builder.h"
#include "kris/mesh.h"
#include "kris/scene.h"
#include "kris/resource_utils.h"

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
class KrisTestApp final : public examples::SimpleWindowedApplication
{
	using device_base_t = examples::SimpleWindowedApplication;
	using clock_t = std::chrono::steady_clock;

	constexpr static inline uint32_t WIN_W = 1280, WIN_H = 720;

	public:
		inline KrisTestApp(const path& _localInputCWD, const path& _localOutputCWD, const path& _sharedInputCWD, const path& _sharedOutputCWD)
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
			return { {m_surface.get()/*,EQF_NONE*/} };
		}

		bool onAppInitialized(smart_refctd_ptr<ISystem>&& system) override
		{
			m_inputSystem = make_smart_refctd_ptr<InputSystem>(logger_opt_smart_ptr(smart_refctd_ptr(m_logger)));

			if (!device_base_t::onAppInitialized(smart_refctd_ptr(system)))
				return false;

			auto gQueue = getGraphicsQueue();

			{
				// window and surface
				{
					auto windowCallback = core::make_smart_refctd_ptr<CEventCallback>(smart_refctd_ptr(m_inputSystem), smart_refctd_ptr(m_logger));
					nbl::ui::IWindow::SCreationParams params = {};
					params.callback = core::make_smart_refctd_ptr<nbl::video::ISimpleManagedSurface::ICallback>();
					params.width = WIN_W;
					params.height = WIN_H;
					params.x = 32;
					params.y = 32;
					params.flags = ui::IWindow::ECF_HIDDEN | nbl::ui::IWindow::ECF_BORDERLESS | nbl::ui::IWindow::ECF_RESIZABLE;
					params.windowCaption = "KRIS-TestApp";
					params.callback = windowCallback;
					m_window = m_winMgr->createWindow(std::move(params));

					m_winMgr->setWindowSize(m_window.get(), WIN_W, WIN_H);

					m_surface = CSurfaceVulkanWin32::create(smart_refctd_ptr(m_api), smart_refctd_ptr_static_cast<nbl::ui::IWindowWin32>(m_window));
				}

				// swapchain
				{
					nbl::video::ISwapchain::SSharedCreationParams sci;
					// 0s are invalid values, so they indicate we want them deduced
					sci.width = 0;
					sci.height = 0;
					sci.minImageCount = kris::FramesInFlight;

					sci.deduce(m_device->getPhysicalDevice(), m_surface.get());
					KRIS_ASSERT(sci.minImageCount == kris::FramesInFlight);

					ISwapchain::SCreationParams ci = {
							.surface = core::smart_refctd_ptr<ISurface>(m_surface),
							.surfaceFormat = {},
							.sharedParams = sci
							// we're not going to support concurrent sharing in this simple class
					};

					const bool success = ci.deduceFormat(m_device->getPhysicalDevice());
					KRIS_ASSERT(success);

					if (success)
					{
						m_sc = CVulkanSwapchain::create(kris::refctd(m_device), std::move(ci));
						KRIS_ASSERT(m_sc->getImageCount() == kris::FramesInFlight);
					}
				}

				// image acquire semaphores
				{
					for (uint32_t i = 0U; i < kris::FramesInFlight; ++i)
					{
						m_imgacqSemaphore[i] = m_device->createSemaphore(0ULL);
					}
				}
			}

			m_assetMgr = nbl::core::make_smart_refctd_ptr<nbl::asset::IAssetManager>(kris::refctd(m_system));

			// Allocate the memory
			// allocate image (texture for cube mesh)
			kris::refctd<kris::ImageResource> imageResource;
			kris::refctd<nbl::asset::ICPUImage> cpuimg;
			{
				constexpr auto cachingFlags = static_cast<IAssetLoader::E_CACHING_FLAGS>(IAssetLoader::ECF_DONT_CACHE_REFERENCES & IAssetLoader::ECF_DONT_CACHE_TOP_LEVEL);
				IAssetLoader::SAssetLoadParams loadParams(0ull, nullptr, cachingFlags);
				auto imageBundle = m_assetMgr->getAsset((localInputCWD / "images/tex.dds").string(), loadParams);
				auto imageContents = imageBundle.getContents();
				KRIS_ASSERT(!imageContents.empty());
				auto cpuimgview = nbl::core::smart_refctd_ptr_static_cast<nbl::asset::ICPUImageView>(*imageContents.begin());

				cpuimg = cpuimgview->getCreationParameters().image;

				nbl::video::IGPUImage::SCreationParams params;
				static_cast<nbl::asset::IImage::SCreationParams&>(params) = cpuimg->getCreationParameters();

				imageResource = m_ResourceAlctr.allocImage(m_device.get(), std::move(params), m_physicalDevice->getDeviceLocalMemoryTypeBits());
			}
			// allocate buffer (output for compute shader)
			{
				constexpr size_t BufferSize = sizeof(uint32_t)*WorkgroupSize*WorkgroupCount;

				// Always default the creation parameters, there's a lot of extra stuff for DirectX/CUDA interop and slotting into external engines you don't usually care about. 
				nbl::video::IGPUBuffer::SCreationParams params = {};
				params.size = BufferSize;
				// While the usages on `ICPUBuffers` are mere hints to our automated CPU-to-GPU conversion systems which need to be patched up anyway,
				// the usages on an `IGPUBuffer` are crucial to specify correctly.
				params.usage = IGPUBuffer::EUF_STORAGE_BUFFER_BIT;

				m_buffAllocation = m_ResourceAlctr.allocBuffer(m_device.get(), std::move(params), m_physicalDevice->getHostVisibleMemoryTypeBits());
				if (!m_buffAllocation)
					return logFail("Failed to create a GPU Buffer of size %d!\n", params.size);

				// Naming objects is cool because not only errors (such as Vulkan Validation Layers) will show their names, but RenderDoc captures too.
				m_buffAllocation->getBuffer()->setObjectDebugName("My Output Buffer");
			}

			m_Renderer.init(kris::refctd<nbl::video::ILogicalDevice>(m_device), m_sc.get(), nbl::asset::EF_D16_UNORM,
				gQueue->getFamilyIndex(), &m_ResourceAlctr, m_physicalDevice->getHostVisibleMemoryTypeBits());
			m_Scene.init(&m_Renderer);

			kris::MaterialBuilder mtlbuilder(m_system.get()); 
			
			// Upload all the data to GPU and create materials
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

				// image upload
				{
					utils.uploadImageData(imageResource.get(), cpuimg.get());
				}

				//m_api->startCapture();
				utils.endPassAndSubmit(getTransferUpQueue());
				//m_api->endCapture();
				utils.blockForSubmit();

				{
					auto mesh = nbl::core::make_smart_refctd_ptr<kris::Mesh>();
					mesh->m_mtl = mtlbuilder.buildGfxMaterial(&m_Renderer, m_logger.get(), localInputCWD / "materials/cube.mat");
					mesh->m_vtxBuf = std::move(vtxbuf);
					mesh->m_idxBuf = std::move(idxbuf);
					mesh->m_idxCount = m_cubedata.indexCount;
					mesh->m_idxtype = m_cubedata.indexType;
					mesh->m_vtxinput = m_cubedata.inputParams;
					mesh->m_resources[0] = { .rmapIx = 3, .res = imageResource };

					m_scenenode = m_Scene.createMeshSceneNode(m_device.get(), &m_ResourceAlctr, mesh.get());

					m_childnode = m_Scene.createMeshSceneNode(m_device.get(), &m_ResourceAlctr, mesh.get());
					m_childnode->getLocalTransform().setTranslation(nbl::core::vectorSIMDf(1.2f, 0.f, 0.f, 0.f));

					m_scenenode->addChild(kris::refctd(m_childnode));
				}

				{
					// set rmap
					m_Renderer.resourceMap[2] = m_buffAllocation.get();

					m_mtl = mtlbuilder.buildComputeMaterial(&m_Renderer, m_logger.get(), localInputCWD / "materials/hellocompute.mat");
				}
			}

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
					const uint64_t acqSignal = ++m_imgAcqCount;
					nbl::video::IQueue::SSubmitInfo::SSemaphoreInfo signalInfo = {
						.semaphore = m_imgacqSemaphore[acqSignal % (uint64_t)kris::FramesInFlight].get(),
						.value = acqSignal
					};

					//m_currentImageAcquire = m_surface->acquireNextImage();
					nbl::video::ISwapchain::SAcquireInfo acq;
					acq.queue = getGraphicsQueue();
					acq.signalSemaphores = { &signalInfo, 1 };
					m_sc->acquireNextImage(acq, &m_currImgAcq);

					oracle.reportEndFrameRecord();
					const auto timestamp = oracle.getNextPresentationTimeStamp();
					oracle.reportBeginFrameRecord();

					return timestamp;
				};

			const auto nextPresentationTimestamp = updatePresentationTimestamp();

			static double workloopTime = 0.0;
			const auto dt = oracle.getDeltaTimeInMicroSeconds() * 1e-6;
			workloopTime += dt;

			//if (!m_currentImageAcquire)
			//	return;

			camera.beginInputProcessing(nextPresentationTimestamp);
			mouse.consumeEvents([&](const nbl::ui::IMouseEventChannel::range_t& events) -> void { camera.mouseProcess(events); }, m_logger.get());
			keyboard.consumeEvents([&](const nbl::ui::IKeyboardEventChannel::range_t& events) -> void { camera.keyboardProcess(events); }, m_logger.get());
			camera.endInputProcessing(nextPresentationTimestamp);

			// Update transforms
			{
				{
					const float SpeedFactor = 1.f;
					const float Radius = 3.f;
					m_scenenode->getLocalTransform().setTranslation(
						nbl::core::vectorSIMDf(Radius * (float)std::sin(SpeedFactor*workloopTime), 0.f, Radius * (float)std::cos(SpeedFactor*workloopTime), 0.f)
					);
				}
				{
					const float SpeedFactor = 1.5f;
					const float Radius = 1.4f;
					m_childnode->getLocalTransform().setTranslation(
						nbl::core::vectorSIMDf(Radius * (float)std::sin(SpeedFactor*workloopTime), 0.f, Radius * (float)std::cos(SpeedFactor*workloopTime), 0.f)
					);
				}

				m_scenenode->updateTransformTree();
			}

			m_Renderer.beginFrame(&camera);

			// Record all the commands to command buffer using CommandRecorder
			{
				kris::CommandRecorder cmdrec = m_Renderer.createCommandRecorder(kris::Material::BasePass);

				cmdrec.setupMaterial(m_device.get(), m_mtl.get()); // first setup for dispatch (update desc set, memory barriers)
				cmdrec.dispatch(m_device.get(), kris::Material::BasePass, m_mtl.get(), WorkgroupCount, 1, 1); // do actual dispatch

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

				// setup draws (update desc sets, memory barriers)
				{
					cmdrec.setupDrawSceneNode(m_device.get(), m_scenenode.get());
				}

				// do draws within renderpass 
				{
					// begin renderpass
					{
						const VkRect2D currentRenderArea =
						{
							.offset = {0,0},
							.extent = {m_window->getWidth(),m_window->getHeight()}
						};

						const IGPUCommandBuffer::SClearColorValue clearValue = { .float32 = {1.f,0.f,0.f,1.f} };
						const IGPUCommandBuffer::SClearDepthStencilValue depthValue = { .depth = 0.f };

						cmdrec.beginRenderPass(
							currentRenderArea,
							clearValue,
							depthValue,
							m_Renderer.getFramebuffer(kris::Material::BasePass, m_currImgAcq));
					}

					cmdrec.drawSceneNode(m_device.get(), kris::Material::BasePass, m_scenenode.get());

					cmdrec.endRenderPass();
				}

				m_Renderer.consumeAsPass(kris::Material::BasePass, std::move(cmdrec));
			}

			//m_api->startCapture();
			auto rendered = m_Renderer.submit(getGraphicsQueue(),
				nbl::core::bitflag<nbl::asset::PIPELINE_STAGE_FLAGS>(nbl::asset::PIPELINE_STAGE_FLAGS::COMPUTE_SHADER_BIT) | nbl::asset::PIPELINE_STAGE_FLAGS::ALL_GRAPHICS_BITS);
			//m_api->endCapture();

#define CHECK_COMPUTE_RESULT 0

#if CHECK_COMPUTE_RESULT
			m_Renderer.blockForCurrentFrame(); // wait to read CS result on CPU
#endif 

			m_Renderer.endFrame();

#if CHECK_COMPUTE_RESULT
			if (!m_buffAllocation->map(IDeviceMemoryAllocation::EMCAF_READ))
			{
				logFail("Failed to map the Device Memory!\n");
				return;
			}
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
#endif

			//m_sc->present(m_currentImageAcquire.imageIndex, { &rendered, 1U });
			// present
			{
				const nbl::video::ISwapchain::SPresentInfo info = {
					.queue = getGraphicsQueue(),
					.imgIndex = m_currImgAcq,
					.waitSemaphores = {&rendered, 1}
				};

				m_sc->present(info);
			}
		}

		inline bool keepRunning() override
		{
			//if (m_surface->irrecoverable())
			//	return false;

			return true;
		}

		inline bool onAppTerminated() override
		{
			m_device->waitIdle();
			return device_base_t::onAppTerminated();
		}

	private:
		smart_refctd_ptr<nbl::ui::IWindow> m_window;
		kris::refctd<CSurfaceVulkanWin32> m_surface;
		kris::refctd<nbl::video::ISwapchain> m_sc;

		uint64_t m_imgAcqCount = 0ULL;
		kris::refctd<nbl::video::ISemaphore> m_imgacqSemaphore[kris::FramesInFlight];
		uint32_t m_currImgAcq = 0U;

		core::smart_refctd_ptr<InputSystem> m_inputSystem;
		InputSystem::ChannelReader<nbl::ui::IMouseEventChannel> mouse;
		InputSystem::ChannelReader<nbl::ui::IKeyboardEventChannel> keyboard;

		Camera camera = Camera(core::vectorSIMDf(0, 0, 0), core::vectorSIMDf(0, 0, 0), core::matrix4SIMD());
		video::CDumbPresentationOracle oracle;

		kris::refctd<nbl::asset::IAssetManager> m_assetMgr;

		GeometryCreator::return_type m_cubedata;

		kris::ResourceAllocator m_ResourceAlctr;
		kris::Renderer m_Renderer;

		kris::Scene m_Scene;
		kris::refctd<kris::SceneNode> m_scenenode;
		kris::refctd<kris::SceneNode> m_childnode;

		kris::refctd<kris::BufferResource> m_buffAllocation;
		kris::refctd<kris::ComputeMaterial> m_mtl;
};


NBL_MAIN_FUNC(KrisTestApp)