#pragma once

#include "common.h"

enum WINDOW_POSITION {
  CENTER = 0,
  DOWN
};


class Dialog
{
public:
  std::string m_title;
  WINDOW* m_window;
  WINDOW* m_parent;
  WINDOW_POSITION m_position;
  int m_y = 0;
  int m_x = 0;

  Dialog(std::string title, int y, int x, WINDOW* parent, WINDOW_POSITION position)
  {
    this->m_title = title;
    this->m_y = y;
    this->m_x = x;
    this->m_parent = parent;
    this->m_position = position;
    this->init();
  }

  ~Dialog()
  {
    wrefresh(this->m_window);
    delwin(this->m_window);
  }

  void init()
  {
    switch (this->m_position)
    {
      case CENTER:
        this->m_window = newwin(this->m_y, this->m_x, getmaxy(this->m_parent) / 2 - this->m_y / 2, getmaxx(this->m_parent) / 2 - this->m_x / 2);
        break;

      case DOWN:
        this->m_window = newwin(getmaxy(this->m_parent) / 2 - 1, getmaxx(this->m_parent), getmaxy(this->m_parent) / 2, 0);
        break;
    }
    
    box(this->m_window, 0, 0);
    mvwprintw(this->m_window, 0, getmaxx(this->m_window) / 2 - this->m_title.size() / 2, this->m_title.c_str());

    if(this->m_position == DOWN)
    {
      clrtobot();
      return;
    }

    wrefresh(this->m_window);
    refresh();
  }

};

class DialogList : public Dialog
{
public:

  std::vector<std::string> m_list = {};
  
  DialogList(std::string title, int y, int x, WINDOW* parent, WINDOW_POSITION position, std::vector<std::string> list) :  
    Dialog(title, y, x, parent, position)
  {
    this->m_list = list;
  }
  
  void display_list()
  {
    for(unsigned int i = 0; i < this->m_list.size(); i++)
    {
      mvwprintw(this->m_window, i + 1, 1, this->m_list[i].c_str());
    }

    wrefresh(this->m_window);
    refresh;
  }

};

class DialogBox : public Dialog
{
public:
  std::string m_str;
  WINDOW* m_input_window;

  DialogBox(std::string title, int y, int x, WINDOW* parent, WINDOW_POSITION position) :  
    Dialog(title, y, x, parent, position) {
  }

  std::string get_input()
  {
    // input
    this->m_input_window = newwin(getmaxy(this->m_window) - 4, getmaxx(this->m_window) - 4, getbegy(this->m_window) + 1, getbegx(this->m_window) + 2);
    wrefresh(this->m_input_window);
    refresh();

    char str[9999];
    echo();
    mvwgetstr(this->m_input_window, 1, 1, str);
    noecho();

    return str;
  }
};