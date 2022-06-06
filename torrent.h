#pragma once
#include "common.h"

class Torrent
{
public:
	unsigned int m_id;
	std::string m_name;
	unsigned int m_size;
	float m_progress;
	std::string m_status;
	unsigned int m_speed;
	unsigned int m_seeds;
	std::vector<std::string> m_files;
	std::string m_path;
  void* m_handle;
	bool m_selected;
	float m_ratio;
}; 