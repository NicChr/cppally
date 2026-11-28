/* puff.h
  Copyright (C) 2002-2013 Mark Adler, all rights reserved
  version 2.3, 21 Jan 2013

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */

// ALTERED for cppally: a C++ port of puff.c and puff.h from zlib 1.3.1 (contrib/puff), not the original software.
// Changes: namespaced; errors throw std::runtime_error in place of longjmp() and return codes; the fixed code
// tables are built per block in place of thread-unsafe statics; the size-scanning mode and the SLOW decoder are removed.
// Inflates raw deflate data (RFC 1951).

#ifndef CPPALLY_PUFF_H
#define CPPALLY_PUFF_H

#include <cstddef>
#include <stdexcept>

namespace zlib {

namespace puff {

constexpr int max_bits = 15;      // Maximum bits in a code
constexpr int max_lcodes = 286;   // Maximum number of literal/length codes
constexpr int max_dcodes = 30;    // Maximum number of distance codes
constexpr int fix_lcodes = 288;   // Number of fixed literal/length codes

constexpr short length_base[29] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr short length_extra[29] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr short dist_base[30] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
  8193, 12289, 16385, 24577};
constexpr short dist_extra[30] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
  7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
  12, 12, 13, 13};
// Permutation of code length codes
constexpr short code_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

[[noreturn]] inline void fail(const char* what) {
  throw std::runtime_error(what);
}

struct state {
  unsigned char* out;
  std::size_t outlen;
  std::size_t outcnt;
  const unsigned char* in;
  std::size_t inlen;
  std::size_t incnt;
  int bitbuf;
  int bitcnt;
};

// Canonical Huffman decoding tables: count[1..max_bits] symbols of each length, symbols in canonical order
struct huffman {
  short* count;
  short* symbol;
};

// Returns `need` bits from the input, leaving fewer than eight in the buffer
[[gnu::cold]] inline int bits(state& s, int need) {
  long val = s.bitbuf;
  while (s.bitcnt < need) {
    if (s.incnt == s.inlen) {
      fail("inflate: out of input");
    }
    val |= static_cast<long>(s.in[s.incnt++]) << s.bitcnt;
    s.bitcnt += 8;
  }
  s.bitbuf = static_cast<int>(val >> need);
  s.bitcnt -= need;
  return static_cast<int>(val & ((1L << need) - 1));
}

[[gnu::cold]] inline void stored(state& s) {
  // Discard leftover bits from the current byte
  s.bitbuf = 0;
  s.bitcnt = 0;
  if (s.incnt + 4 > s.inlen) {
    fail("inflate: out of input");
  }
  unsigned len = s.in[s.incnt++];
  len |= static_cast<unsigned>(s.in[s.incnt++]) << 8;
  if (s.in[s.incnt++] != (~len & 0xff) || s.in[s.incnt++] != ((~len >> 8) & 0xff)) {
    fail("inflate: stored block length did not match its complement");
  }
  if (s.incnt + len > s.inlen) {
    fail("inflate: out of input");
  }
  if (s.outcnt + len > s.outlen) {
    fail("inflate: output space exhausted");
  }
  while (len--) {
    s.out[s.outcnt++] = s.in[s.incnt++];
  }
}

// Decodes a symbol using `h`, -10 if the code is empty or an invalid code is received
[[gnu::cold]] inline int decode(state& s, const huffman& h) {
  int bitbuf = s.bitbuf;
  int left = s.bitcnt;
  int code = 0;
  int first = 0;
  int index = 0;
  int len = 1;
  const short* next = h.count + 1;
  while (true) {
    while (left--) {
      code |= bitbuf & 1;
      bitbuf >>= 1;
      int count = *next++;
      if (code - count < first) {
        s.bitbuf = bitbuf;
        s.bitcnt = (s.bitcnt - len) & 7;
        return h.symbol[index + (code - first)];
      }
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
      len++;
    }
    left = (max_bits + 1) - len;
    if (left == 0) {
      break;
    }
    if (s.incnt == s.inlen) {
      fail("inflate: out of input");
    }
    bitbuf = s.in[s.incnt++];
    if (left > 8) {
      left = 8;
    }
  }
  return -10;
}

// Builds decoding tables from code lengths length[0..n-1]. Zero for a complete code set,
// negative for an over-subscribed one, positive for an incomplete one
[[gnu::cold]] inline int construct(huffman& h, const short* length, int n) {
  short offs[max_bits + 1];
  for (int len = 0; len <= max_bits; len++) {
    h.count[len] = 0;
  }
  for (int symbol = 0; symbol < n; symbol++) {
    (h.count[length[symbol]])++;
  }
  if (h.count[0] == n) {
    return 0;
  }
  int left = 1;
  for (int len = 1; len <= max_bits; len++) {
    left <<= 1;
    left -= h.count[len];
    if (left < 0) {
      return left;
    }
  }
  offs[1] = 0;
  for (int len = 1; len < max_bits; len++) {
    offs[len + 1] = static_cast<short>(offs[len] + h.count[len]);
  }
  for (int symbol = 0; symbol < n; symbol++) {
    if (length[symbol] != 0) {
      h.symbol[offs[length[symbol]]++] = static_cast<short>(symbol);
    }
  }
  return left;
}

