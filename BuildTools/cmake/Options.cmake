include_guard()

macro(DeclareBoolOption var desc value)
	if(NOT DEFINED ${var})
		set(${var} ${value})
	endif()

	option(${var} ${desc} ${value})
	StatusMessage("${var} = ${${var}}")
endmacro()

macro(DeclareValueOption var desc value)
	GetCacheProperty(cacheType ${var} TYPE)

	if(cacheType STREQUAL "BOOL" AND ("${${var}}" STREQUAL "ON" OR "${${var}}" STREQUAL "OFF"))
		set(${var} "${value}")
	endif()

	if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
		set(${var} "${value}")
	endif()

	set(${var} "${${var}}" CACHE STRING "${desc}" FORCE)
	StatusMessage("${var} = ${${var}}")
endmacro()

macro(DeclareBoolOptions)
	set(optionArgs ${ARGN})
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
	set(optionArgs ${ARGN})
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
	set(optionArgs ${ARGN})
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
