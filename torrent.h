#pragma once
#include "common.h"

class Torrent
{
public:

	Torrent(lt::torrent_handle* handle) : m_handle(handle){}

  lt::torrent_handle* m_handle;
	bool m_selected = false;
	std::vector<std::string> m_files = {};
	float m_ratio = 0;
	std::map<std::string, std::string> m_strings;

	// fetched data
	lt::queue_position_t m_id = 0;
	std::string m_name = "";
	int64_t m_size = 0;
	int m_speed = 0;
	int m_seeds = 0;
	int64_t m_downloaded = 0;
	float m_progress = 0;
	std::string m_path = "";
	int m_state = 0;

	void fetch_data()
	{
		if(this->m_handle->is_valid())
		{
			lt::torrent_status ts = this->m_handle->status();
			
			this->m_id = ts.queue_position;
			this->m_name = ts.name;
			this->m_size = ts.total;
			this->m_speed = ts.download_rate;
			this->m_seeds = ts.num_seeds;
			this->m_downloaded = ts.total_done;
			this->m_progress = ts.progress;
			this->m_path = ts.save_path;
			this->m_state = ts.state;
		}
	}

	std::string parse_state(int state)
	{
		switch (state)
		{
			case 1:
				return "checking   ";
			
			case 2:
				return "metadata   ";
			
			case 3:
				return "downloading";
	
			case 4:
				return "finished   ";

			case 5:
				return "seeding    ";

			case 6:
				return "peanuts    ";

			case 7:
				return "checking   ";
	
			default:
				return "???        ";
		}
	}

	void insert_string(std::string key, std::string value)
	{
		this->m_strings[key] = value;
	}

	const char* get_string(std::string key)
	{
		return this->m_strings[key].c_str();
	}

	void update_strings(unsigned int padding)
	{
		std::stringstream ss;
		std::string temp;

		this->insert_string("ID", std::to_string(this->m_id));
		
		if(this->m_name.size() >= padding) 
		{
			std::string temp_name = this->m_name;
			temp_name.resize(padding - 4);
			temp_name.append("...");
			this->insert_string("Name", temp_name);
		} else {
			this->insert_string("Name", this->m_name);
		}
		
		ss << std::fixed << std::setprecision(2) << this->m_size / (1024 * 1024 * 1024) << "GB";
    temp = ss.str();
		this->insert_string("Size", temp);
		ss.str("");
		temp = "";

		ss << std::fixed << std::setprecision(2) << this->m_progress * 100 << "%%";
    temp = ss.str();
		this->insert_string("Progress", temp);
		ss.str("");
		temp = "";

		this->insert_string("Status", this->parse_state(this->m_state));

		ss << std::fixed << std::setprecision(2) << this->m_speed / (1024 * 1024) << "MB/s";
    temp = ss.str();
		this->insert_string("Speed", temp);
		ss.str("");
		temp = "";

		this->insert_string("Seeds", std::to_string(this->m_seeds));
		this->insert_string("Ratio", std::to_string(0));
	}
};