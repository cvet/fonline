#include "Interface.h"
#include "Script.h"
#include <vector>

std::vector<std::pair<InterfaceListKey, InterfaceList*>> Interface::AllInterface;
Interface * Interface::Null = InterfaceNull::Create();

Interface::Interface( std::string name ) : Name( name.c_str() )
{

}

void Interface::Call( )
{

}

Interface * Interface::Create( std::string name )
{
	Interface* result = new Interface( name );
	return result;
}

Interface * Interface::Get( std::string name )
{
	return Null;
}

std::string Interface::Register( int pragma, std::string name, std::string method )
{
	if( method.empty( ) )
	{
		if( Script::GetEngine( )->RegisterInterface( name.c_str( ) ) < 0 )
			return "Invalid pragma data, Interface not registered, pragma '{}'.\n";
	}
	else
	{
		if( Script::GetEngine( )->RegisterInterfaceMethod( name.c_str( ), method.c_str( ) ) < 0 )
			return "Invalid pragma data, Interface method not registered, pragma '{}'.\n";

		const char* backupNamespace = Script::GetEngine( )->GetDefaultNamespace( );
		Script::GetEngine( )->SetDefaultNamespace( name.c_str( ) );
		if( Script::GetEngine( )->RegisterFuncdef( method.c_str( ) ) < 0 )
		{
			Script::GetEngine( )->SetDefaultNamespace( backupNamespace );
			return "Invalid pragma data, funcdef for interface method not registered, pragma '{}'.\n";
		}
		Script::GetEngine( )->SetDefaultNamespace( backupNamespace );
	}

	bool isTarget = false;
#ifdef FONLINE_SERVER
	isTarget = pragma == PRAGMA_SERVER;
#endif
#ifdef FONLINE_CLIENT
	isTarget = pragma == PRAGMA_CLIENT;
#endif
	if( isTarget )
	{
		Interface* iface = Create( name );
		//AllInterface.push_back( std::pair<> iface );
	}
	return "";
}

InterfaceNull::InterfaceNull( ) : Interface( "Null" )
{

}

void InterfaceNull::Call( )
{
	WriteLog( "Null interface call\n" );
}

Interface * InterfaceNull::Create( )
{
	return new InterfaceNull();
}
