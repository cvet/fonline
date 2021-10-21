#pragma once
#include "angelscript.h"
#include "Entity.h"
#include <string>
#include <vector>
#include <list>

class InterfaceNull;

typedef int InterfaceListKey;

class InterfaceList
{
//	std::list<Interface*> List;
};

class Interface
{
	friend InterfaceNull;

	const char * Name;

	Interface( std::string name );
	void Call( );

	static Interface * Create( std::string name );
	static Interface * Null;
	static std::vector<std::pair<InterfaceListKey, InterfaceList*>> AllInterface;
public:
	static Interface * Get( std::string name );
	static std::string Register( int pragma, std::string name, std::string method );
};

class InterfaceNull : Interface
{
	InterfaceNull( );

	void Call( );

public:
	static Interface * Create( );
};

