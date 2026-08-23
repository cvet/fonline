include_guard()

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
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 3")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareBoolOptions expects groups of 3 arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc optionValue)
		DeclareBoolOption(${optionName} "${optionDesc}" ${optionValue})
	endwhile()
endmacro()

macro(DeclareRequiredValueOptions)
	set(optionArgs "${ARGN}")
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 2")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareRequiredValueOptions expects pairs of arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc)
		DeclareValueOption(${optionName} "${optionDesc}" "")
	endwhile()
endmacro()

macro(DeclareValueOptions)
	set(optionArgs "${ARGN}")
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 3")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("DeclareValueOptions expects groups of 3 arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionDesc optionValue)
		DeclareValueOption(${optionName} "${optionDesc}" "${optionValue}")
	endwhile()
endmacro()
