#include "common.h"
#include "torrent_list.h"

/* the fearsome TO-DO list
	
	[ ] find a nicer way of handling options (their display and input handling)
	[ ] add torrent backend 
	[ ] show list of trackers/seeders/progress per file
	[ ] add log window
	[ ] select save location

	done:
	[x] finish ui skeleton
	[x] upload to gh

*/

int main()
{
	initscr();
	noecho();
	keypad(stdscr, TRUE);
	refresh();
	curs_set(0);
	/* BEGIN */

	WINDOW* torrent_window = newwin(getmaxy(stdscr), getmaxx(stdscr), 0, 0);
	TorrentList torrent_list(torrent_window);

	// colors
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);

	// window bg
	bkgd(COLOR_PAIR(1));

	torrent_list.display();
	while(char c = getch())
	{
		torrent_list.update();
		if(torrent_list.handleInput(c) == 1)
			break;
		torrent_list.display();
		
	}

	/* END */
	endwin();
	return 0;
}

