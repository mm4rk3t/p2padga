#pragma once
#include "common.h"

class File
{
public:
  std::string m_name = "";
  int64_t m_size = 0;
  bool m_download = true;

  void toggle_download()
  {
    this->m_download = !this->m_download;
  }
};