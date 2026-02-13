#include "WebTerminalInput.h"

WebTerminalInput::WebTerminalInput(WebSocketServer& server) : server(server) {}

char WebTerminalInput::handler() {
    return server.readCharBlocking();
}

char WebTerminalInput::readChar() {
    return server.readCharNonBlocking();
}

void WebTerminalInput::waitPress() {
    server.readCharBlocking();
}
