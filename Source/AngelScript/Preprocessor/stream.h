/*
   Preprocessor 0.5
   Copyright (c) 2005 Anthony Casteel

   This software is provided 'as-is', without any express or implied 
   warranty. In no event will the authors be held liable for any 
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any 
   purpose, including commercial applications, and to alter it and 
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you 
      must not claim that you wrote the original software. If you use
	  this software in a product, an acknowledgment in the product 
	  documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and 
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source 
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/
   under addons & utilities or at
   http://www.omnisu.com

   Anthony Casteel
   jm@omnisu.com
*/

#ifndef JM_PREPROCESSOR_STREAM_H
#define JM_PREPROCESSOR_STREAM_H

#include <vector>
#include <string>
#include <sstream>

namespace Preprocessor
{
	class OutStream
	{
	protected:
		virtual void Write(const char*,size_t size) = 0;

	public:
		virtual ~OutStream() {}

		template<typename T>
		OutStream& operator<<(const T& in)
		{
			std::stringstream strstr;
			strstr << in;
			std::string str;
			strstr >> str;
			Write(str.c_str(),str.length());
			return *this;
		}

		OutStream& operator<<(const std::string& in)
		{
			Write(in.c_str(),in.length());
			return *this;
		}		

		OutStream& operator<<(const char* in)
		{
			return operator<<(std::string(in));
		}
	};

	class VectorOutStream: public OutStream
	{
	private:
		std::vector<char> streamData;

	protected:
		virtual void Write(const char* d,unsigned int size)
		{
			streamData.insert(streamData.end(),d,d+size);
		}

	public:
		const char* GetData() { return &streamData[0]; }
		size_t GetSize() { return streamData.size(); }

		void Format()
		{
			if(streamData.size()<10) return;

			for(int i=0;i<(int)streamData.size()-2;++i)
				if(streamData[i]=='\n' && streamData[i+1]=='\n' && streamData[i+2]=='\n')
					streamData[i]=' ';

			for(int i=0;i<(int)streamData.size()-1;++i)
				if(streamData[i]=='{') i=InsertTabs(i+1,1);
		}

		int InsertTabs(int cur_pos, int level)
		{
			if(cur_pos<0 || cur_pos>=(int)streamData.size()) return (int)streamData.size();

			int i=cur_pos;
			for(;i<(int)streamData.size()-1;i++)
			{
				if(streamData[i]=='\n') 
				{
					int k=0;
					if(streamData[i+1]=='}') k++;
					for(;k<level;++k)
					{
						i++;
						streamData.insert(streamData.begin()+i,std::vector<char>::value_type('\t'));
					}
				}
				else if(streamData[i]=='{') i=InsertTabs(i+1,level+1);
				else if(streamData[i]=='}') return i;
			}

			return i;
		}

		void PushNull(){streamData.push_back('\0');}
	};

	class NullOutStream: public OutStream
	{
	protected:
		virtual void Write(const char*,unsigned int) {}
	};
};

#endif