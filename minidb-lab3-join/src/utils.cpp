#include "minidb/utils.hpp"

#include <filesystem>
#include <sys/stat.h>

bool dir_exists(const std::string& path) {
	struct stat info;
	int			statrc;

	if (path.empty()) {
		return (false);
	}
	statrc = stat(path.c_str(), &info);
	if (statrc != 0) {
		return (false);
	}
	return (info.st_mode & S_IFDIR) ? true : false;
}

bool file_exists(const std::string& path) {
	return std::filesystem::exists(path);
}
