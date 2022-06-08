#include "common.h"
#include "torrent_list.h"

/* bugs / To-Do
	
	* IMPORTANT

	* EASY
	[ ] limit str size and add ...
	[ ] clear lines to avoid things like "finisheding"

	* OTHER
	[ ] show list of trackers, seeders and progress per file
	[ ] add log window
	[ ] add alerts
	[ ] adjust size of fields (for example: make ID 5 and Name 15)
	[ ] adjust magnitude of size and velocity (not only GB but KB, MB, B and TB too)
	[ ] choose not to download some files
	[ ] change priority
	[ ] create File object (similar to Torrent object)
	
	done:
	[x] finish ui skeleton
	[x] upload to gh
	[x] add torrent backend
	[x] fix bug when adding magnet files
	[x] weird output @ the beginning
	[x] decouple output string from values
	[x] program state logic
	[x] can't output more than a single torrent at the same time  (can download more just fine)
	[x] check if speed is correct
	[x] select save location
	[x] add file viewer
	[x] Solve files bug when adding magnet links
	[x] add more torrent functionality
	
*/


int main()
{
	lt::session torrent_session;

	initscr();
	noecho();
	keypad(stdscr, TRUE);
	refresh();
	curs_set(0);
	

	WINDOW* torrent_window = newwin(getmaxy(stdscr), getmaxx(stdscr), 0, 0);
	nodelay(torrent_window, true);

	TorrentList torrent_list(torrent_window, &torrent_session);

	// colors
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);

	// window bg
	bkgd(COLOR_PAIR(1));

	torrent_list.display();
	while(true)
	{
		char c = wgetch(torrent_window);

		if(torrent_list.m_window_active)
			if(torrent_list.handleInput(c) == 1)
				break;

		torrent_list.update();
		torrent_list.display();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	endwin();

	return 0;
}