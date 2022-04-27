/*#############################################################################
#
#            Notice of Copyright and Non-Disclosure Information
#
#
###############################################################################
*/

#ifndef COMMON_H
#define COMMON_H

#include <sys/time.h>
#include <sys/syscall.h>

#include <unistd.h>

#include <string>


//#############################################################################
#define RANDOM_CHAR_SIZE                         9



//#############################################################################
inline pid_t get_tid()
  {
  static __thread pid_t tid = 0;
  if (tid == 0)
    {
    tid = (pid_t)syscall(__NR_gettid);
    }

  return tid;
  }

//#############################################################################
inline std::string generate_now_string()
  {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  char tm_buf[16] = {0};
  if (strftime(tm_buf, 9, "%T", localtime(&tv.tv_sec)) == 0)
    {
    return "wrong time ";
    }

  sprintf(tm_buf+8, ":%06ld", tv.tv_usec);

  return tm_buf;
  }

//#############################################################################
// printf with thread id, time and line feed
template <typename... ARGS>
inline void tprint(const std::string& fmt, const ARGS&... args)
  {
  printf(std::string("[%05d] %s " + fmt + "\n").data(),
    get_tid(), generate_now_string().data(), args...);
  }

#endif // COMMON_H
