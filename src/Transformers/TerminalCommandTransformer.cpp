#include "TerminalCommandTransformer.h"
#include <sstream>

TerminalCommand TerminalCommandTransformer::transform(const std::string& raw) const {
    std::istringstream iss(raw);

    std::string root, subcommand, args;
    iss >> root >> subcommand;
    std::getline(iss, args);

    if (!args.empty() && args[0] == ' ') {
        args.erase(0, 1);
    }

    TerminalCommand cmd;
    cmd.setRoot(root);
    cmd.setSubcommand(subcommand);
    cmd.setArgs(args);

    return cmd;
}
