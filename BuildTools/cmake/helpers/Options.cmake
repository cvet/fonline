include_guard()

# Every declaration records its type and whether its final value affects baked resources.
function(RegisterDeclaredOption var optionType affectsBaking)
	get_property(registeredType GLOBAL PROPERTY _FO_OPTION_${var}_TYPE)
	get_property(registeredImpact GLOBAL PROPERTY _FO_OPTION_${var}_BAKING)
	if(registeredType AND (NOT registeredType STREQUAL optionType OR NOT registeredImpact STREQUAL affectsBaking))
		AbortMessage("Conflicting option declaration for ${var}")
	endif()
	set_property(GLOBAL APPEND PROPERTY _FO_DECLARED_OPTIONS "${var}")
	set_property(GLOBAL PROPERTY _FO_OPTION_${var}_TYPE "${optionType}")
	set_property(GLOBAL PROPERTY _FO_OPTION_${var}_BAKING "${affectsBaking}")
endfunction()

# Run immediately before code generation, after option overrides and preceding stage hooks.
function(ComputeBakingConfigHash outputVar)
	get_property(optionNames GLOBAL PROPERTY _FO_DECLARED_OPTIONS)
	list(REMOVE_DUPLICATES optionNames)
	list(SORT optionNames)
	set(configuration "fonline-baking-config-v1\n")
	foreach(optionName IN LISTS optionNames)
		get_property(affectsBaking GLOBAL PROPERTY _FO_OPTION_${optionName}_BAKING)
		if(NOT affectsBaking)
			continue()
		endif()
		get_property(optionType GLOBAL PROPERTY _FO_OPTION_${optionName}_TYPE)
		set(optionValue "${${optionName}}")
		if(optionType STREQUAL "BOOL")
			if(${optionName})
				set(optionValue 1)
			else()
				set(optionValue 0)
			endif()
		endif()
		# Length prefixes preserve strings containing separators, semicolons or newlines.
		string(LENGTH "${optionName}" nameLength)
		string(LENGTH "${optionValue}" valueLength)
		string(APPEND configuration "${nameLength}:${optionName}${optionType}${valueLength}:${optionValue}\n")
	endforeach()
	string(SHA256 configurationHash "${configuration}")
	set(${outputVar} "${configurationHash}" PARENT_SCOPE)
endfunction()

function(TryGetEnvOptionOverride var optionType hasOverrideOut valueOut)
	set(hasOverride OFF)
	set(overrideValue "")

	if("${var}" MATCHES "^FO_" AND DEFINED ENV{${var}})
		set(hasOverride ON)
		set(overrideValue "$ENV{${var}}")

		if("${optionType}" STREQUAL "BOOL")
			string(TOUPPER "${overrideValue}" overrideValueNormalized)

			if(
				overrideValueNormalized STREQUAL "1" OR
				overrideValueNormalized STREQUAL "ON" OR
				overrideValueNormalized STREQUAL "TRUE" OR
				overrideValueNormalized STREQUAL "YES")
				set(overrideValue ON)
			elseif(
				overrideValueNormalized STREQUAL "0" OR
				overrideValueNormalized STREQUAL "OFF" OR
				overrideValueNormalized STREQUAL "FALSE" OR
				overrideValueNormalized STREQUAL "NO")
				set(overrideValue OFF)
			else()
				AbortMessage("Invalid boolean environment override ${var}='$ENV{${var}}'. Use 1/0, ON/OFF, TRUE/FALSE, or YES/NO")
			endif()
		endif()
	endif()

	set(${hasOverrideOut} ${hasOverride} PARENT_SCOPE)
	set(${valueOut} "${overrideValue}" PARENT_SCOPE)
endfunction()

macro(DeclareBoolOption var desc value)
	cmake_parse_arguments(_fo_decl "BAKING" "" "" ${ARGN})
	if(_fo_decl_UNPARSED_ARGUMENTS)
		AbortMessage("DeclareBoolOption: unexpected arguments ${_fo_decl_UNPARSED_ARGUMENTS}")
	endif()
	RegisterDeclaredOption("${var}" BOOL "${_fo_decl_BAKING}")
	TryGetEnvOptionOverride("${var}" "BOOL" hasEnvOverride envOverrideValue)

	if(hasEnvOverride)
		set(${var} ${envOverrideValue})
	elseif(NOT DEFINED ${var})
		set(${var} ${value})
	endif()

	option(${var} ${desc} ${value})

	if(hasEnvOverride)
		set(${var} ${envOverrideValue} CACHE BOOL "${desc}" FORCE)
		StatusMessage("${var} = ${${var}} (env)")
	else()
		StatusMessage("${var} = ${${var}}")
	endif()
endmacro()

macro(DeclareValueOption var desc value)
	cmake_parse_arguments(_fo_decl "BAKING" "" "" ${ARGN})
	if(_fo_decl_UNPARSED_ARGUMENTS)
		AbortMessage("DeclareValueOption: unexpected arguments ${_fo_decl_UNPARSED_ARGUMENTS}")
	endif()
	RegisterDeclaredOption("${var}" STRING "${_fo_decl_BAKING}")
	TryGetEnvOptionOverride("${var}" "STRING" hasEnvOverride envOverrideValue)
	GetCacheProperty(cacheType ${var} TYPE)

	if(cacheType STREQUAL "BOOL" AND ("${${var}}" STREQUAL "ON" OR "${${var}}" STREQUAL "OFF"))
		set(${var} "${value}")
	endif()

	if(hasEnvOverride)
		set(${var} "${envOverrideValue}")
	elseif(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
		set(${var} "${value}")
	endif()

	set(${var} "${${var}}" CACHE STRING "${desc}" FORCE)

	if(hasEnvOverride)
		StatusMessage("${var} = ${${var}} (env)")
	else()
		StatusMessage("${var} = ${${var}}")
	endif()
endmacro()

macro(DeclareBoolOptions)
	set(optionArgs "${ARGN}")
	set(bakingKeyword "")
	if("${ARGV0}" STREQUAL "BAKING")
		ListPopFront(optionArgs bakingKeyword)
	endif()
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 3")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareBoolOptions expects groups of 3 arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc optionValue)
		DeclareBoolOption(${optionName} "${optionDesc}" ${optionValue} ${bakingKeyword})
	endwhile()
endmacro()

macro(DeclareRequiredValueOptions)
	set(optionArgs "${ARGN}")
	set(bakingKeyword "")
	if("${ARGV0}" STREQUAL "BAKING")
		ListPopFront(optionArgs bakingKeyword)
	endif()
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 2")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareRequiredValueOptions expects pairs of arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc)
		DeclareValueOption(${optionName} "${optionDesc}" "" ${bakingKeyword})
	endwhile()
endmacro()

macro(DeclareValueOptions)
	set(optionArgs "${ARGN}")
	set(bakingKeyword "")
	if("${ARGV0}" STREQUAL "BAKING")
		ListPopFront(optionArgs bakingKeyword)
	endif()
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 3")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareValueOptions expects groups of 3 arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc optionValue)
		DeclareValueOption(${optionName} "${optionDesc}" "${optionValue}" ${bakingKeyword})
	endwhile()
endmacro()
