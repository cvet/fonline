/**
 * \file
 * Darwin-specific interface to the logger
 *
 */
#include <config.h>

#if defined(HOST_IOS)

#include <os/log.h>
#include "mono-logger-internals.h"
void
mono_log_open_asl (const char *path, void *userData)
{
}

void
mono_log_write_asl (const char *log_domain, GLogLevelFlags level, mono_bool hdr, const char *message)
{
	switch (level & G_LOG_LEVEL_MASK)
	{
		case G_LOG_LEVEL_MESSAGE:
			os_log (OS_LOG_DEFAULT, "%s%s%s\n",
				log_domain != NULL ? log_domain : "",
				log_domain != NULL ? ": " : "",
				message);
			break;
		case G_LOG_LEVEL_INFO:
			os_log_info (OS_LOG_DEFAULT, "%s%s%s\n",
				log_domain != NULL ? log_domain : "",
				log_domain != NULL ? ": " : "",
				message);
			break;
		case G_LOG_LEVEL_DEBUG:
			os_log_debug (OS_LOG_DEFAULT, "%s%s%s\n",
				log_domain != NULL ? log_domain : "",
				log_domain != NULL ? ": " : "",
				message);
			break;
		case G_LOG_LEVEL_ERROR:
		case G_LOG_LEVEL_WARNING:
			os_log_error (OS_LOG_DEFAULT, "%s%s%s\n",
				log_domain != NULL ? log_domain : "",
				log_domain != NULL ? ": " : "",
				message);
		case G_LOG_LEVEL_CRITICAL:
		default:
			os_log_fault (OS_LOG_DEFAULT, "%s%s%s\n",
				log_domain != NULL ? log_domain : "",
				log_domain != NULL ? ": " : "",
				message);
			break;
	}

	if (level & G_LOG_LEVEL_ERROR)
		abort();
}

void
mono_log_close_asl ()
{
}
#endif
