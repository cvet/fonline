//
// SPARK particle engine
//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com
// Copyright (C) 2017 - Frederic Martin - fredakilla@gmail.com
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef H_SPK_DEF
#define H_SPK_DEF

#include <climits>
#include <cstdlib>

// for windows platform only
#if defined(WIN32) || defined(_WIN32)

#ifdef SPK_CORE_EXPORT
#define SPK_PREFIX __declspec(dllexport)
#elif defined(SPK_IMPORT) || defined(SPK_CORE_IMPORT)
#define SPK_PREFIX __declspec(dllimport)
#else
#define SPK_PREFIX
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251) // disables the warning about exporting STL classes in DLLs
#pragma warning(disable : 4996) // disables the deprecation of some functions
#pragma warning(disable : 4275) // disables the warning about exporting DLL classes children of non DLL classes
#endif

// other platforms than windows
#else
#define SPK_PREFIX
#endif

#include "SPK_MemoryTracer.h"
#include "SPK_Reference.h"
#include "SPK_Enum.h"

/** @brief Returns a random value within [min,max[ from an explicit SPARK context. */
#define SPK_RANDOM(context,min,max) (context).generateRandom(min,max)

/**
* @namespace SPK::IO
* @brief the namespace for input/output relative SPARK code
*/

/**
* @namespace SPK
* @brief the namespace for core SPARK code
*/
namespace SPK
{
// The code below regarding fixed size integer is an adaptation from SFML code
// 32 bits integer types
#if USHRT_MAX == 0xFFFFFFFF
	typedef signed   short int32;
	typedef unsigned short uint32;
#elif UINT_MAX == 0xFFFFFFFF
	typedef signed   int int32;
	typedef unsigned int uint32;
#elif ULONG_MAX == 0xFFFFFFFF
	typedef signed   long int32;
	typedef unsigned long uint32;
#else
#error No 32 bits integer type for this platform
#endif

	class Zone;
	namespace IO { class IOManager; }

#ifdef SPK_DOXYGEN_ONLY // for documentation purpose only
	/** @brief Constants defining parameters of a particle */
	enum Param
	{
		PARAM_SCALE = 0,				/**< @brief The scale of a particle */
		PARAM_MASS = 1,					/**< @brief The mass of a particle */
		PARAM_ANGLE = 2,				/**< @brief The rotation angle of a particle */
		PARAM_TEXTURE_INDEX = 3,		/**< @brief The index of texture coordinates of a particle */
		PARAM_ROTATION_SPEED = 4,		/**< @brief The rotation speed of a particle */
	};

	/** @brief Constants defining the way a factor is applied*/
	enum Factor
	{
		FACTOR_CONSTANT = 0,	/**< @brief Defines a constant factor (C) */
		FACTOR_LINEAR = 1,		/**< @brief Defines a linear factor (x) */
		FACTOR_QUADRATIC = 2,	/**< @brief Defines a quadratic factor (x^2) */
		FACTOR_CUBIC = 3,		/**< @brief Defines a cubic factor (x^3) */		
	};

	/**
	* @enum InterpolationType
	* @brief Constants defining which type of value is used for interpolation
	*/
	enum InterpolationType
	{
		INTERPOLATOR_LIFETIME,	/**< Constant defining the life time as the value used to interpolate */
		INTERPOLATOR_AGE,		/**< Constant defining the age as the value used to interpolate */
		INTERPOLATOR_PARAM,		/**< Constant defining a parameter as the value used to interpolate */
		INTERPOLATOR_VELOCITY,	/**< Constant defining the square norm of the velocity as the value used to interpolate */
	};
#endif

	#define SPK_ENUM_PARAM(XX) \
		XX(PARAM_SCALE,=0) \
		XX(PARAM_MASS,=1) \
		XX(PARAM_ANGLE,=2) \
		XX(PARAM_TEXTURE_INDEX,=3) \
		XX(PARAM_ROTATION_SPEED,=4) \

