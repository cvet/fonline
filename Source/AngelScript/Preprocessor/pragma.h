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

#ifndef JM_PREPROCESSOR_PRAGMA_H
#define JM_PREPROCESSOR_PRAGMA_H

#include <string>

namespace Preprocessor
{
	class PragmaInstance
	{
	public:
		std::string text;
		std::string current_file;
		unsigned int current_file_line;
		std::string root_file;
		unsigned int global_line;
	};

	class PragmaCallback
	{
	public:
		virtual void pragma(const PragmaInstance&) = 0;
	};
};

#endif