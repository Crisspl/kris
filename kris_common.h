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

#define KRIS_UNUSED_PARAM(param)	(void*) param

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

		ResourceMapSlots = 256U,
		DefaultBufferResourceMapSlot = 0U,
		DefaultImageResourceMapSlot = 1U,
		FirstUsableResourceMapSlot = DefaultImageResourceMapSlot + 1,

		Size_FullRange = 0U,
		MipCount_FullRange = 0U,
		LayerCount_FullRange = 0U,

		CameraDescSetIndex = 1U,
		MaterialDescSetIndex = 3U
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
}