#include "minidb/config.hpp"
#include "minidb/database.hpp"
#include <iostream>

#include "minidb/catalog.hpp"
#include "minidb/shutdown.hpp"

#include <csignal>
#include <iomanip>
#include <iostream>

// shutdown static
std::atomic<bool> shutdown_requested{false};

Config* Config::instance = nullptr;

void signal_handler(int signal) {
	if (signal == SIGINT) {
		shutdown_requested = true;
	}
}

const char* BANNER = R"(                         ____    ____      
 /'\_/`\  __          __/\  _`\ /\  _`\    
/\      \/\_\    ___ /\_\ \ \/\ \ \ \L\ \  
\ \ \__\ \/\ \ /' _ `\/\ \ \ \ \ \ \  _ <' 
 \ \ \_/\ \ \ \/\ \/\ \ \ \ \ \_\ \ \ \L\ \
  \ \_\\ \_\ \_\ \_\ \_\ \_\ \____/\ \____/
   \/_/ \/_/\/_/\/_/\/_/\/_/\/___/  \/___/ 
)";

#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_CYAN "\033[36m"
#define COLOR_YELLOW "\033[33m"

static void print_header_line(char ch = '=') {
}

static void print_startup_info(const Config& cfg) {
	std::cerr << std::string(50, '=') << std::endl;
	std::cerr << BANNER << std::endl;
	std::cerr << std::string(50, '=') << std::endl;

	std::cout << std::left;
	std::cout << COLOR_YELLOW << std::setw(25) << "Data Directory:" << COLOR_RESET << cfg.datadir
			  << "\n";
	std::cout << COLOR_YELLOW << std::setw(25) << "Catalog Path:" << COLOR_RESET << cfg.catalog_path
			  << "\n";
	std::cout << COLOR_YELLOW << std::setw(25) << "Buffer Pool Size:" << COLOR_RESET
			  << cfg.buffer_manager_pool_size << "\n";

#ifdef ENABLE_THREADPOOL
	std::cout << COLOR_YELLOW << std::setw(25) << "Mode:" << COLOR_RESET
			  << "Thread Pool (network server)\n";
	std::cout << COLOR_YELLOW << std::setw(25) << "Number of Threads:" << COLOR_RESET
			  << cfg.nthreads << "\n";
	std::cout << COLOR_YELLOW << std::setw(25) << "Listening on Port:" << COLOR_RESET << cfg.port
			  << "\n";
#else
	std::cout << COLOR_YELLOW << std::setw(25) << "Mode:" << COLOR_RESET
			  << "Single-process stdin/stdout\n";
#endif

	std::cerr << std::string(50, '=') << std::endl;
}
int main(int argc, char** argv) {
	std::signal(SIGINT, signal_handler);

	Config* config = Config::get_instance();
	try {
		config->get(argc, argv);
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}
	print_startup_info(*config);

	DataBase db;

	db.start();
	db.stop();

	exit(EXIT_FAILURE);
}
