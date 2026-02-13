#pragma once

#include <string>
#include <libssh/libssh.h>

class SshService {
public:
    void startTask(const std::string& host, const std::string& user, const std::string& pass, int verbosity, int port);
    
    bool isConnected() const;
    void writeChar(char c);
    std::string readOutput();
    std::string readOutputNonBlocking();
    void close();

private:
    // SSH Task, cause overflow if it runs in the main loop, so it must run in a dedicated FreeRTOS task with a larger stack
    static void connectTask(void* pvParams);

    bool connect(const std::string& host, const std::string& user, const std::string& pass, int verbosity, int port=22);
    bool authenticate(const std::string& password);
    bool openChannel();
    bool requestPty();
    bool startShell();

    ssh_session session = nullptr;
    ssh_channel channel = nullptr;
    bool connected = false;
};
