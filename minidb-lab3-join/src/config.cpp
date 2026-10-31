#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <stdexcept>

#include "minidb/config.hpp"
#include "minidb/utils.hpp"

static void print();

Config::Config() {
	datadir					 = "";
	nthreads				 = NTHREADS;
	port					 = PORT;
	catalog_path			 = "";
	buffer_manager_pool_size = 3;
	index_dir				 = "index";
}

Config* Config::get_instance() {
	if (instance == nullptr) {
		instance = new Config();
	}
	return (instance);
}

void Config::set() {
	// set catalog file path
	if (datadir.empty()) {
		throw ConfigParseException("provide a data directory");
	}
	if (!dir_exists(datadir)) {
		throw ConfigParseException("No such directory: " + datadir);
	}
	auto path = std::filesystem::path(datadir);
	path /= std::string(CATALOG_FILE_NAME);

	catalog_path = path.string();
	auto p		 = std::filesystem::path(datadir) / index_dir;

	if (!std::filesystem::exists(p)) {
		std::filesystem::create_directory(p);
	}
	index_dir = p.string();
}

void Config::get(int argc, char** argv) {
	static struct option long_options[] = {{"datadir", required_argument, 0, 'd'},
										   {"nthreads", required_argument, 0, 'n'},
										   {"port", required_argument, 0, 'p'},
										   {"help", no_argument, 0, '?'}};
	int					 option_index, c, tmp;

	option_index = 0;
	for (;;) {
		c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'd':
			this->datadir = optarg;
			if (!dir_exists(this->datadir)) {
				throw ConfigParseException("No such directory: " + this->datadir);
			}
			break;
		case 'n':
			tmp = atoi(optarg);
			if (tmp <= 1) {
				throw ConfigParseException("config parse error");
			}
			this->nthreads = tmp;
			break;
		case 'p':
			tmp		   = atoi(optarg);
			this->port = tmp;
			break;
		case '?':
			print();
			exit(EXIT_SUCCESS);

		default:
			throw ConfigParseException("Invalid Argument");
			print();
			exit(EXIT_FAILURE);
		}
	}
	this->set();
}

std::ostream& operator<<(std::ostream& o, const Config& config) {
	o << "datadir: 		" << config.datadir << std::endl;
	o << "number of threads: 	" << config.nthreads << std::endl;
	o << "port:		" << config.port << std::endl;
	o << "catalog path: " << config.catalog_path << std::endl;
	o << "index dir: " << config.index_dir << std::endl;

	return (o);
}

static void print() {
	const char* usage = "usage:\nminidb [options]\n"
						"\t--datadir <path>	path to the data directory\n"
						"\t--nthreads <number>	number of threads\n"
						"\t--port <number> port\n"
						"\t--help 			display this information\n";
	std::cout << usage;
}
