include_guard()

macro(AppendList outputVar)
	list(APPEND ${outputVar} ${ARGN})
endmacro()

macro(ListLength listVar outputVar)
	list(LENGTH ${listVar} ${outputVar})
endmacro()

macro(ListPopFront listVar)
	list(POP_FRONT ${listVar} ${ARGN})
endmacro()

macro(ListFind listVar value outputVar)
	list(FIND ${listVar} ${value} ${outputVar})
endmacro()

macro(ListGet listVar index outputVar)
	list(GET ${listVar} ${index} ${outputVar})
endmacro()

macro(ListSort listVar)
	list(SORT ${listVar})
endmacro()

macro(MathExpr outputVar)
	math(EXPR ${outputVar} "${ARGN}")
endmacro()

macro(StringReplace match replace outputVar input)
	string(REPLACE "${match}" "${replace}" ${outputVar} "${input}")
endmacro()

macro(StringFind input search outputVar)
	string(FIND "${input}" "${search}" ${outputVar})
endmacro()

macro(StringToUpper input outputVar)
	string(TOUPPER "${input}" ${outputVar})
endmacro()

macro(StringRegexMatch regex outputVar input)
	string(REGEX MATCH "${regex}" ${outputVar} "${input}")
endmacro()

macro(StringRandom outputVar)
	string(RANDOM ${ARGN} ${outputVar})
endmacro()

macro(GetFilenameComponent outputVar input)
	get_filename_component(${outputVar} ${input} ${ARGN})
endmacro()

macro(FileGlob outputVar)
	file(GLOB ${outputVar} ${ARGN})
endmacro()

macro(FileReadStrings inputFile outputVar)
	file(STRINGS "${inputFile}" ${outputVar})
endmacro()

macro(FileMakeDirectory)
	file(MAKE_DIRECTORY ${ARGV})
endmacro()

macro(FileWrite filePath content)
	file(WRITE "${filePath}" "${content}")
endmacro()

macro(FileAppend filePath content)
	file(APPEND "${filePath}" "${content}")
endmacro()

macro(FileCreateLink sourcePath destinationPath)
	file(CREATE_LINK "${sourcePath}" "${destinationPath}" ${ARGN})
endmacro()

macro(GetTargetProperty outputVar target property)
	get_target_property(${outputVar} ${target} ${property})
endmacro()

macro(ExecuteProcess)
	execute_process(${ARGV})
endmacro()

macro(RequirePackage packageName)
	find_package(${packageName} ${ARGN})
endmacro()

macro(GetCMakeProperty outputVar property)
	get_cmake_property(${outputVar} ${property})
endmacro()

macro(GetCacheProperty outputVar cacheVar property)
	get_property(${outputVar} CACHE ${cacheVar} PROPERTY ${property})
endmacro()

macro(SetGlobalProperty property value)
	set_property(GLOBAL PROPERTY ${property} ${value})
endmacro()

macro(SetTargetProperty target property value)
	set_property(TARGET ${target} PROPERTY ${property} ${value})
endmacro()

macro(SetTargetProperties target)
	set_target_properties(${target} PROPERTIES ${ARGN})
endmacro()

macro(AddCustomCommand)
	add_custom_command(${ARGV})
endmacro()

macro(AddDependencies)
	add_dependencies(${ARGV})
endmacro()

macro(AddLibrary)
	add_library(${ARGV})
endmacro()

macro(AddExecutable)
	add_executable(${ARGV})
endmacro()

macro(AddCustomTarget)
	add_custom_target(${ARGV})
endmacro()

macro(FindLibrary outputVar libraryName)
	find_library(${outputVar} ${libraryName})
endmacro()

macro(ParseArguments prefix options oneValueArgs multiValueArgs)
	cmake_parse_arguments(${prefix} "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
endmacro()

macro(TargetCompileDefinitions)
	target_compile_definitions(${ARGV})
endmacro()

macro(TargetIncludeDirectories)
	target_include_directories(${ARGV})
endmacro()

macro(TargetLinkLibraries)
	target_link_libraries(${ARGV})
endmacro()
