#pragma once

#include <string>

class NetcatService {
public:
    void startTask(const std::string& host, int verbosity, uint16_t port, bool lineBuffer = false);    
    bool isConnected() const;
    void writeChar(char c);
    std::string readOutputNonBlocking();
    void close();

private:
    // Netcat Task, cause overflow if it runs in the main loop, so it must run in a dedicated FreeRTOS task with a larger stack
    static void connectTask(void* pvParams);

    bool connect(const std::string& host, int verbosity, uint16_t port, bool lineBuffer);
    bool openSocket(const std::string& host, uint16_t port);
    void setNonBlocking();

    int  sock  = -1;        // lwIP socket FD
    bool connected = false;
    bool buffered  = false; // if true, send() when '\n' received
    std::string txBuf;
};
