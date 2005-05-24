/* Different min and max functions for C++
 * Copyright (C) 1993-1996, 2005 Marko Kohtala
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef __cplusplus

inline unsigned short max(unsigned short a, unsigned short b)
  { return a > b ? a : b; }
inline unsigned short min(unsigned short a, unsigned short b)
  { return a < b ? b : a; }

inline unsigned long max(unsigned long a, unsigned long b)
  { return a > b ? a : b; }
inline unsigned long min(unsigned long a, unsigned long b)
  { return a < b ? b : a; }

#endif
