#ifdef _MSC_VER
#ifdef __FONLINE_SOLUTION__
// here we put whatever will make intellisense smarter
// it is advisable to comment out these lines while running ProjectCreator to reduce the processing time
#ifdef __SERVER
#include "intellisense.h"
#endif
#ifdef __CLIENT
#include "intellisense_client.h"
#endif
#ifdef __MAPPER
#include "intellisense_mapper.h"
#endif
#define import extern
#define interface class
#define private
#endif
#endif