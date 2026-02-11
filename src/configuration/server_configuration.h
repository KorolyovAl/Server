#pragma once

#include <boost/program_options.hpp>
 
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace config {

using namespace std::literals;

struct Args {
    std::optional<int> tick_period_ms;
    std::optional<int> save_state_period_ms;
    std::string config_file;
    std::string state_file;
    std::string www_root;
    bool randomize_spawn_points = false;
    bool show_help = false;
};


[[nodiscard]] inline Args ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions")
        ("state-file,f", po::value<std::string>()->value_name("state file"), "set path to save server state")
        ("save-state-period,p", po::value<int>()->value_name("milliseconds"), "set period to save server state");

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"s)) {
        args.show_help = true;
        std::cout << desc << std::endl;
        return args;
    }

    if (vm.count("tick-period")) {
        args.tick_period_ms = vm["tick-period"].as<int>();
    }

    if (vm.count("save-state-period")) {
        args.save_state_period_ms = vm["save-state-period"].as<int>();
    }

    if (vm.count("config-file")) {
        args.config_file = vm["config-file"].as<std::string>();
    }

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }

    if (vm.count("www-root")) {
        args.www_root = vm["www-root"].as<std::string>();
    }

    if (vm.count("randomize-spawn-points")) {
        args.randomize_spawn_points = true;
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

}