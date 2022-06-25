#pragma once
#include "common.h"
#include "file.h"

class Torrent
{
public:
	Torrent(lt::torrent_handle handle)
	{
		m_handle = handle;
	}

  lt::torrent_handle m_handle;
	bool m_selected = false;
	float m_ratio = 0;
	std::map<std::string, std::string> m_strings;

	std::shared_ptr<const lt::torrent_info> m_info = nullptr;
	lt::torrent_status m_status;

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
	unsigned int m_num_files = 0;
	std::vector<File> m_files;

	void fetch_data()
	{
		if(this->m_handle.is_valid())
		{
			this->m_status = this->m_handle.status();
	
			this->m_info = this->m_handle.torrent_file();
			
			this->m_id = this->m_status.queue_position;
			this->m_name = this->m_status.name;
			this->m_size = this->m_status.total;
			this->m_speed = this->m_status.download_rate;
			this->m_seeds = this->m_status.num_seeds;
			this->m_downloaded = this->m_status.total_done;
			this->m_progress = this->m_status.progress;
			this->m_path = this->m_status.save_path;
			this->m_state = this->m_status.state;

			if(this->m_info != nullptr && this->m_info->is_valid())
			{
				const lt::file_storage& files = this->m_info->files();
				
				this->m_num_files = files.num_files();
				
				for(unsigned int i = 0; i < this->m_num_files; i++)
				{
					if(this->m_files.size() < this->m_num_files)
					{
						this->m_files.push_back(File());
					}
						
					this->m_files[i].m_name = std::string(files.file_name(i));
					this->m_files[i].m_size = files.file_size(i);
				}
			}
		}
	}

	void pause()
	{
		this->m_handle.pause();
		this->m_handle.unset_flags(lt::torrent_flags::auto_managed);
	}

	void resume()
	{
		this->m_handle.resume();
		this->m_handle.set_flags(lt::torrent_flags::auto_managed);
	}

	void move_top()
	{
		m_handle.queue_position_top();
	}

	void move_up()
	{
		this->m_handle.queue_position_up();
	}

	void move_down()
	{
		this->m_handle.queue_position_down();
	}

	void move_bottom()
	{
		this->m_handle.queue_position_bottom();
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
		
		this->insert_string("Size", handle_units(this->m_size));

		ss << std::fixed << std::setprecision(2) << this->m_progress * 100 << "%%";
    temp = ss.str();
		this->insert_string("Progress", temp);

		this->insert_string("Status", parse_state(this->m_state));
		this->insert_string("Speed", handle_units(this->m_speed).append("/s"));

		this->insert_string("Seeds", std::to_string(this->m_seeds));
		this->insert_string("Ratio", std::to_string(0));
	}

	void update()
	{
		for(unsigned int i = 0; i < this->m_num_files; i++)
		{
			if(this->m_files[i].m_download)
				this->m_handle.file_priority(i, 4);
			else
				this->m_handle.file_priority(i, 0);
		}
	}

};