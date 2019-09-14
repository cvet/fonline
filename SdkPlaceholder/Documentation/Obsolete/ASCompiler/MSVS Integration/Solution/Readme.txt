This document provides an overwiew for the FOnline MSVC solution tools.

Table of Contents
  1. General information
  2. Disclaimer
  3. Supported versions
  4. Tools
  5. Instructions
  6. Known problems

1. General information
  The provided MSVC solution tools aim at a better integration of FOnline script base in an existing MSVC IDE, and utilization of its many features, such as:
  - Autocompletion
  - Code definition lookup
  - Symbol references search
  - Jumping to definitions and declarations
  - Class browser
  - Module functions listing
  - Other
This is made possible because of close similarities of AS syntax to that of C++. The MSVC parser can be, with certain tweaks, tricked into thinking of AS scripts as C++ sources. The purpose of the tools is to automate the whole process. The results are much worth the effort.

2. Disclaimer
  Under no circumstances shall the author be liable to any user for direct, indirect, incidental, consequential, special, or exemplary damages, arising from user's use or misuse of the software, or errors in the software.

3. Supported versions
  The solution tools work with MSVC 2008 (any version) as well as MSVC 2005 (any version). It does NOT work on MSVC 2010 or 2011, at least not to the same extent. Also see the Known problems section.

4. Tools
  The solution tools consist of three separate executables.

4.1. ProjectCreator.exe
  The executable opens ProjectCreator.cfg config file and prepares the solution, taking file names from the scripts.cfg and assigning them to Server, Client and Mapper projects under appropriate filters. The projects themselves must exist in the directory pointed in the config file. They must contain at least three filters, also referenced in the config file. For convenience, blank projects are provided with this release of tools, along with a solution file that references all three of them.
  The executable adds files using the following rules:
- If the file is an enabled (server, client, mapper) module, it is added to the appropriate project into the "Modules" filter.
- If the file is a disabled (server, client, mapper) module, it is added to the appropriate project into the "Disabled" filter.
- If the file is inlined in any (server, client, mapper) module, it is added to the appropriate project into the "Headers" filter.
The search for inlined headers use the appropriate compilation define (__SERVER, __CLIENT, __MAPPER) and the preprocessor directives encountered along the way.

4.2. IntellisenseCreator.dll
  Usage: Must be loaded by an AngelScript compiler embedded in one of the FOnline's applications
  This tool extracts information on server, client and mapper APIs from the compiler it was invoked with, for the Intellisense purpose. It first registers all AngelScript functions and classes provided by the compiler, and then scan the compiler's executable to search the argument names (if present). The results are saved as a header file.
  The compiler's executable must be named either ASCompiler.exe, FOnlineServer.exe, FOnline.exe or Mapper.exe.

4.3. ScriptsRefactorer.exe
  Usage: ScriptsRefactorer.exe input_file output_file
  Scripts refactorer processes the AngelScript source file in order to make it more compliant with C++ syntax, while still preserving it as a valid AngelScript source file. Specifically, it does the following:
a) Adds semicolons at end of each class and interface. This is optional is AS but not in C++.
b) Refactors old AS array syntax into more modern one: T[] arr -> array<T> arr. This allows C++ parser to actually see this as an array template instance. Exception: arrays recognized as static arrays are left with the old syntax. See requirements for AngelScript 3.0.0 for more explanation.
c) Replaces all occurrances of "not" keyword with "!".

5. Instructions
  The following instructions are valid for the release contained within the FOnline SDK repository. In other case, paths in ProjectCreator.cfg and in referenced .bat files need to be changed manually. Refer to the tools description for more informations.

First time use:
1. Associate the .fos extension with the MSVC editor (devenv.exe or vcexpress.exe).
2. Associate the .fos extension with C++ language:
a) Open Tools->Options->VC++ Project Settings,
b) Add the AS source extension to C/C++ Files Extensions and restart the IDE.
3. Assign the AS source with an appropriate editing experience:
a) Open Tools->Options->Text Editor,
b) Add the AS extension to the MSVC++.
4. Generate the intellisense data using IntellisenseCreator dynamic libraries. For that purpose, compile the file create_intellisense.fos with the tool you wish the intellisense data to be created for. ASCompiler.exe can be used for that purpose, but using server/client/mapper executable gives more complete information.
5. Run update_projects.bat to update the .vcproj files.
6. Put defines_patch.h file in the same directory as _defines.fos. Add the following line to _defines.fos:
#include "defines_patch.h"
7. [optional, highly recommended] Run refactor.bat.
8. [optional, highly recommended] Integrate the ASCompiler into MSVC IDE, see the msvs integration.doc file.
9. [MSVC 2005 users only] Run MSVC2005/update.bat.

FOnline.sln solution is now ready to work.

Maintenance:
  - Whenever script is enabled and disabled inside scripts.cfg, it might be useful, but not necessary, to run update_projects.bat again.
  - Whenever ASCompiler libraries are updated, run update_intellisense.bat.
  - Occasionally run refactor.bat.
  - [MSVC 2005 users only] Run MSVC2005/update.bat after each update_projects.bat run.

6. Known problems:
  - MSVC 2005 has problems with recognition of global scope functions.
  - MSVC 2010 and 2011 parsers do not ignore @ for handle, greatly limiting the overall usefulness.