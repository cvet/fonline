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

#ifndef JM_PREPROCESSOR_PREPROCESS_H
#define JM_PREPROCESSOR_PREPROCESS_H

#include <stdio.h>
#include <string>
#include <string.h>
#include "stream.h"
#include "line_number_translator.h"
#include "pragma.h"

#define PREPROCESSOR_VERSION_STRING "0.5"

namespace Preprocessor
{
	class FileSource
	{
	public:
		FileSource():Stream(NULL){}

		bool LoadFile(const std::string& filename, std::vector<char>& data)
		{
			if(Stream)
			{
				size_t len=strlen(Stream);
				data.resize(len);
				memcpy(&data[0],Stream,len);
				Stream=NULL;
			}
			else
			{
				std::string path=CurrentDir+filename;

				FILE* fs=NULL;
				if(fopen_s(&fs,path.c_str(),"rb")) return false;

				fseek(fs,0,SEEK_END);
				int len=ftell(fs);
				fseek(fs,0,SEEK_SET);

				data.resize(len);

				if(!fread(&data[0],len,1,fs))
				{
					fclose(fs);
					return false;
				}
				fclose(fs);
			}

			return true;
		}

		std::string CurrentDir;
		const char* Stream;
	};

	int Preprocess(
		std::string filename,
		FileSource& file_source,
		OutStream& destination,
		bool process_pragmas = true,
		OutStream* err = NULL,
		LineNumberTranslator* = 0
		);

	void Define(const std::string& str);
	void Undefine(const std::string& str);
	bool IsDefined(const std::string& str);
	void UndefAll();
	void SetPragmaCallback(PragmaCallback* callback);
	void CallPragma(const std::string& name, const PragmaInstance& instance);
	std::vector<std::string>& GetFileDependencies();
	std::vector<std::string>& GetParsedPragmas();
};

#endif
