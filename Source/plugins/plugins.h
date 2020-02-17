#ifndef __PLUGIN_FRAMEWORK_SUPPORT_H
#define __PLUGIN_FRAMEWORK_SUPPORT_H

#include "Module.h"

#include "Channel.h"
#include "Configuration.h"
#include "IPlugin.h"
#include "IShell.h"
#include "IStateControl.h"
#include "ISubSystem.h"
#include "JSONRPC.h"
#include "Request.h"
#include "Service.h"

#ifndef VIRTUALINPUT_DISABLED
#include "VirtualInput.h"
#endif

#ifdef __WINDOWS__
#pragma comment(lib, "plugins.lib")
#endif

#endif // __PLUGIN_FRAMEWORK_SUPPORT_H
