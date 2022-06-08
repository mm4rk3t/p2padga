#include "common.h"
#include "torrent_list.h"

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