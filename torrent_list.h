#pragma once
#include "common.h"
#include "torrent.h"
#include "dialog.h"

class TorrentList
{
public:
	
  std::vector<std::unique_ptr<Torrent>> m_torrents;
  std::vector<std::string> m_fields{"ID", "Name", "Status", "Progress", "Size", "Speed", "Seeds", "Ratio"};
  std::vector<std::string> m_options{"(a)dd", "(f)ilter", "(s)ort", "f(i)les", "(l)og", "(q)uit"};
  std::vector<std::string> m_torrent_options{"(r)esume", "(p)ause", "pri(o)rity", "(d)elete"};
  unsigned int m_selected = 0;
  unsigned int m_selected_file = 0;
  WINDOW* m_window;
  unsigned int m_window_y = getmaxy(this->m_window);
  unsigned int m_window_x = getmaxx(this->m_window);
  bool m_window_active = true;
  unsigned int m_padding = 0;

  lt::session* m_torrent_session;

  TorrentList(WINDOW* torrent_window, lt::session* torrent_session) : m_window(torrent_window), m_torrent_session(torrent_session)
  {
    keypad(this->m_window, true);
  }

  void display()
  {
    // display fields
    clear();
    wrefresh(this->m_window);
    refresh();

    wmove(this->m_window, 0, 0);
    wclrtoeol(this->m_window);
    this->m_padding = getmaxx(this->m_window) / this->m_fields.size();
    unsigned int start_x = 0;
    for(unsigned int i = 0; i < this->m_fields.size(); i++)
    {
      mvwprintw(this->m_window, 0, start_x, this->m_fields[i].c_str());
      start_x += m_padding;
    }
    mvwchgat(this->m_window, 0, 0, getmaxx(this->m_window), A_STANDOUT, 1, NULL);
    wrefresh(this->m_window);
    refresh();

    // display torrents
    if(this->m_torrents.size() > 0)
    {
      for(unsigned int j = 0; j < this->m_torrents.size(); j++)
      {
        wmove(this->m_window, j+1, 0);
        wclrtoeol(this->m_window);
        for(unsigned int i = 0; i < this->m_fields.size(); i++)
          mvwprintw(this->m_window, j + 1, this->m_padding * i, this->m_torrents[j]->get_string(this->m_fields[i]));

        if(this->m_selected == j)
          mvwchgat(this->m_window, j + 1, 0, getmaxx(this->m_window), A_NORMAL, 2, NULL);
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

  void display_log()
  {
    this->m_window_active = false;
    this->m_window_active = true;
  }

  void init()
  {
    // set everything up
  }

  void update()
  {
    for(const auto& t : this->m_torrents)
    {
      t->fetch_data();
      t->update();
      t->update_strings(this->m_padding);
    }
    this->m_window_y = getmaxy(this->m_window);
    this->m_window_x = getmaxx(this->m_window);
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
    
    DialogBox add_dialog("[ ADD TORRENT ]", 10, 50, this->m_window, CENTER);

    // options
    std::vector<std::string> add_torrent_options{"(m)agnet", "(t)orrent"};
    int start_x = 1;
    for(unsigned int i = 0; i < add_torrent_options.size(); i++)
    {
      mvwprintw(add_dialog.m_window, getmaxy(add_dialog.m_window) - 2, start_x, add_torrent_options[i].c_str());
      start_x += getmaxx(add_dialog.m_window) / add_torrent_options.size();
    }
    wrefresh(add_dialog.m_window);
    refresh();
    
    // input
    char c = wgetch(add_dialog.m_window);
    switch(c)
    {
      case 'm':
        mvwchgat(add_dialog.m_window, getmaxy(add_dialog.m_window) - 2, 1, getmaxx(add_dialog.m_window) / 2 -1, A_STANDOUT, 1, NULL);
        wrefresh(add_dialog.m_window);
        refresh();
        this->add_magnet(std::string(add_dialog.get_input()));
        break;

      case 't':
        mvwchgat(add_dialog.m_window, getmaxy(add_dialog.m_window) - 2, getmaxx(add_dialog.m_window) / 2 + 1, getmaxx(add_dialog.m_window) / 2 - 2, A_STANDOUT, 1, NULL);
        wrefresh(add_dialog.m_window);
        refresh();
        this->add_torrent(std::string(add_dialog.get_input()));
        break;

      default:
        break;
    }
    this->m_window_active = true;
  }

  std::string get_save_path()
  {
    this->m_window_active = false;
    DialogBox save_dialog("[ SAVE PATH ]", 10, 50, this->m_window, CENTER);
    std::string str = save_dialog.get_input();
    this->m_window_active = true;
    return str;
  }

  void add_magnet(std::string magnet_link)
  {
    if(magnet_link.empty())
      return;
    std::string str = "Parsing magnet uri: "  + magnet_link;
    mvwprintw(this->m_window, this->m_window_y - 2, 0, str.c_str());

    lt::add_torrent_params atp = lt::parse_magnet_uri(magnet_link);
    atp.save_path = this->get_save_path();
	  lt::torrent_handle h = this->m_torrent_session->add_torrent(atp);
  
    std::unique_ptr<Torrent> t(new Torrent(h));
    this->m_torrents.push_back(std::move(t));
  }

  void add_torrent(std::string torrent_file)
  {
    if(torrent_file.empty())
      return;
    std::string str = "Parsing torrent file: "  + torrent_file;
    mvwprintw(this->m_window, this->m_window_y - 2, 0, str.c_str());

    lt::add_torrent_params atp;
    atp.save_path = this->get_save_path();
    atp.ti = std::make_shared<lt::torrent_info>(torrent_file);
    lt::torrent_handle h = this->m_torrent_session->add_torrent(atp);

    std::unique_ptr<Torrent> t(new Torrent(h));
    this->m_torrents.push_back(std::move(t));

  }

  void change_priority()
  {
    this->m_window_active = false;
  
    std::vector<std::string> options = {"(1) Top", "(2) Up", "(3) Down", "(4) Bottom"};

    DialogList priority_dialog("[ Priority ]", 6, 50, this->m_window, CENTER, options);
    priority_dialog.display_list();

    char c = wgetch(priority_dialog.m_window);
    switch(c)
    {
      case '1':
        this->m_torrents[this->m_selected]->move_top();
        break;

      case '2':
        this->m_torrents[this->m_selected]->move_up();
        break;

      case '3':
        this->m_torrents[this->m_selected]->move_down();
        break;
      
      case '4':
        this->m_torrents[this->m_selected]->move_bottom();
        break;

      default:
        break;
    }
    
    this->m_window_active = true;
  }

  void torrent_info()
  {
    if(this->m_torrents.size() < 1)
      return;

    std::vector<std::string> files_str;

    for(unsigned int i = 0; i < this->m_torrents[this->m_selected]->m_num_files; i++)
    {
      std::string check = this->m_torrents[this->m_selected]->m_files[i].m_download ? "[x] " : "[ ] ";
      check.append(handle_units(this->m_torrents[this->m_selected]->m_files[i].m_size));
      check.append(" ");
      check.append(this->m_torrents[this->m_selected]->m_files[i].m_name);
      files_str.push_back(check);
    }
     
    DialogList files_dialog(this->m_torrents[this->m_selected]->m_name, 0, 0, this->m_window, DOWN, files_str);
    files_dialog.display_list();

    // print options
    int start_x = 0;
    for(unsigned int i = 0; i < this->m_torrent_options.size(); i++)
    {
      mvprintw(getmaxy(stdscr) - 1, start_x, this->m_torrent_options[i].c_str());
      start_x += getmaxx(files_dialog.m_window) / this->m_torrent_options.size();
    }
    mvchgat(getmaxy(stdscr) - 1, 0, getmaxx(files_dialog.m_window), A_STANDOUT, 1, NULL);

    wrefresh(files_dialog.m_window);
    refresh();

    // handle input
    char c = wgetch(files_dialog.m_window);
    switch(c)
    {
      case 'r':
        this->m_torrents[this->m_selected]->resume();
        break;

      case 'p':
        this->m_torrents[this->m_selected]->pause();
        break;

      case 'o':
        this->change_priority();
        break;
      case 'd':
        this->m_torrent_session->remove_torrent(m_torrents[this->m_selected]->m_handle);
        this->m_torrents.erase(this->m_torrents.begin() + this->m_selected);
        this->m_selected = 0;
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

  void handle_resize()
  {
    clear();
    refresh();
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
          this->torrent_info();
          break;
        
        case 'l':
          this->display_log();
          break;

        case KEY_RESIZE:
          this->handle_resize();
          break;

        default:
          break;
	  	}
    }
    return 0;
  }
};