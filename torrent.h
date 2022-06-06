#pragma once
#include "common.h"

class Torrent
{
public:

	Torrent(lt::torrent_handle* handle) : m_handle(handle){}

  lt::torrent_handle* m_handle;

	bool m_selected = false;
	uint32_t m_id = 0;
	unsigned int m_size = 0;
	unsigned int m_speed = 0;
	unsigned int m_seeds = 0;
	std::vector<std::string> m_files = {};
	std::string m_name = "";
	std::string m_path = "";
	std::string m_status = "";
	float m_progress = 0;
	float m_ratio = 0;

	void fetch_data()
	{
		this->m_name = this->m_handle->get_torrent_info().name();
	}
}; 