	SPK_DECLARE_ENUM(Param,SPK_ENUM_PARAM)

	#define SPK_ENUM_FACTOR(XX) \
		XX(FACTOR_CONSTANT,=0) \
		XX(FACTOR_LINEAR,=1) \
		XX(FACTOR_QUADRATIC,=2) \
		XX(FACTOR_CUBIC,=3) \

	SPK_DECLARE_ENUM(Factor,SPK_ENUM_FACTOR)

	#define SPK_ENUM_INTERPOLATION_TYPE(XX) \
		XX(INTERPOLATOR_LIFETIME,) \
		XX(INTERPOLATOR_AGE,) \
		XX(INTERPOLATOR_PARAM,) \
		XX(INTERPOLATOR_VELOCITY,) \

	SPK_DECLARE_ENUM(InterpolationType,SPK_ENUM_INTERPOLATION_TYPE)

	/** Runtime state owned by one SPARK engine integration instance. */
	class SPK_PREFIX SPKContext
	{
	public :
		SPKContext();
		~SPKContext();

		/** @brief Releases all dynamic data held by the context */
		void release();

		/** @brief Gets the IO registry owned by this context. */
		IO::IOManager& getIOManager();
		const IO::IOManager& getIOManager() const;

		/** @brief Exposes generator state so particle systems can own independent random streams. */
		unsigned int getRandomSeed() const;
		void setRandomSeed(unsigned int seed);

		/**
		* @brief Gets the default zone
		* The default zone is a shared point located at position (0.0f,0.0f,0.0f).
		* @return the default zone
		*/
		const Ref<Zone>& getDefaultZone();

		/**
		* @brief Gets a random value within the interval [min,max[
		* @param min : the minimum bound of the interval (inclusive)
		* @param max : the maximum bound of the interval (exclusive)
		* @return a random number within [min,max[
		*/
		template<typename T>
		T generateRandom(const T& min,const T& max);

	private :
		IO::IOManager* ioManager;
		Ref<Zone> defaultZone;
		unsigned int randomSeed;

		SPKContext(const SPKContext&); // Not used
		SPKContext& operator=(const SPKContext&); // Not used
	};

	/** Preserves a context stream while advancing one particle system's random state. */
	class SPK_PREFIX RandomSeedScope
	{
	public :
		RandomSeedScope(SPKContext& context,unsigned int& randomSeed);
		~RandomSeedScope();

	private :
		SPKContext& context;
		unsigned int& randomSeed;
		unsigned int previousRandomSeed;

		RandomSeedScope(const RandomSeedScope&); // Not used
		RandomSeedScope& operator=(const RandomSeedScope&); // Not used
	};

	inline IO::IOManager& SPKContext::getIOManager()
	{
		return *ioManager;
	}

	inline const IO::IOManager& SPKContext::getIOManager() const
	{
		return *ioManager;
	}

	inline unsigned int SPKContext::getRandomSeed() const
	{
		return randomSeed;
	}

	inline void SPKContext::setRandomSeed(unsigned int seed)
	{
		randomSeed = seed % 0x7FFFFFFFU;
		if (randomSeed == 0)
			randomSeed = 1;
	}

	template<typename T>
	inline T SPKContext::generateRandom(const T& min,const T& max)
    {
		// optimized standard minimal
		long tmp0 = 16807L * (randomSeed & 0xFFFFL);
        long tmp1 = 16807L * (randomSeed >> 16);
        long tmp2 = (tmp0 >> 16) + tmp1;
        tmp0 = ((tmp0 & 0xFFFF)|((tmp2 & 0x7FFF) << 16)) + (tmp2 >> 15);

		// correction of the error
        if ((tmp0 & 0x80000000L) != 0)
			tmp0 = (tmp0 + 1) & 0x7FFFFFFFL;

		randomSeed = tmp0;

		// find a random number in the interval
        return static_cast<T>(min + ((randomSeed - 1) / 2147483646.0) * (max - min));
    }
}

#endif
