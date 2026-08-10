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

#include <sstream>
#include <SPARK_Core.h>

namespace SPK
{
namespace IO
{
	const bool IOBuffer::USE_LITTLE_ENDIANS = IOBuffer::isLittleEndians();

	IOBuffer::IOBuffer(size_t capacity) :
		capacity(capacity),
		size(0),
		position(0),
		readOnly(false),
		error(false),
		fallback {}
	{
		buf = SPK_NEW_ARRAY(char,capacity);
	}

	IOBuffer::IOBuffer(size_t capacity,std::istream& is) :
		capacity(capacity),
		size(0),
		position(0),
		readOnly(true),
		error(false),
		fallback {}
	{
		buf = SPK_NEW_ARRAY(char,capacity);
		is.read(buf,capacity);
		size = static_cast<size_t>(is.gcount()); // Never expose uninitialized bytes after a short read.
		error = size != capacity;
	}

	IOBuffer::~IOBuffer()
	{
		SPK_DELETE_ARRAY(buf);
	}

	const char* IOBuffer::get(size_t length) const
	{
		// Reject integer overflow and reads past initialized data.
		if (length > getRemaining())
		{
			error = true;
			position = size;
			return fallback;
		}

		const char* result = buf + position;
		position += length;
		return result;
	}

	void IOBuffer::skip(size_t nb) const
	{
		// Saver buffers reserve future bytes, while loader buffers are strictly bounded.
		if (nb > static_cast<size_t>(-1) - position || (readOnly && nb > getRemaining()))
		{
			error = true;
			position = readOnly ? size : position;
			return;
		}

		position += nb;
	}

	void IOBuffer::setPosition(size_t pos) const
	{
		// Both saver rewrites and loader seeks must target already initialized bytes.
		if (pos > size)
		{
			error = true;
			position = size;
			return;
		}

		position = pos;
	}

	template<> std::string IOBuffer::get<std::string>() const	
	{ 
		std::string str;
		while (position < size)
		{
			const char c = get<char>();
			if (c == '\0')
				return str;
			str += c;
		}

		error = true; // Serialized strings must be null terminated inside the object data.
		return str;
	}

	template<> Vector3D IOBuffer::get<Vector3D>() const	
	{ 
		float x = get<float>();
		float y = get<float>();
		float z = get<float>();
		return Vector3D(x,y,z); 
	}

	void IOBuffer::put(char c) 
	{ 
		updateSize(position + 1); 
		buf[position++] = c; 
	}

	void IOBuffer::put(const char* c,size_t length) 
	{ 
		updateSize(position + length);
		std::memcpy(buf + position,c,length);
		position += length;
	}

	void IOBuffer::updateSize(size_t newPosition)
	{
		size_t newCapacity = capacity;
		while (newPosition >= newCapacity)
			newCapacity <<= 1;	
		if (newCapacity != capacity)
		{
			char* newBuf = SPK_NEW_ARRAY(char,newCapacity);
			std::memcpy(newBuf,buf,size);
			SPK_DELETE_ARRAY(buf);
			buf = newBuf;
		}
		if (newPosition > size)
			size = newPosition;
	}

	bool IOBuffer::isLittleEndians()
	{
		uint32 test = 0x01;
		return (reinterpret_cast<char*>(&test)[0]) == 0x01;
	}
}}
