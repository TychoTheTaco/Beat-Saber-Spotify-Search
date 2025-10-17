#include "Configuration.hpp"

namespace SpotifySearch {

// Loads the config from disk using our modInfo, then returns it for use
// other config tools such as config-utils don't use this config, so it can be
// removed if those are in use
Configuration& getConfig() {
    static Configuration config(modInfo);
    return config;
}

}
