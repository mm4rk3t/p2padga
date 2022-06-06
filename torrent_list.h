#pragma once
#include "common.h"
#include "torrent.h"

class TorrentList
{
public:
	std::vector<Torrent*> m_torrents;
  std::vector<std::string> m_fields{"ID", "Name", "Size", "Progress", "Status", "Speed", "Seeds", "Ratio"};
  std::vector<std::string> m_options{"(a)dd", "(f)ilter", "(s)ort", "f(i)les", "(q)uit"};
  std::vector<std::string> m_torrent_options{"(c)ontinue", "(p)ause", "p(r)iority", "(d)elete", "(b)ack"};
  unsigned int m_selected = 0;
  unsigned int m_selected_file = 0;
  WINDOW* m_window;
  unsigned int m_window_y = getmaxy(this->m_window);
  unsigned int m_window_x = getmaxx(this->m_window);
  bool m_window_active = true;

  lt::session* m_torrent_session;

  TorrentList(WINDOW* torrent_window, lt::session* torrent_session) : m_window(torrent_window), m_torrent_session(torrent_session)
  {
    keypad(this->m_window, true);
  }

  void display()
  {
    // display fields
    clear();
    refresh();
    wrefresh(this->m_window);

    int padding = getmaxx(this->m_window) / this->m_fields.size();
    unsigned int start_x = 0;
    for(unsigned int i = 0; i < this->m_fields.size(); i++)
    {
      mvwprintw(this->m_window, 0, start_x, this->m_fields[i].c_str());
      start_x += padding;
    }
    mvwchgat(this->m_window, 0, 0, getmaxx(this->m_window), A_STANDOUT, 1, NULL);

    // display list of torrents
    for (unsigned int i = 0; i < this->m_torrents.size(); i++)
    {
      mvwprintw(this->m_window, i + 1, padding * 0, std::to_string(this->m_torrents[i]->m_id).c_str());
      mvwprintw(this->m_window, i + 1, padding * 1, this->m_torrents[i]->m_name.c_str());
      mvwprintw(this->m_window, i + 1, padding * 2, std::to_string(this->m_torrents[i]->m_size).c_str());
      
      // we need to to this to print floats with 2 point precision
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << this->m_torrents[i]->m_progress;
      std::string temp = ss.str();
      mvwprintw(this->m_window, i + 1, padding * 3, temp.c_str());
      ss.str("");

      mvwprintw(this->m_window, i + 1, padding * 4, this->m_torrents[i]->m_status.c_str());
      mvwprintw(this->m_window, i + 1, padding * 5, std::to_string(this->m_torrents[i]->m_speed).c_str());
      mvwprintw(this->m_window, i + 1, padding * 6, std::to_string(this->m_torrents[i]->m_seeds).c_str());
      
      // again here
      ss << std::fixed << std::setprecision(2) << this->m_torrents[i]->m_ratio;
      temp = ss.str();
      mvwprintw(this->m_window, i + 1, padding * 7, temp.c_str());


      if(this->m_selected == i)
      {
        mvwchgat(this->m_window, i + 1, 0, getmaxx(this->m_window), A_NORMAL, 2, NULL);
      }

    }

    // display options
    start_x = 0;
    for(unsigned int i = 0; i < this->m_options.size(); i++)
    {
      mvprintw(getmaxy(stdscr) - 1, start_x, this->m_options[i].c_str());

      start_x += getmaxx(stdscr) / this->m_options.size();
    }
    mvchgat(getmaxy(stdscr) - 1, 0, getmaxx(stdscr), A_STANDOUT, 1, NULL);
    
    wrefresh(this->m_window);
    refresh();
  }

  void init()
  {
    // set everything up
  }

  void update()
  {
    for(Torrent* t : this->m_torrents)
      t->fetch_data();
  }

  void select_next()
  {
    this->m_selected++;
    if(m_selected >= this->m_torrents.size())
      this->m_selected = 0;
  }

  void select_previous()
  {
    this->m_selected--;
    if(m_selected > this->m_torrents.size())
      this->m_selected = this->m_torrents.size() - 1;
  }

