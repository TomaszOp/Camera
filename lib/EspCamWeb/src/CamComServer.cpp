/**
 * @file CamComServer.cpp
 * @brief CamComServer implementation: non-blocking line editor on a serial stream.
 */
#include "CamComServer.h"

#if COM_SERVER_ON

namespace espcamweb {

static const unsigned MAX_LINE = 128;  ///< Longest accepted command line; extra characters are dropped.

void CamComServer::begin(const ComServerSettings &settings) {
  s_ = settings;
  io_.print("\r\nCOM command server ready - type help\r\n");
  prompt();
}

void CamComServer::prompt() {
  if (s_.prompt) io_.print(s_.prompt);
}

void CamComServer::handle() {
  while (io_.available()) {
    char c = io_.read();
    if (c == '\r' || c == '\n') {
      bool crlf = c == '\n' && lastWasCr_;
      lastWasCr_ = c == '\r';
      if (crlf) continue;  // second half of CR LF
      io_.print("\r\n");
      commands_.execute(line_, io_);  // quit/exit does not apply to a serial port
      line_ = "";
      prompt();
      continue;
    }
    lastWasCr_ = false;
    if (c == '\b' || c == 127) {
      if (line_.length()) {
        line_.remove(line_.length() - 1);
        if (s_.echo) io_.print("\b \b");
      }
    } else if (c >= ' ' && line_.length() < MAX_LINE) {
      line_ += c;
      if (s_.echo) io_.write(c);
    }
  }
}

}  // namespace espcamweb

#endif  // COM_SERVER_ON
