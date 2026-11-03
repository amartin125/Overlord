#pragma once
#include <string>

// TEMPORARY testing aid; see docs/vr-memory-log.md for how to remove it.
namespace memory_log
{
	// Appends a timestamped line unless it repeats the previous note.
	void note(const std::string& line);
}
