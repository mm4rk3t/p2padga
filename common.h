#pragma once
#include <thread>
#include <chrono>
#include <fstream>
#include <csignal>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/torrent_status.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/download_priority.hpp>

#include <curses.h>

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

	std::string handle_units(float n)
	{

		float new_number = 0.0f;
		std::string unit = "";
		std::stringstream ss;

		if (n < 1024) // bytes
		{
			new_number = n;
			unit = "B";
		}

		else if (n < (1024 * 1024)) // kilobytes
		{
			new_number = n / (1024);
			unit = "KB";
		}
		
		else if (n < (1024 * 1024 * 1024)) // megabytes
		{
			new_number = n / (1024 * 1024);
			unit = "MB";
		}

		else // gigabytes
		{
			new_number = n / (1024 * 1024 * 1024);
			unit = "GB";
		}

		ss << std::fixed << std::setprecision(2) << new_number << unit;
		return ss.str();

	}