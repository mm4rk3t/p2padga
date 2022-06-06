#include "common.h"
#include "torrent_list.h"

/* the fearsome TO-DO list
	
	[ ] show list of trackers/seeders/progress per file
	[ ] add log window
	[ ] select save location
	[ ] fix bug when adding magnet files

	done:
	[x] finish ui skeleton
	[x] upload to gh
	[x] add torrent backen
*/

int main()
{
	lt::session torrent_session;

	initscr();
	noecho();
	keypad(stdscr, TRUE);
	refresh();
	curs_set(0);
	halfdelay(2);

	WINDOW* torrent_window = newwin(getmaxy(stdscr), getmaxx(stdscr), 0, 0);
	TorrentList torrent_list(torrent_window, &torrent_session);

	// colors
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);

	// window bg
	bkgd(COLOR_PAIR(1));

	torrent_list.display();
	while(char c = getch())
	{
		if(torrent_list.m_window_active)
			if(torrent_list.handleInput(c) == 1)
				break;
		torrent_list.update();
		torrent_list.display();
	}

	endwin();

	return 0;
}

