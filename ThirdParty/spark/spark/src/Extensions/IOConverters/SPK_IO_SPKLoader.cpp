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

#include <SPARK_Core.h>
#include "Extensions/IOConverters/SPK_IO_SPKLoader.h"

#include <sstream>

namespace SPK
{
namespace IO
{	
	const char SPKLoader::MAGIC_NUMBER[3] = { 0x53, 0x50, 0x4B }; // "SPK" in ASCII
	const char SPKLoader::VERSION = 0;

	const size_t SPKLoader::DATA_LENGTH_OFFSET = 4;
	const size_t SPKLoader::HEADER_LENGTH = 12;
	const size_t SPKLoader::MAX_DATA_LENGTH = 64 * 1024 * 1024; // (FOnline Patch)
	const size_t SPKLoader::MAX_OBJECTS = 64 * 1024; // (FOnline Patch) Far above practical particle graphs without permitting million-object allocation attacks.
	const size_t SPKLoader::MAX_ATTRIBUTE_VALUES = 64 * 1024; // (FOnline Patch)

    bool SPKLoader::innerLoadFromBuffer(Graph& graph, const char * data, unsigned int datasize)
    {
        // (FOnline Patch) Runtime resources are memory-backed, so use the same validated binary path as file streams.
		if (data == NULL || datasize < HEADER_LENGTH)
		{
			SPK_LOG_ERROR("SPKLoader::innerLoadFromBuffer(Graph&,const char*,unsigned int) - Truncated SPK header");
			return false;
		}

		// (FOnline Patch) Memory-backed resources must contain exactly the payload declared by their header.
		const unsigned int declaredDataSize =
			static_cast<unsigned int>(static_cast<unsigned char>(data[4])) |
			(static_cast<unsigned int>(static_cast<unsigned char>(data[5])) << 8) |
			(static_cast<unsigned int>(static_cast<unsigned char>(data[6])) << 16) |
			(static_cast<unsigned int>(static_cast<unsigned char>(data[7])) << 24);
		if (static_cast<size_t>(declaredDataSize) > MAX_DATA_LENGTH || static_cast<size_t>(declaredDataSize) != static_cast<size_t>(datasize) - HEADER_LENGTH)
		{
			SPK_LOG_ERROR("SPKLoader::innerLoadFromBuffer(Graph&,const char*,unsigned int) - Declared payload size does not match the buffer");
			return false;
		}

        std::string buffer(data, datasize);
        std::istringstream is(buffer, std::ios::in | std::ios::binary);
        return innerLoad(is, graph, std::string());
    }

    bool SPKLoader::innerLoad(std::istream& is, Graph& graph, const std::string &path) const
	{
		// Check header
		const IOBuffer header(HEADER_LENGTH,is);
		if (header.hasError()) // (FOnline Patch) A short read must not expose uninitialized header bytes.
		{
			SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Truncated SPK header");
			return false;
		}

		for (size_t i = 0; i < 3; ++i)
			if (header.get<char>() != MAGIC_NUMBER[i])
			{
				SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Invalid SPK format");
				return false;
			}

		if (header.get<char>() != VERSION)
		{
			SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Wrong version of SPK format");
			return false;
		}

		size_t dataLength = header.get<uint32>();
		SPK_LOG_DEBUG("SPKLoader::innerLoad(std::istream&,Graph&) - Data length : " << dataLength << " bytes");

		size_t nbObjects = header.get<uint32>();
		SPK_LOG_DEBUG("SPKLoader::innerLoad(std::istream&,Graph&) - Nb objects : " << nbObjects);

		// (FOnline Patch) Bound all allocations and require enough bytes for each minimal object record.
		if (header.hasError() || dataLength > MAX_DATA_LENGTH || nbObjects > MAX_OBJECTS || nbObjects > dataLength / 9)
		{
			SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Invalid SPK data or object count");
			return false;
		}
		
		// Loads data
		const IOBuffer data(dataLength,is);
		if (data.hasError()) // (FOnline Patch) failbit/eofbit short reads are errors too.
		{
			SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Error while reading the stream");
			return false;
		}

		std::vector<size_t> objectDataOffset(nbObjects); // (FOnline Patch) RAII on every validation exit.
		
		// First pass to get the objects types
		for (size_t i = 0; i < nbObjects; ++i)
		{
			if (data.isAtEnd())
			{
				SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Corrupted data");
				return false;
			}

			std::string type = data.get<std::string>();
			if (data.hasError() || data.getRemaining() < sizeof(uint32))
			{
				SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Truncated object record");
				return false;
			}

			objectDataOffset[i] = data.getPosition();
			const size_t objectDataSize = data.get<uint32>();
			if (data.hasError() || objectDataSize < sizeof(uint32) || objectDataSize > data.getRemaining())
			{
				SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - Invalid object data size");
				return false;
			}
			data.skip(objectDataSize);
			if (!graph.addNode(i,type)) // (FOnline Patch) Reject graphs containing unknown or duplicate objects.
			{
				return false;
			}
		}

		if (data.hasError() || data.getPosition() != data.getSize()) // (FOnline Patch) Reject trailing or skipped binary payload.
		{
			SPK_LOG_ERROR("SPKLoader::innerLoad(std::istream&,Graph&) - SPK object records do not consume the declared payload");
			return false;
		}

		if (!graph.validateNodes())
			return false;

		// Second pass to fill the descriptors
		for (size_t i = 0; i < nbObjects; ++i)
		{
			Node* node = graph.getNode(i);
			if (node != NULL)
			{
				data.setPosition(objectDataOffset[i]);
				if (!readObject(*node,graph,data))
				{
					return false;
				}
			}
		}

		return !data.hasError();
	}

