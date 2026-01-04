#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string host{"127.0.0.1"};
    int port{8080};
};

void printUsage() {
    std::cout << "Usage:\n"
              << "  commandlineclient list-projects [--host HOST] [--port PORT]\n"
              << "  commandlineclient list-cues <projectId> [--host HOST] [--port PORT]\n"
              << "  commandlineclient play-cue <projectId> <cueId> [--host HOST] [--port PORT]\n"
              << "  commandlineclient help\n";
}

Options parseOptions(int& argc, char* argv[]) {
    Options opts{};
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--host" && i + 1 < argc) {
            opts.host = argv[++i];
        } else if (arg.rfind("--host=", 0) == 0) {
            opts.host = arg.substr(7);
        } else if (arg == "--port" && i + 1 < argc) {
            opts.port = std::stoi(argv[++i]);
        } else if (arg.rfind("--port=", 0) == 0) {
            opts.port = std::stoi(arg.substr(7));
        } else {
            // Keep positional args intact
            continue;
        }
        // Remove processed args from argc to simplify command parsing
        argv[i] = nullptr;
    }
    return opts;
}

httplib::Client makeClient(const Options& opts) {
    httplib::Client cli(opts.host, opts.port);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    return cli;
}

int listCues(const Options& opts, const std::string& projectId) {
    auto cli = makeClient(opts);

    // Fetch project to get its cueOrder
    if (auto projRes = cli.Get(std::string("/api/projects/") + projectId)) {
        if (projRes->status != 200) {
            std::cerr << "Project " << projectId << " not found (status=" << projRes->status << ")\n";
            return 1;
        }
        auto projectJson = nlohmann::json::parse(projRes->body);
        auto cueOrder = projectJson.value("cueOrder", nlohmann::json::array());

        // Build a set of cue ids in project for quick lookup
        std::unordered_set<std::string> cueIds;
        for (const auto& id : cueOrder) {
            cueIds.insert(id.get<std::string>());
        }

        // Fetch all cues scoped to the project and print those in project order
        if (auto res = cli.Get(std::string("/api/projects/") + projectId + "/cues")) {
            if (res->status == 200) {
                auto allCues = nlohmann::json::parse(res->body);
                // Index cues by id
                std::unordered_map<std::string, nlohmann::json> cueMap;
                for (const auto& cue : allCues) {
                    cueMap.emplace(cue.value("id", ""), cue);
                }

                std::cout << "Cues for project " << projectId << ":\n";
                for (const auto& idVal : cueOrder) {
                    auto id = idVal.get<std::string>();
                    auto it = cueMap.find(id);
                    if (it != cueMap.end()) {
                        const auto& cue = it->second;
                        std::cout << "- cueId=" << cue.value("id", "") << " name=\"" << cue.value("name", "")
                                  << "\" sceneId=" << cue.value("sceneId", "") << "\n";
                    } else {
                        std::cout << "- cueId=" << id << " (missing)\n";
                    }
                }
                return 0;
            }
            std::cerr << "Server responded " << res->status << " for /projects/" << projectId << "/cues\n";
        } else {
            std::cerr << "Failed to reach server at " << opts.host << ":" << opts.port << ", error=" << res.error() << "\n";
            return 1;
        }

        return 1;
    } else {
        std::cerr << "Failed to reach server at " << opts.host << ":" << opts.port << ", error=" << projRes.error() << "\n";
        return 1;
    }
}

int listProjects(const Options& opts) {
    auto cli = makeClient(opts);
    if (auto res = cli.Get("/api/projects")) {
        if (res->status == 200) {
            auto body = nlohmann::json::parse(res->body);
            std::cout << "Projects:\n";
            for (const auto& proj : body) {
                std::cout << "- id=" << proj.value("id", "") << " name=\"" << proj.value("name", "") << "\"\n";
            }
            return 0;
        }
        std::cerr << "Server responded " << res->status << " for /projects\n";
    } else {
        std::cerr << "Failed to reach server at " << opts.host << ":" << opts.port << ", error=" << res.error() << "\n";
        return 1;
    }
    return 1;
}

int playCue(const Options& opts, const std::string& projectId, const std::string& cueId) {
    auto cli = makeClient(opts);

    // Verify the cue belongs to the given project
    if (auto projRes = cli.Get(std::string("/api/projects/") + projectId)) {
        if (projRes->status != 200) {
            std::cerr << "Project " << projectId << " not found (status=" << projRes->status << ")\n";
            return 1;
        }
        auto projectJson = nlohmann::json::parse(projRes->body);
        auto cueOrder = projectJson.value("cueOrder", nlohmann::json::array());
        bool found = false;
        for (const auto& id : cueOrder) {
            if (id.get<std::string>() == cueId) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Cue " << cueId << " is not part of project " << projectId << "\n";
            return 1;
        }
    } else {
        std::cerr << "Failed to reach server at " << opts.host << ":" << opts.port << ", error=" << projRes.error() << "\n";
        return 1;
    }

    nlohmann::json payload{{"cueId", cueId}};
    if (auto res = cli.Post("/api/renderer/playCue", "application/json", payload.dump())) {
        if (res->status == 200) {
            std::cout << "Requested playCue for cueId=" << cueId << " (project=" << projectId << ")\n";
            return 0;
        }
        std::cerr << "playCue responded " << res->status << ", response: " << res->body << "\n";
    }

    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    Options opts = parseOptions(argc, argv);
    std::string command = argv[1] ? argv[1] : "";

    if (command == "help") {
        printUsage();
        return 0;
    }

    if (command == "list-projects") {
        return listProjects(opts);
    }

    if (command == "list-cues") {
        if (argc < 3 || argv[2] == nullptr) {
            std::cerr << "list-cues requires <projectId>\n";
            printUsage();
            return 1;
        }
        return listCues(opts, argv[2]);
    }

    if (command == "play-cue") {
        if (argc < 4 || argv[2] == nullptr || argv[3] == nullptr) {
            std::cerr << "play-cue requires <projectId> <cueId>\n";
            printUsage();
            return 1;
        }
        return playCue(opts, argv[2], argv[3]);
    }

    printUsage();
    return 1;
}
