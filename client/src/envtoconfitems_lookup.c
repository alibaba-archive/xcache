/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf src/envtoconfitems.gperf  */
/* Computed positions: -k'1,4,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 9 "src/envtoconfitems.gperf"
struct env_to_conf_item;
/* maximum key range = 63, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
envtoconfitems_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
       0, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65,  5, 10,  0, 15, 10,
      25, 65,  5, 25, 65, 25, 15, 10,  0,  0,
       0, 65,  0,  5,  0, 10,  0, 65, 10, 10,
      65, 65, 65, 65, 65, 35, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

static
#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct env_to_conf_item *
envtoconfitems_get (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 36,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 15,
      MIN_HASH_VALUE = 2,
      MAX_HASH_VALUE = 64
    };

  static const struct env_to_conf_item wordlist[] =
    {
      {"",""}, {"",""},
#line 12 "src/envtoconfitems.gperf"
      {"CC", "compiler"},
      {"",""},
#line 17 "src/envtoconfitems.gperf"
      {"CPP2", "run_second_cpp"},
      {"",""}, {"",""},
#line 44 "src/envtoconfitems.gperf"
      {"TEMPDIR", "temporary_dir"},
#line 13 "src/envtoconfitems.gperf"
      {"COMPILER", "compiler"},
      {"",""}, {"",""}, {"",""},
#line 33 "src/envtoconfitems.gperf"
      {"NLEVELS", "cache_dir_levels"},
#line 15 "src/envtoconfitems.gperf"
      {"COMPRESS", "compression"},
#line 34 "src/envtoconfitems.gperf"
      {"PATH", "path"},
#line 43 "src/envtoconfitems.gperf"
      {"STATS", "stats"},
      {"",""},
#line 25 "src/envtoconfitems.gperf"
      {"HASHDIR", "hash_dir"},
#line 19 "src/envtoconfitems.gperf"
      {"DIR", "cache_dir"},
      {"",""},
#line 42 "src/envtoconfitems.gperf"
      {"SLOPPINESS", "sloppiness"},
#line 32 "src/envtoconfitems.gperf"
      {"REMOTE_ONLY", "remote_only"},
#line 41 "src/envtoconfitems.gperf"
      {"RECACHE", "recache"},
#line 18 "src/envtoconfitems.gperf"
      {"COMMENTS", "keep_comments_cpp"},
      {"",""},
#line 23 "src/envtoconfitems.gperf"
      {"EXTRAFILES", "extra_files_to_hash"},
      {"",""},
#line 11 "src/envtoconfitems.gperf"
      {"BASEDIR", "base_dir"},
#line 16 "src/envtoconfitems.gperf"
      {"COMPRESSLEVEL", "compression_level"},
#line 22 "src/envtoconfitems.gperf"
      {"EXTENSION", "cpp_extension"},
#line 39 "src/envtoconfitems.gperf"
      {"READONLY_DIRECT", "read_only_direct"},
#line 20 "src/envtoconfitems.gperf"
      {"DIRECT", "direct_mode"},
#line 30 "src/envtoconfitems.gperf"
      {"MAXSIZE", "max_size"},
#line 38 "src/envtoconfitems.gperf"
      {"READONLY", "read_only"},
      {"",""},
#line 37 "src/envtoconfitems.gperf"
      {"PREFIX_CPP", "prefix_command_cpp"},
#line 31 "src/envtoconfitems.gperf"
      {"REMOTE_CONF", "remote_conf"},
#line 21 "src/envtoconfitems.gperf"
      {"DISABLE", "disable"},
#line 14 "src/envtoconfitems.gperf"
      {"COMPILERCHECK", "compiler_check"},
      {"",""},
#line 40 "src/envtoconfitems.gperf"
      {"READONLY_REMOTE", "read_only_remote"},
#line 36 "src/envtoconfitems.gperf"
      {"PREFIX", "prefix_command"},
      {"",""},
#line 26 "src/envtoconfitems.gperf"
      {"IGNOREHEADERS", "ignore_headers_in_manifest"},
      {"",""},
#line 45 "src/envtoconfitems.gperf"
      {"UMASK", "umask"},
      {"",""}, {"",""},
#line 29 "src/envtoconfitems.gperf"
      {"MAXFILES", "max_files"},
      {"",""},
#line 46 "src/envtoconfitems.gperf"
      {"UNIFY", "unify"},
      {"",""}, {"",""},
#line 24 "src/envtoconfitems.gperf"
      {"HARDLINK", "hard_link"},
      {"",""},
#line 35 "src/envtoconfitems.gperf"
      {"PCH_EXTSUM", "pch_external_checksum"},
      {"",""},
#line 28 "src/envtoconfitems.gperf"
      {"LOGFILE", "log_file"},
      {"",""}, {"",""}, {"",""}, {"",""}, {"",""}, {"",""},
#line 27 "src/envtoconfitems.gperf"
      {"LIMIT_MULTIPLE", "limit_multiple"}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = envtoconfitems_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].env_name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
static const size_t ENVTOCONFITEMS_TOTAL_KEYWORDS = 36;