// Decodes literals and length/distance pairs until an end-of-block code
[[gnu::cold]] inline void codes(state& s, const huffman& lencode, const huffman& distcode) {
  int symbol;
  do {
    symbol = decode(s, lencode);
    if (symbol < 0) {
      fail("inflate: invalid literal/length code");
    }
    if (symbol < 256) {
      if (s.outcnt == s.outlen) {
        fail("inflate: output space exhausted");
      }
      s.out[s.outcnt++] = static_cast<unsigned char>(symbol);
    } else if (symbol > 256) {
      symbol -= 257;
      if (symbol >= 29) {
        fail("inflate: invalid fixed code");
      }
      int len = length_base[symbol] + bits(s, length_extra[symbol]);
      symbol = decode(s, distcode);
      if (symbol < 0) {
        fail("inflate: invalid distance code");
      }
      std::size_t dist = static_cast<std::size_t>(dist_base[symbol] + bits(s, dist_extra[symbol]));
      if (dist > s.outcnt) {
        fail("inflate: distance too far back");
      }
      if (s.outcnt + static_cast<std::size_t>(len) > s.outlen) {
        fail("inflate: output space exhausted");
      }
      // Overlapping copies are allowed, so copy forwards byte by byte
      while (len--) {
        s.out[s.outcnt] = s.out[s.outcnt - dist];
        s.outcnt++;
      }
    }
  } while (symbol != 256);
}

[[gnu::cold]] inline void fixed(state& s) {
  short lencnt[max_bits + 1], lensym[fix_lcodes];
  short distcnt[max_bits + 1], distsym[max_dcodes];
  huffman lencode{lencnt, lensym};
  huffman distcode{distcnt, distsym};
  short lengths[fix_lcodes];
  int symbol = 0;
  for (; symbol < 144; symbol++) {
    lengths[symbol] = 8;
  }
  for (; symbol < 256; symbol++) {
    lengths[symbol] = 9;
  }
  for (; symbol < 280; symbol++) {
    lengths[symbol] = 7;
  }
  for (; symbol < fix_lcodes; symbol++) {
    lengths[symbol] = 8;
  }
  construct(lencode, lengths, fix_lcodes);
  for (symbol = 0; symbol < max_dcodes; symbol++) {
    lengths[symbol] = 5;
  }
  construct(distcode, lengths, max_dcodes);
  codes(s, lencode, distcode);
}

[[gnu::cold]] inline void dynamic(state& s) {
  short lengths[max_lcodes + max_dcodes];
  short lencnt[max_bits + 1], lensym[max_lcodes];
  short distcnt[max_bits + 1], distsym[max_dcodes];
  huffman lencode{lencnt, lensym};
  huffman distcode{distcnt, distsym};

  int nlen = bits(s, 5) + 257;
  int ndist = bits(s, 5) + 1;
  int ncode = bits(s, 4) + 4;
  if (nlen > max_lcodes || ndist > max_dcodes) {
    fail("inflate: too many length or distance codes");
  }
  int index = 0;
  for (; index < ncode; index++) {
    lengths[code_order[index]] = static_cast<short>(bits(s, 3));
  }
  for (; index < 19; index++) {
    lengths[code_order[index]] = 0;
  }
  // Code length codes must be complete
  if (construct(lencode, lengths, 19) != 0) {
    fail("inflate: code lengths codes incomplete");
  }
  index = 0;
  while (index < nlen + ndist) {
    int symbol = decode(s, lencode);
    if (symbol < 0) {
      fail("inflate: invalid code length code");
    }
    if (symbol < 16) {
      lengths[index++] = static_cast<short>(symbol);
    } else {
      short len = 0;
      if (symbol == 16) {
        if (index == 0) {
          fail("inflate: repeat lengths with no first length");
        }
        len = lengths[index - 1];
        symbol = 3 + bits(s, 2);
      } else if (symbol == 17) {
        symbol = 3 + bits(s, 3);
      } else {
        symbol = 11 + bits(s, 7);
      }
      if (index + symbol > nlen + ndist) {
        fail("inflate: repeat more than specified lengths");
      }
      while (symbol--) {
        lengths[index++] = len;
      }
    }
  }
  if (lengths[256] == 0) {
    fail("inflate: missing end-of-block code");
  }
  // Incomplete codes are only allowed for a single length 1 code
  int err = construct(lencode, lengths, nlen);
  if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) {
    fail("inflate: invalid literal/length code lengths");
  }
  err = construct(distcode, lengths + nlen, ndist);
  if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) {
    fail("inflate: invalid distance code lengths");
  }
  codes(s, lencode, distcode);
}

// Inflates `source` into `dest`, which it must fill exactly. Throws std::runtime_error on invalid or truncated data
[[gnu::cold]] inline void inflate(unsigned char* dest, std::size_t destlen, const unsigned char* source, std::size_t sourcelen) {
  state s{dest, destlen, 0, source, sourcelen, 0, 0, 0};
  int last;
  do {
    last = bits(s, 1);
    int type = bits(s, 2);
    if (type == 0) {
      stored(s);
    } else if (type == 1) {
      fixed(s);
    } else if (type == 2) {
      dynamic(s);
    } else {
      fail("inflate: invalid block type");
    }
  } while (!last);
  if (s.outcnt != destlen) {
    fail("inflate: output size mismatch");
  }
}

}

}

#endif
