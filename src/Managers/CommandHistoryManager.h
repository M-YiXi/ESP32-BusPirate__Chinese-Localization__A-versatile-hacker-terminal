#pragma once

#include <vector>
#include <string>

class CommandHistoryManager {
public:
    // Adds a new line to the history (unless it's empty or duplicate of last)
    void add(const std::string& line);

    // Moves up in history and returns the previous command
    const std::string& up();

    // Moves down in history and returns the next command
    const std::string& down();

    // Resets the navigation index to the end of the history
    void reset();

    // Returns total number of stored history entries
    size_t size() const;
private:
    std::vector<std::string> history;
    std::string currentLine;
    size_t index = 0;
    static constexpr size_t maxHistory = 50;
};