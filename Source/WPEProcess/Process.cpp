 /*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// process.cpp : Defines the entry point for the console application.
//

#include "Module.h"

MODULE_NAME_DECLARATION(BUILD_REFERENCE)

namespace WPEFramework {

namespace Process {

    class WorkerPoolImplementation : public Core::IIPCServer, public Core::WorkerPool {
    private:
        class Sink : public Core::ServiceAdministrator::ICallback {
        public:
            Sink() = delete;
            Sink(const Sink&) = delete;
            Sink& operator= (const Sink&) = delete;

#ifdef __WINDOWS__
#pragma warning(disable : 4355)
#endif
            Sink(WorkerPoolImplementation& parent) 
                : _parent(parent)
                , _job(*this) 
            {
            }
#ifdef __WINDOWS__
#pragma warning(default : 4355)
#endif
            ~Sink() override
            {
            }

        public:
            void Dispatch() {
                Core::ServiceAdministrator::Instance().FlushLibraries();

                if (Core::ServiceAdministrator::Instance().Instances() == 0) {
                    // Seems there is no more live here, time to signal the
                    // WorkerPool to quit running and close down...
                    _parent.Stop();
                }
            }
            void Destructed() override {
                Core::ProxyType<Core::IDispatch> job(_job.Aquire());

                if (job.IsValid() == true) {
                    _parent.Submit(job);
                }
            }

        private:
            WorkerPoolImplementation& _parent;
            Core::ThreadPool::JobType<Sink&> _job;
        };

    public:
        WorkerPoolImplementation() = delete;
        WorkerPoolImplementation(const WorkerPoolImplementation&) = delete;
        WorkerPoolImplementation& operator=(const WorkerPoolImplementation&) = delete;

#ifdef __WINDOWS__
#pragma warning(disable : 4355)
#endif
        WorkerPoolImplementation(const uint8_t threads, const uint32_t stackSize, const uint32_t queueSize)
            : WorkerPool(threads - 1, stackSize, queueSize)
            , _announceHandler(nullptr)
            , _sink(*this)
        {
            Core::ServiceAdministrator::Instance().Callback(&_sink);

            if (threads > 1) {
                SYSLOG(Logging::Notification, ("Spawned: %d additional minions.", threads - 1));
            }
        }
#ifdef __WINDOWS__
#pragma warning(default : 4355)
#endif

        ~WorkerPoolImplementation()
        {
            Core::ServiceAdministrator::Instance().Callback(nullptr);

            // Disable the queue so the minions can stop, even if they are processing and waiting for work..
            Core::WorkerPool::Stop();
        }
        void Announcements(Core::IIPCServer* announces)
        {
            ASSERT((announces != nullptr) ^ (_announceHandler != nullptr));
            _announceHandler = announces;
        }
        void Run()
        {

            Core::WorkerPool::Run();
            Core::WorkerPool::Join();
        }
        void Stop()
        {
            Core::WorkerPool::Shutdown();
        }


    protected:
        void Procedure(Core::IPCChannel& channel, Core::ProxyType<Core::IIPC>& data) override
        {
            Core::ProxyType<RPC::Job> job(RPC::Job::Instance());

            job->Set(channel, data, _announceHandler);

            WorkerPool::Submit(Core::ProxyType<Core::IDispatch>(job));
        }
    private:
        Core::IIPCServer* _announceHandler;
        Sink _sink;
    };

    class ConsoleOptions : public Core::Options {
    public:
        ConsoleOptions(int argumentCount, TCHAR* arguments[])
            : Core::Options(argumentCount, arguments, _T("h:l:c:r:p:s:d:a:m:i:u:g:t:e:x:V:v:"))
            , Locator(nullptr)
            , ClassName(nullptr)
            , RemoteChannel(nullptr)
            , InterfaceId(Core::IUnknown::ID)
            , Version(~0)
            , Exchange(0)
            , PersistentPath()
            , SystemPath()
            , DataPath()
            , VolatilePath()
            , AppPath()
            , ProxyStubPath()
            , User(nullptr)
            , Group(nullptr)
            , Threads(1)
            , EnabledLoggings(0)
        {
            Parse();
        }
        ~ConsoleOptions()
        {
        }

    public:
        const TCHAR* Locator;
        const TCHAR* ClassName;
        const TCHAR* RemoteChannel;
        uint32_t InterfaceId;
        uint32_t Version;
        uint32_t Exchange;
        string PersistentPath;
        string SystemPath;
        string DataPath;
        string VolatilePath;
        string AppPath;
        string ProxyStubPath;
        const TCHAR* User;
        const TCHAR* Group;
        uint8_t Threads;
        uint32_t EnabledLoggings;

    private:
        string Strip(const TCHAR text[]) const
        {
            int length = static_cast<int>(strlen(text));
            if (length > 0) {
                if (*text != '"') {
                    return (string(text, length));
                } else {
                    return (string(&text[1], (length > 2 ? length - 2 : length - 1)));
                }
            }
            return (string());
        }
        virtual void Option(const TCHAR option, const TCHAR* argument)
        {
            switch (option) {
            case 'l':
                Locator = argument;
                break;
            case 'c':
                ClassName = argument;
                break;
            case 'r':
                RemoteChannel = argument;
                break;
            case 'p':
                PersistentPath = Strip(argument);
                break;
            case 's':
                SystemPath = Strip(argument);
                break;
            case 'd':
                DataPath = Strip(argument);
                break;
            case 'v':
                VolatilePath = Strip(argument);
                break;
            case 'a':
                AppPath = Strip(argument);
                break;
            case 'm':
                ProxyStubPath = Strip(argument);
                break;
            case 'u':
                User = argument;
                break;
            case 'g':
                Group = argument;
                break;
            case 'i':
                InterfaceId = Core::NumberType<uint32_t>(Core::TextFragment(argument)).Value();
                break;
            case 'e':
                EnabledLoggings = Core::NumberType<uint32_t>(Core::TextFragment(argument)).Value();
                break;
            case 'V':
                Version = Core::NumberType<uint32_t>(Core::TextFragment(argument)).Value();
                break;
            case 'x':
                Exchange = Core::NumberType<uint32_t>(Core::TextFragment(argument)).Value();
                break;
            case 't':
                Threads = Core::NumberType<uint8_t>(Core::TextFragment(argument)).Value();
                break;
            case 'h':
            default:
                RequestUsage(true);
                break;
            }
        }
    };

    static void* CheckInstance(const string& path, const TCHAR locator[], const TCHAR className[], const uint32_t ID, const uint32_t version)
    {
        void* result = nullptr;

        if (path.empty() == false) {
            Core::ServiceAdministrator& admin(Core::ServiceAdministrator::Instance());

            string libraryPath = locator;
            if (libraryPath.empty() || (libraryPath[0] != '/')) {
                // Relative path, prefix with path name.
                string pathName(Core::Directory::Normalize(path));
                libraryPath = pathName + locator;
            }

            Core::Library library(libraryPath.c_str());

            if (library.IsLoaded() == true) {
                // Instantiate the object
                result = admin.Instantiate(library, className, version, ID);
            }
        }

        return (result);
    }

    static void* AquireInterfaces(ConsoleOptions& options)
    {
        void* result = nullptr;

        if ((options.Locator != nullptr) && (options.ClassName != nullptr)) {
            result = CheckInstance(options.PersistentPath, options.Locator, options.ClassName, options.InterfaceId, options.Version);

            if (result == nullptr) {
                result = CheckInstance(options.SystemPath, options.Locator, options.ClassName, options.InterfaceId, options.Version);

                if (result == nullptr) {
                    result = CheckInstance(options.DataPath, options.Locator, options.ClassName, options.InterfaceId, options.Version);

                    if (result == nullptr) {
                        string searchPath(options.AppPath.empty() == false ? Core::Directory::Normalize(options.AppPath) : string());

                        result = CheckInstance((searchPath + _T("Plugins/")), options.Locator, options.ClassName, options.InterfaceId, options.Version);
                    }
                }
            }
        }

        return (result);
    }

class ExitHandler {
public:
    ExitHandler(const ExitHandler&) = delete;
    ExitHandler& operator=(const ExitHandler&) = delete;

    ExitHandler()
        : _server()
        , _engine()
        , _proxyStubs()
    {
        _instance = this;

        if (atexit(ForcedShutdown) != 0) {
            TRACE_L1("Could not register @exit handler.");
            exit(EXIT_FAILURE);
        } else {
            TRACE_L1("Spawning a new process: %d.", Core::ProcessInfo().Id());
            #ifndef __WINDOWS__
            struct sigaction sa;
            memset(&sa, 0, sizeof(struct sigaction));
            sigemptyset(&sa.sa_mask);
            sa.sa_handler = ExitDaemonHandler;
            sa.sa_flags = 0; // not SA_RESTART!;

            sigaction(SIGINT, &sa, nullptr);
            sigaction(SIGTERM, &sa, nullptr);
            #ifdef __DEBUG__
            sigaction(SIGSEGV, &sa, nullptr);
            #endif
            sigaction(SIGQUIT, &sa, nullptr);
            #endif
        }
    }
    virtual ~ExitHandler()
    {
    }

public:
    void Startup(const uint8_t threadCount, const Core::NodeId& remoteNode)
    {
        // Seems like we have enough information, open up the Process communcication Channel.
        _engine = Core::ProxyType<Process::WorkerPoolImplementation>::Create(threadCount, Core::Thread::DefaultStackSize(), 16);

        // Whenever someone is looking for a WorkerPool, here it is, register it..
        Core::IWorkerPool::Assign(&(*_engine));

        _server = (Core::ProxyType<RPC::CommunicatorClient>::Create(remoteNode, Core::ProxyType<Core::IIPCServer>(_engine)));
        _engine->Announcements(_server->Announcement());
    }
    void Run(const string& pathName, const uint32_t interfaceId, void* base, const uint32_t sequenceId)
    {
        uint32_t result;
        uint32_t waitTime (RPC::CommunicationTimeOut != Core::infinite ? 2 * RPC::CommunicationTimeOut : RPC::CommunicationTimeOut);

        TRACE_L1("Loading ProxyStubs from %s", (pathName.empty() == false ? pathName.c_str() : _T("<< No Proxy Stubs Loaded >>")));

        if (pathName.empty() == false) {
            Core::Directory index(pathName.c_str(), _T("*.so"));

            while (index.Next() == true) {
                Core::Library library(index.Current().c_str());

                if (library.IsLoaded() == true) {
                    _proxyStubs.push_back(library);
                }
            }
        }
 
        if ((result = _server->Open(waitTime, interfaceId, base, sequenceId)) == Core::ERROR_NONE) {
            TRACE_L1("Process up and running: %d.", Core::ProcessInfo().Id());
            _engine->Run();
        } else {
            TRACE_L1("Could not open the connection, error (%d)", result);
        }
    }
    void Shutdown()
    {
        TRACE_L1("Entering @Exit. Cleaning up process: %d.", Core::ProcessInfo().Id());

        if(_engine.IsValid() == true) {
            _engine->Stop();
            _engine.Release();
        }

        if (_server.IsValid() == true) {

            // We are done, close the channel and unregister all shit we added...
            _server->Close(2 * RPC::CommunicationTimeOut);

            _proxyStubs.clear();

            _server.Release();
        }

        // We are going to tear down the stugg. Unregistere the Worker Pool
        if (Core::IWorkerPool::IsAvailable() == true) {
            Core::IWorkerPool::Assign(nullptr);
        }

        Core::Singleton::Dispose();
        TRACE_L1("Leaving @Exit. Cleaning up process: %d.", Core::ProcessInfo().Id());
    }

private:
    #ifndef __WINDOWS__
    static void ExitDaemonHandler(int signo)
    {
        TRACE_L1("Signal received %d.", signo);
        syslog(LOG_NOTICE, "Signal received %d.", signo);

        if ((signo == SIGTERM) || (signo == SIGQUIT)) {
            if ( (_instance != nullptr) && (_instance->_engine.IsValid() == true) )
            {
                _instance->_engine->Stop();
            }
        } else if (signo == SIGSEGV) {
            DumpCallStack();
            // now invoke the default segfault handler
            signal(signo, SIG_DFL);
            kill(getpid(), signo);
        }
    }
    #endif
    static void ForcedShutdown ()
    {
        if (_instance != nullptr) {
            _instance->Shutdown();
        }
    }

private:
    Core::ProxyType<RPC::CommunicatorClient> _server;
    Core::ProxyType<WorkerPoolImplementation> _engine;
    std::list<Core::Library> _proxyStubs;

    static ExitHandler* _instance;
};


ExitHandler*  ExitHandler::_instance = nullptr;

}
} // Process

using namespace WPEFramework;

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char** argv)
#endif
{
    // Give the debugger time to attach to this process..
    // Sleep(20000);

    Process::ExitHandler exitHandler;

    Process::ConsoleOptions options(argc, argv);

    if ((options.RequestUsage() == true) || (options.Locator == nullptr) || (options.ClassName == nullptr) || (options.RemoteChannel == nullptr) || (options.Exchange == 0)) {
        printf("Process [-h] \n");
        printf("         -l <locator>\n");
        printf("         -c <classname>\n");
        printf("         -r <communication channel>\n");
        printf("         -x <eXchange identifier>\n");
        printf("        [-i <interface ID>]\n");
        printf("        [-t <thread count>\n");
        printf("        [-V <version>]\n");
        printf("        [-u <user>]\n");
        printf("        [-g <group>]\n");
        printf("        [-p <persistent path>]\n");
        printf("        [-s <system path>]\n");
        printf("        [-d <data path>]\n");
        printf("        [-v <volatile path>]\n");
        printf("        [-a <app path>]\n");
        printf("        [-m <proxy stub library path>]\n");
        printf("        [-e <enabled SYSLOG categories>]\n\n");
        printf("This application spawns a seperate process space for a plugin. The plugins");
        printf("are searched in the same order as they are done in process. Starting from:\n");
        printf(" 1) <persistent path>/<locator>\n");
        printf(" 2) <system path>/<locator>\n");
        printf(" 3) <data path>/<locator>\n");
        printf(" 4) <app path>/Plugins/<locator>\n\n");
        printf("Within the DSO, the system looks for an object with <classname>, this object must implement ");
        printf("the interface, indicated byt the Id <interfaceId>, and if passed, the object should be of ");
        printf("version <version>. All these conditions must met for an object to be instantiated and thus run.\n\n");

        for (uint8_t teller = 0; teller < argc; teller++) {
            printf("Argument [%02d]: %s\n", teller, argv[teller]);
        }
    } else {
        Core::NodeId remoteNode(options.RemoteChannel);

        // Any remote connection that will be spawned from here, will have this ExchangeId as its parent ID.
        Core::SystemInfo::SetEnvironment(_T("COM_PARENT_EXCHANGE_ID"), Core::NumberType<uint32_t>(options.Exchange).Text());

        TRACE_L1("Opening a trace file with ID: [%d].", options.Exchange);

        // Due to the LXC container support all ID's get mapped. For the TraceBuffer, use the host given ID.
        Trace::TraceUnit::Instance().Open(options.Exchange);

        // Time to open up the LOG tracings as specified by the caller.
        Logging::LoggingType<Logging::Startup>::Enable((options.EnabledLoggings & 0x00000001) != 0);
        Logging::LoggingType<Logging::Shutdown>::Enable((options.EnabledLoggings & 0x00000002) != 0);
        Logging::LoggingType<Logging::Notification>::Enable((options.EnabledLoggings & 0x00000004) != 0);

        if (remoteNode.IsValid()) {
            void* base = nullptr;

            TRACE_L1("Spawning a new plugin %s.", options.ClassName);

            // Firts make sure we apply the correct rights to our selves..
            if (options.User != nullptr) {
                Core::ProcessInfo::User(string(options.User));
            }

            if (options.Group != nullptr) {
                Core::ProcessInfo().Group(string(options.Group));
            }

            exitHandler.Startup(options.Threads, remoteNode);

            // Register an interface to handle incoming requests for interfaces.
            if ((base = Process::AquireInterfaces(options)) != nullptr) {

                TRACE_L1("Allright time to start running");
                exitHandler.Run(options.ProxyStubPath, options.InterfaceId, base, options.Exchange);
            }
        }
    }

    exitHandler.Shutdown(); 
    return 0;
}
