#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/bot.hpp"
#include "app/config.hpp"
#include "util/log.hpp"

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void usage() {
    std::puts(
        "usage: hft_bot [--config FILE] [--snapshot] [--debug]\n"
        "  --config FILE   bot config (default: config/arbitrum.json)\n"
        "  --snapshot      load pools, print them and exit\n"
        "  --debug         verbose logging\n"
        "RPC endpoints come from the config or ARBITRUM_RPC_URL / ARBITRUM_WS_URL (.env).");
}

}

int main(int argc, char** argv) {
    std::string config_path = "config/arbitrum.json";
    bool snapshot = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (std::strcmp(argv[i], "--snapshot") == 0) {
            snapshot = true;
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            hft::log::set_level(hft::log::Level::debug);
        } else {
            usage();
            return std::strcmp(argv[i], "--help") == 0 ? 0 : 2;
        }
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    try {
        hft::app::load_dotenv();
        hft::app::Bot bot(hft::app::Config::load(config_path));
        bot.bootstrap();
        bot.print_snapshot();
        if (!snapshot) {
            bot.run(g_stop);
        }
    } catch (const std::exception& e) {
        hft::log::error("fatal: {}", e.what());
        return 1;
    }
    return 0;
}