  void add()
  {
    this->m_window_active = false;
    
    // make window appear
    int size_y = 10;
    int size_x = 50;
    WINDOW* add_torrent_window = newwin(size_y, size_x, this->m_window_y / 2 - size_y / 2, this->m_window_x / 2 - size_x / 2);
    box(add_torrent_window, 0, 0);
    std::string text = "[ ADD TORRENT ]";
    mvwprintw(add_torrent_window, 0, size_x / 2 - text.size() / 2, text.c_str());
    
    // options
    std::vector<std::string> add_torrent_options{"(m)agnet", "(t)orrent"};
    int start_x = 1;
    for(unsigned int i = 0; i < add_torrent_options.size(); i++)
    {
      mvwprintw(add_torrent_window, getmaxy(add_torrent_window) - 2, start_x, add_torrent_options[i].c_str());
      start_x += getmaxx(add_torrent_window) / add_torrent_options.size();
    }
    wrefresh(add_torrent_window);
    refresh();

    // input
    WINDOW* input_window = newwin(size_y-4, size_x-4, getbegy(add_torrent_window) + 1, getbegx(add_torrent_window) + 2);
    char str[9999];
    char c = wgetch(add_torrent_window);
    echo();

    switch(c)
    {
      case 'm':
        mvwchgat(add_torrent_window, getmaxy(add_torrent_window) - 2, 1, getmaxx(add_torrent_window) / 2 -1, A_STANDOUT, 1, NULL);
        wrefresh(add_torrent_window);
        refresh();
        mvwgetstr(input_window, 1, 1, str);
        this->add_magnet(std::string(str));
        break;

      case 't':
        mvwchgat(add_torrent_window, getmaxy(add_torrent_window) - 2, size_x / 2 + 1, getmaxx(add_torrent_window) / 2 - 2, A_STANDOUT, 1, NULL);
        wrefresh(add_torrent_window);
        refresh();
        mvwgetstr(input_window, 1, 1, str);
        this->add_torrent(std::string(str));
        break;

      default:
        break;
    }

    noecho();
    this->m_window_active = true;
  }

  void add_magnet(std::string magnet_link)
  {
    if(magnet_link.empty())
      return;
    std::string str = "Parsing magnet uri: "  + magnet_link;
    mvwprintw(this->m_window, this->m_window_y - 2, 0, str.c_str());

    // torrent logic
    lt::add_torrent_params atp = lt::parse_magnet_uri(magnet_link);
    atp.save_path = "./dl-test";
	  lt::torrent_handle h = this->m_torrent_session->add_torrent(atp);

    // add torrent to list using handle
    Torrent* t = new Torrent(&h);
  
    this->m_torrents.push_back(t); // SEGFAULT HERE
    /*
      Apparently, constructing vectors of raw pointers is a a very bad idea
      so I should probably just use objects instead and leave that alone.

      Don't do it now, do it tomorrow, or any other day.

      I don't know why this doesn't happen with .torrent.
    */
  }

  void add_torrent(std::string torrent_file)
  {
    if(torrent_file.empty())
      return;
    std::string str = "Parsing torrent file: "  + torrent_file;
    mvwprintw(this->m_window, this->m_window_y - 2, 0, str.c_str());

    // torrent logic
    lt::add_torrent_params atp;
    atp.save_path = "./dl-test";
    atp.ti = std::make_shared<lt::torrent_info>(torrent_file);
    lt::torrent_handle h = this->m_torrent_session->add_torrent(atp);

    // add torrent to list using handle
    Torrent* t = new Torrent(&h);
    this->m_torrents.push_back(t);

  }

  void select_torrent()
  {
    this->m_window_active = false;
    WINDOW* show_files_window = newwin(this->m_window_y/2 - 1, this->m_window_x, this->m_window_y/2, 0);
    box(show_files_window, 0, 0);
    mvwprintw(show_files_window, 0, 5, this->m_torrents[this->m_selected]->m_name.c_str()); 
    mvwprintw(show_files_window, 0, 5 + this->m_torrents[this->m_selected]->m_name.size() + 1, this->m_torrents[this->m_selected]->m_path.c_str());
    clrtobot();

    // print files
    for(unsigned int i = 0; i < this->m_torrents[this->m_selected]->m_files.size(); i++)
    {
      mvwprintw(show_files_window, i + 1, 1, this->m_torrents[this->m_selected]->m_files[i].c_str());
      
      // turn on again when figuring out how to select files
      // if(i == this->m_selected_file)
      //      mvwchgat(show_files_window, i + 1, 1, getmaxx(show_files_window) - 2, A_STANDOUT, 1, NULL);
    }

    // print options
    int start_x = 0;
    for(unsigned int i = 0; i < this->m_torrent_options.size(); i++)
    {
      mvprintw(getmaxy(stdscr) - 1, start_x, this->m_torrent_options[i].c_str());
      start_x += getmaxx(show_files_window) / this->m_torrent_options.size();
    }
    mvchgat(getmaxy(stdscr) - 1, 0, getmaxx(show_files_window), A_STANDOUT, 1, NULL);

    wrefresh(show_files_window);
    refresh();

    // handle input
    char c = wgetch(show_files_window);
    switch(c)
    {
      case 'c':
        break;

      case 'p':
        break;

      case 'r':
        break;

      case 'd':
        break;

      case 'b':
        break;

      default:
        break;
    }
    this->m_window_active = true;
  }

  void filter()
  {
    // to-do
  }

  void sort()
  {
    // to-do
  }

  int handleInput(int c)
  { 
    if(this->m_window_active)
    {
      switch (c)
		  {
        case 'a':
          this->add();
          break;

        case 'f':
          this->filter();
          break;

        case 's':
          this->sort();
          break;

        case 'q':
          return 1;
          break;
        
        case 'j':
          this->select_next();
          break;
        
        case 'k':
          this->select_previous();
          break;

        case 'i':
          this->select_torrent();
          break;
        
        default:
          break;
	  	}
    }
    return 0;
  }
};