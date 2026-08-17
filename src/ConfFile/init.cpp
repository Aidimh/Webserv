#include "../../includes/multiplexing/header.hpp"


void initServerBlock(Server_block& server)
{
    // Boolean flags
    server.server_found = false;
    server.host_found = false;
    server.location_found = false;
    server.root_found = false;
    server.server_name_found = false;
    server.listen_found = false;
    server.index_found = false;
    server.error_page_found = false;
    server.client_max_body_found = false;
    server.uploadLimits = false;
    server.server_has_autoindex = false;

    // Numeric counts and limits
    server.ports_count = 0;
    server.index_count = 0;
    server.location_count = 0;
    server.max_body_size = 0;

    // Strings
    server.host.clear();
    server.server_name.clear();
    server.root.clear();
    server.server_auto_index.clear();
    server.default_file.clear();
    server.autoindex.clear();

    // Containers (Vectors & Maps)
    server.listen_port.clear();
    server.listen_port_str.clear();
    server.index_files.clear();
    server.error_pages.clear();
    server.location.clear();
    server.methods.clear();
}

void initLocationConfig(Location_Config& loc)
{
    loc.root.clear();
    loc.path.clear();
    loc.upload_path.clear();
    loc.index_files.clear();
    loc.allowed_methods.clear();
    loc.cgi_extensions.clear();
    loc.cgi_paths.clear();
    loc.error_pages.clear();
    loc._return.clear();
    loc.autoindex.clear();

    loc.has_index = false;
    loc.has_root = false;
    loc.has_autoindex = false;
    loc.has_max_body_size = false;

    loc.cgi_paths_index = 0;
    loc.cgi_extns_index = 0;
    loc.max_body_size = 0;
}