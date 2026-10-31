#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <exception>
#include <string>

#define NTHREADS 1
#define PORT 5000

#define CATALOG_FILE_NAME "catalog.json"

class ConfigParseException : public std::exception {
  private:
	std::string message;

  public:
	ConfigParseException(const std::string& msg) : message(msg) {}
	const char* what() const noexcept override { return message.c_str(); }
};

/**
 * @class Config
 * @brief Singleton class for managing application configuration.
 *
 * Parses and stores configuration parameters such as:
 * - Data directory path
 * - Number of threads
 * - Server port
 * - Catalog file path
 * - Buffer manager pool size
 *
 * Provides a static get_instance() method to access the singleton.
 * Parses command line arguments via get().
 * Supports streaming operator for printing the current configuration.
 *
 * Copy construction is disabled to enforce singleton pattern.
 */
class Config {
  public:
	std::string	   datadir;
	int			   nthreads;
	unsigned short port;
	std::string	   catalog_path;
	std::string	   index_dir; // directory name for indices
	int			   buffer_manager_pool_size;

	Config(const Config&) = delete;
	static Config* get_instance();

	void get(int argc, char** argv);

	friend std::ostream& operator<<(std::ostream& o, const Config& config);

  private:
	~Config() = default;
	Config();

	void set();

	static Config* instance;
};

std::ostream& operator<<(std::ostream& o, const Config& config);

#endif
