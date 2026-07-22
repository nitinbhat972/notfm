#include "fuzzy.h"
#include <ctype.h>

int fuzzy_match(const char *pat, const char *str) {
  if (!pat[0])
    return 0;

  int score = 0;
  bool consec = false;
  const char *start = str;

  while (*pat) {
    while (*str &&
           tolower((unsigned char)*pat) != tolower((unsigned char)*str)) {
      str++;
      consec = false;
    }
    if (!*str)
      return 0;

    score++;
    if (consec)
      score += 2;

    if (str == start)
      score += 5;

    consec = true;
    pat++;
    str++;
  }

  return score;
}