	bool SPKLoader::readObject(Node& node,const Graph& graph,const IOBuffer& data) const
	{
		Descriptor& desc = node.getDescriptor();		
		if (data.getRemaining() < sizeof(uint32))
			return false;

		size_t attributeDataSize = data.get<uint32>();
		if (data.hasError() || attributeDataSize < sizeof(uint32) || attributeDataSize > data.getRemaining())
		{
			SPK_LOG_ERROR("SPKLoader::readObject(Node&,const Graph&,const IOBuffer&) - Invalid attribute data size");
			return false;
		}

		size_t endOffset = data.getPosition() + attributeDataSize;
		if (desc.getSignature() != data.get<uint32>())
		{
			// (FOnline Patch) Descriptor drift cannot safely fall back to default attributes.
			SPK_LOG_ERROR("SPKLoader::readObject(Node&,const Graph&,const IOBuffer&) - Wrong signature for node "+node.getObject()->getClassName());
			return false;
		}
		else
		{
			for (size_t i = 0; i < desc.getNbAttributes(); ++i)
			{
				Attribute& attrib = desc.getAttribute(i);
				if (!readAttribute(attrib,graph,data))
					return false;
			}

			if (data.getPosition() != endOffset)
			{
				SPK_LOG_ERROR("SPKLoader::readObject(Node&,const Graph&,const IOBuffer&) - Corrupted data");
				return false;
			}
		}

		return !data.hasError();
	}

	bool SPKLoader::readAttribute(Attribute& attrib,const Graph& graph,const IOBuffer& data) const
	{
		if (data.isAtEnd())
		{
			SPK_LOG_ERROR("SPKLoader::readAttribute(Attribute&,const Graph&,const IOBuffer&) - Corrupted data");
			return false;
		}

		if (data.get<bool>())
		{
			switch (attrib.getType())
			{
			case ATTRIBUTE_TYPE_CHAR :		attrib.setValue(data.get<char>()); break;
			case ATTRIBUTE_TYPE_BOOL :		attrib.setValue(data.get<bool>()); break;
			case ATTRIBUTE_TYPE_INT32 :		attrib.setValue(data.get<int32>()); break;
			case ATTRIBUTE_TYPE_UINT32 :	attrib.setValue(data.get<uint32>()); break;
			case ATTRIBUTE_TYPE_FLOAT :		attrib.setValue(data.get<float>()); break;
			case ATTRIBUTE_TYPE_VECTOR :	attrib.setValue(data.get<Vector3D>()); break;
			case ATTRIBUTE_TYPE_COLOR :		attrib.setValue(data.get<Color>()); break;
			case ATTRIBUTE_TYPE_STRING :	attrib.setValue(data.get<std::string>()); break;
			
			case ATTRIBUTE_TYPE_CHARS :		if (!setAttributeValues<char>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_BOOLS :		if (!setAttributeValues<bool>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_INT32S :	if (!setAttributeValues<int32>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_UINT32S :	if (!setAttributeValues<uint32>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_FLOATS :	if (!setAttributeValues<float>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_VECTORS :	if (!setAttributeValues<Vector3D>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_COLORS :	if (!setAttributeValues<Color>(attrib,data)) return false; break;
			case ATTRIBUTE_TYPE_STRINGS :	if (!setAttributeValues<std::string>(attrib,data)) return false; break;

			case ATTRIBUTE_TYPE_REF : {	
				Ref<SPKObject> object = readReference(graph,data);
				if (!object)
					return false;
				attrib.setValue(object);
				break; }

			case ATTRIBUTE_TYPE_REFS : {
				if (data.getRemaining() < sizeof(uint32))
					return false;
				size_t nbValues = data.get<uint32>();
				if (data.hasError() || nbValues > MAX_ATTRIBUTE_VALUES || nbValues > data.getRemaining() / sizeof(uint32))
					return false;
				std::vector<Ref<SPKObject> > objects(nbValues);
				for (size_t i = 0; i < nbValues; ++i)
				{
					objects[i] = readReference(graph,data);
					if (!objects[i])
						return false;
				}
				attrib.setValues(objects.data(),nbValues);
				break; }

			default : {
				SPK_LOG_FATAL("SPKLoader::readAttribute(Attribute&,const Graph&,const IOBuffer&) - Unknown attribute type");
				return false; }
			}
		}

		return !data.hasError();
	}

	Ref<SPKObject> SPKLoader::readReference(const Graph& graph,const IOBuffer& data) const
	{
		// (FOnline Patch) Reference IDs are one-based. Reject zero, truncation and
		// unknown IDs instead of turning them into nullable descriptor values.
		if (data.getRemaining() < sizeof(uint32))
			return SPK_NULL_REF;

		uint32 referenceID = data.get<uint32>();
		if (data.hasError() || referenceID == 0)
		{
			SPK_LOG_ERROR("SPKLoader::readReference(const Graph&,const IOBuffer&) - Invalid zero or truncated reference");
			return SPK_NULL_REF;
		}

		Node* node = graph.getNode(static_cast<size_t>(referenceID - 1));
		if (node == NULL)
		{
			SPK_LOG_ERROR("SPKLoader::readReference(const Graph&,const IOBuffer&) - Reference is outside the object graph");
			return SPK_NULL_REF;
		}

		return node->getObject();
	}
}}
