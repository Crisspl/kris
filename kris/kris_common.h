#pragma once

#include <nabla.h>

#if defined(_NBL_DEBUG)
#define KRIS_CFG_DEBUG 1
#else
#define KRIS_CFG_DEBUG 0
#endif
#if defined(_NBL_RELWITHDEBINFO)
#define KRIS_CFG_DEV 1
#else
#define KRIS_CFG_DEV 0
#endif
#if KRIS_CFG_DEBUG && KRIS_CFG_DEV
#error "Something went wrong. Debug and Development configs cannot co-exist!"
#endif
#if !KRIS_CFG_DEBUG && !KRIS_CFG_DEV
#define KRIS_CFG_SHIPPING 1
#else
#define KRIS_CFG_SHIPPING 0
#endif

#define KRIS_MEM_ALLOC(sz)			malloc(sz)
#define KRIS_MEM_FREE(mem)			free(mem)
#define KRIS_MEM_NEW				new
#define	KRIS_MEM_DELETE(ptr)		delete ptr;
#define	KRIS_MEM_SAFE_DELETE(ptr)	if (ptr) { KRIS_MEM_DELETE(ptr); }

#define KRIS_UNUSED_PARAM(param)	((void) param)

#if !KRIS_CFG_SHIPPING
#define KRIS_ASSERT(cond) \
if ((!(cond)) && IsDebuggerPresent())\
{\
	__debugbreak();\
}
//g_Logger.log(fmt, nbl::system::ILogger::ELL_ERROR __VA_OPT__(,) __VA_ARGS__);
#define KRIS_ASSERT_MSG(cond, fmt, ...) \
if ((!(cond)) && IsDebuggerPresent())\
{\
	__debugbreak();\
}
#else
#define KRIS_ASSERT(cond) ((void)(cond))
#define KRIS_ASSERT_MSG(cond, ...) ((void)(cond))
#endif

namespace kris
{
	enum : uint32_t
	{
		FramesInFlight = 3U,

		MaxColorBuffers = 8U,

		ResourceMapSlots = 256U,
		DefaultBufferResourceMapSlot = 0U,
		DefaultImageResourceMapSlot = 1U,
		FirstUsableResourceMapSlot = DefaultImageResourceMapSlot + 1,

		Size_FullRange = 0U,
		MipCount_FullRange = 0U,
		LayerCount_FullRange = 0U,

		CameraDescSetIndex = 1U,
		SceneNodeDescSetIndex = 2U,
		MaterialDescSetIndex = 3U
	};

	enum EPass : uint32_t
	{
		BasePass = 0U,

		NumPasses
	};

#define KRIS_DEBUG_LOGGER_WRITE_TO_STDOUT 1
	class CDebugLogger : public nbl::system::IThreadsafeLogger
	{
	public:
		inline CDebugLogger(nbl::core::bitflag<E_LOG_LEVEL> logLevelMask = ILogger::DefaultLogMask()) : IThreadsafeLogger(logLevelMask) {}

	private:
		// more info about how this works: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
		virtual void threadsafeLog_impl(const std::string_view& fmt, E_LOG_LEVEL logLevel, va_list args) override
		{
			auto str = constructLogString(fmt, logLevel, args);
#if KRIS_DEBUG_LOGGER_WRITE_TO_STDOUT
			switch (logLevel)
			{
			case ELL_DEBUG:
				printf("\x1b[37m%s\x1b[0m", str.data()); // White
				break;
			case ELL_INFO:
				printf("\x1b[37m%s\x1b[0m", str.data()); // White
				break;
			case ELL_WARNING:
				printf("\x1b[33m%s\x1b[0m", str.data()); // yellow
				break;
			case ELL_ERROR:
				printf("\x1b[31m%s\x1b[0m", str.data()); // red
				break;
			case ELL_PERFORMANCE:
				printf("\x1b[34m%s\x1b[0m", str.data()); // blue
				break;
			case ELL_NONE:
				assert(false);
				break;
			}
			fflush(stdout);
#endif

			OutputDebugString(str.c_str());
		}
	};

	extern CDebugLogger g_Logger;

	template <typename T>
	using refctd = nbl::core::smart_refctd_ptr<T>;

	//using BufferBarrier = nbl::video::IGPUCommandBuffer::SBufferMemoryBarrier<nbl::video::IGPUCommandBuffer::SOwnershipTransferBarrier>;
	//using ImageBarrier = nbl::video::IGPUCommandBuffer::SImageMemoryBarrier<nbl::video::IGPUCommandBuffer::SOwnershipTransferBarrier>;

	using half = uint16_t;

	// credit: https://stackoverflow.com/questions/3026441/float32-to-float16
	inline half f32tof16(float _f)
	{
		const uint32_t& fltInt32 = nbl::core::floatBitsToUint(_f);
		half fltInt16 = 0;

		fltInt16 = (fltInt32 >> 31) << 5;
		half tmp = (fltInt32 >> 23) & 0xff;
		tmp = (tmp - 0x70) & ((unsigned int)((int)(0x70 - tmp) >> 4) >> 27);
		fltInt16 = (fltInt16 | tmp) << 10;
		fltInt16 |= (fltInt32 >> 13) & 0x3ff;

		return fltInt16;
	}
	// credit: https://gist.github.com/zhuker/b4bd1fb306c7b04975b712c37c4c4075
	inline float f16tof32(const half in) 
	{
		uint32_t t1;
		uint32_t t2;
		uint32_t t3;

		t1 = in & 0x7fffu;                       // Non-sign bits
		t2 = in & 0x8000u;                       // Sign bit
		t3 = in & 0x7c00u;                       // Exponent

		t1 <<= 13u;                              // Align mantissa on MSB
		t2 <<= 16u;                              // Shift sign bit into position

		t1 += 0x38000000;                       // Adjust bias

		t1 = (t3 == 0 ? 0 : t1);                // Denormals-as-zero

		t1 |= t2;                               // Re-insert sign bit

		float out = 0.f;
		*((uint32_t*)&out) = t1;

		return out;
	}
}