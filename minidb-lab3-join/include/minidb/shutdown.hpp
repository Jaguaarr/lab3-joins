#ifndef __SHUTDOWN_HPP__
#define __SHUTDOWN_HPP__

#include <atomic>

/**
 * @brief Global atomic flag set to true when a shutdown is requested.
 *
 * Other threads should monitor this flag to initiate cleanup and
 * terminate operations safely.
 */
extern std::atomic<bool> shutdown_requested;

#endif
