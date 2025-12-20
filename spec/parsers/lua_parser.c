#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// lua_parser - generated parser

typedef struct {
  const char *input;
  size_t input_len;
  size_t pos;
  bool success;
  char error_message[256];
  size_t depth;
  lua_State *L;
} Parser;

typedef struct {
  size_t pos;
  int stack_size;
} ParserPosition;

#define REMEMBER_POSITION(parser, pos)                                         \
  ParserPosition pos;                                                          \
  (pos).pos = (parser)->pos;                                                   \
  (pos).stack_size = lua_gettop((parser)->L);

// Restore parser position
#define RESTORE_POSITION(parser, pos)                                          \
  (parser)->pos = (pos).pos;                                                   \
  lua_settop((parser)->L, (pos).stack_size);

#ifdef PGEN_DEBUG
static void dumpstack(lua_State *L) {
  int top = lua_gettop(L);
  for (int i = 1; i <= top; i++) {
    printf("%d\t%s\t", i, luaL_typename(L, i));
    switch (lua_type(L, i)) {
    case LUA_TNUMBER:
      printf("%g\n", lua_tonumber(L, i));
      break;
    case LUA_TSTRING:
      printf("%s\n", lua_tostring(L, i));
      break;
    case LUA_TBOOLEAN:
      printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
      break;
    case LUA_TNIL:
      printf("%s\n", "nil");
      break;
    default:
      printf("%p\n", lua_topointer(L, i));
      break;
    }
  }
}
#endif

// Forward declarations
static bool parse_Number(Parser *parser);
static bool parse_functiondef(Parser *parser);
static bool parse_comment(Parser *parser);
static bool parse_unop(Parser *parser);
static bool parse_retstat(Parser *parser);
static bool parse_prefixexp(Parser *parser);
static bool parse_block(Parser *parser);
static bool parse_fieldsep(Parser *parser);
static bool parse_funcbody(Parser *parser);
static bool parse_explist(Parser *parser);
static bool parse_namelist(Parser *parser);
static bool parse_call_suffix(Parser *parser);
static bool parse_String(Parser *parser);
static bool parse_exp(Parser *parser);
static bool parse_keyword(Parser *parser);
static bool parse_simple_exp(Parser *parser);
static bool parse_attnamelist(Parser *parser);
static bool parse_var(Parser *parser);
static bool parse_binop(Parser *parser);
static bool parse_fieldlist(Parser *parser);
static bool parse_functioncall(Parser *parser);
static bool parse_prefixexp_inner(Parser *parser);
static bool parse_stat(Parser *parser);
static bool parse_ws(Parser *parser);
static bool parse_varlist(Parser *parser);
static bool parse_funcname(Parser *parser);
static bool parse_primary(Parser *parser);
static bool parse_var_suffix(Parser *parser);
static bool parse_tableconstructor(Parser *parser);
static bool parse_suffix(Parser *parser);
static bool parse_attrib(Parser *parser);
static bool parse_Name(Parser *parser);
static bool parse_args(Parser *parser);
static bool parse_chunk(Parser *parser);
static bool parse_ident(Parser *parser);
static bool parse_parlist(Parser *parser);
static bool parse_field(Parser *parser);

// Rule functions
static bool parse_Number(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "Number", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "number", 6);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          {     // Choice
            {   // Choice
              { // Sequence with 5 patterns
                REMEMBER_POSITION(parser, pos);

                { // Match single character "0"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 48) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "0"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  { // Match character set "xX"
                    if (parser->pos < parser->input_len) {
                      switch (parser->input[parser->pos]) {
                      case 120: /* "x" */
                      case 88:  /* "X" */
                        parser->pos++;
                        break;
                      default:
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected one of "
                                "\"xX\""
                                " at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected one of "
                              "\"xX\""
                              " at position %zu but reached end of input",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    { // At least 1 repetitions
                      REMEMBER_POSITION(parser, pos);
                      size_t rep_count = 0;

                      while (true) {
                        { // Match character range: "09,af,AF"
                          if (parser->pos < parser->input_len &&
                              ((parser->input[parser->pos] >= 48 &&
                                parser->input[parser->pos] <= 57) ||
                               (parser->input[parser->pos] >= 97 &&
                                parser->input[parser->pos] <= 102) ||
                               (parser->input[parser->pos] >= 65 &&
                                parser->input[parser->pos] <= 70))) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character in ranges ["
                                    "0"
                                    " - "
                                    "9"
                                    ", "
                                    "a"
                                    " - "
                                    "f"
                                    ", "
                                    "A"
                                    " - "
                                    "F"
                                    "] at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }

                        if (!parser->success) {
                          break;
                        }

                        rep_count += 1;
                      }

                      if (rep_count >= 1) {
                        parser->success = true;
                      } else {
                        RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected 1 repetitions at position %zu",
                                parser->pos);
#endif
                      }
                    }
                    if (parser->success) {
                      { // At most 1 repetitions
                        size_t rep_count = 0;

                        while (rep_count < 1) {
                          size_t before_pos = parser->pos;

                          {
                            { // Sequence with 2 patterns
                              REMEMBER_POSITION(parser, pos);

                              { // Match single character "."
                                if (parser->pos < parser->input_len &&
                                    parser->input[parser->pos] == 46) {
                                  parser->pos++;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected character `"
                                          "."
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (parser->success) {
                                { // Zero or more repetitions
                                  while (true) {
                                    { // Match character range: "09,af,AF"
                                      if (parser->pos < parser->input_len &&
                                          ((parser->input[parser->pos] >= 48 &&
                                            parser->input[parser->pos] <= 57) ||
                                           (parser->input[parser->pos] >= 97 &&
                                            parser->input[parser->pos] <=
                                                102) ||
                                           (parser->input[parser->pos] >= 65 &&
                                            parser->input[parser->pos] <=
                                                70))) {
                                        parser->pos++;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected character in ranges ["
                                                "0"
                                                " - "
                                                "9"
                                                ", "
                                                "a"
                                                " - "
                                                "f"
                                                ", "
                                                "A"
                                                " - "
                                                "F"
                                                "] at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                    if (!parser->success) {
                                      break;
                                    }
                                  }
                                  parser->success = true;
                                }
                                if (!parser->success) {
                                  RESTORE_POSITION(parser, pos);
                                }
                              }
                            }
                          }

                          if (!parser->success || before_pos == parser->pos) {
                            // Break on failure or zero-width match
                            parser->success = true;
                            break;
                          }

                          rep_count += 1;
                        }
                      }
                      if (parser->success) {
                        { // At most 1 repetitions
                          size_t rep_count = 0;

                          while (rep_count < 1) {
                            size_t before_pos = parser->pos;

                            {
                              { // Sequence with 3 patterns
                                REMEMBER_POSITION(parser, pos);

                                { // Match character set "pP"
                                  if (parser->pos < parser->input_len) {
                                    switch (parser->input[parser->pos]) {
                                    case 112: /* "p" */
                                    case 80:  /* "P" */
                                      parser->pos++;
                                      break;
                                    default:
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected one of "
                                              "\"pP\""
                                              " at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected one of "
                                            "\"pP\""
                                            " at position %zu but reached end "
                                            "of input",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  { // At most 1 repetitions
                                    size_t rep_count = 0;

                                    while (rep_count < 1) {
                                      size_t before_pos = parser->pos;

                                      {
                                        { // Match character set "+-"
                                          if (parser->pos < parser->input_len) {
                                            switch (
                                                parser->input[parser->pos]) {
                                            case 43: /* "+" */
                                            case 45: /* "-" */
                                              parser->pos++;
                                              break;
                                            default:
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected one of "
                                                      "\"+-\""
                                                      " at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          } else {
#ifdef PGEN_ERRORS
                                            sprintf(parser->error_message,
                                                    "Expected one of "
                                                    "\"+-\""
                                                    " at position %zu but "
                                                    "reached end of input",
                                                    parser->pos);
#endif
                                            parser->success = false;
                                          }
                                        }
                                      }

                                      if (!parser->success ||
                                          before_pos == parser->pos) {
                                        // Break on failure or zero-width match
                                        parser->success = true;
                                        break;
                                      }

                                      rep_count += 1;
                                    }
                                  }
                                  if (parser->success) {
                                    { // At least 1 repetitions
                                      REMEMBER_POSITION(parser, pos);
                                      size_t rep_count = 0;

                                      while (true) {
                                        { // Match character range: "09"
                                          if (parser->pos < parser->input_len &&
                                              ((parser->input[parser->pos] >=
                                                    48 &&
                                                parser->input[parser->pos] <=
                                                    57))) {
                                            parser->pos++;
                                          } else {
#ifdef PGEN_ERRORS
                                            sprintf(
                                                parser->error_message,
                                                "Expected character in ranges ["
                                                "0"
                                                " - "
                                                "9"
                                                "] at position %zu",
                                                parser->pos);
#endif
                                            parser->success = false;
                                          }
                                        }

                                        if (!parser->success) {
                                          break;
                                        }

                                        rep_count += 1;
                                      }

                                      if (rep_count >= 1) {
                                        parser->success = true;
                                      } else {
                                        RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected 1 repetitions at "
                                                "position %zu",
                                                parser->pos);
#endif
                                      }
                                    }
                                  }
                                  if (!parser->success) {
                                    RESTORE_POSITION(parser, pos);
                                  }
                                }
                              }
                            }

                            if (!parser->success || before_pos == parser->pos) {
                              // Break on failure or zero-width match
                              parser->success = true;
                              break;
                            }

                            rep_count += 1;
                          }
                        }
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }

              if (!parser->success) {
                parser->success = true;
                { // Sequence with 3 patterns
                  REMEMBER_POSITION(parser, pos);

                  { // At least 1 repetitions
                    REMEMBER_POSITION(parser, pos);
                    size_t rep_count = 0;

                    while (true) {
                      { // Match character range: "09"
                        if (parser->pos < parser->input_len &&
                            ((parser->input[parser->pos] >= 48 &&
                              parser->input[parser->pos] <= 57))) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character in ranges ["
                                  "0"
                                  " - "
                                  "9"
                                  "] at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }

                      if (!parser->success) {
                        break;
                      }

                      rep_count += 1;
                    }

                    if (rep_count >= 1) {
                      parser->success = true;
                    } else {
                      RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected 1 repetitions at position %zu",
                              parser->pos);
#endif
                    }
                  }
                  if (parser->success) {
                    { // At most 1 repetitions
                      size_t rep_count = 0;

                      while (rep_count < 1) {
                        size_t before_pos = parser->pos;

                        {
                          { // Sequence with 2 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Match single character "."
                              if (parser->pos < parser->input_len &&
                                  parser->input[parser->pos] == 46) {
                                parser->pos++;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected character `"
                                        "."
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                            if (parser->success) {
                              { // Zero or more repetitions
                                while (true) {
                                  { // Match character range: "09"
                                    if (parser->pos < parser->input_len &&
                                        ((parser->input[parser->pos] >= 48 &&
                                          parser->input[parser->pos] <= 57))) {
                                      parser->pos++;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected character in ranges ["
                                              "0"
                                              " - "
                                              "9"
                                              "] at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
                                  if (!parser->success) {
                                    break;
                                  }
                                }
                                parser->success = true;
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }
                        }

                        if (!parser->success || before_pos == parser->pos) {
                          // Break on failure or zero-width match
                          parser->success = true;
                          break;
                        }

                        rep_count += 1;
                      }
                    }
                    if (parser->success) {
                      { // At most 1 repetitions
                        size_t rep_count = 0;

                        while (rep_count < 1) {
                          size_t before_pos = parser->pos;

                          {
                            { // Sequence with 3 patterns
                              REMEMBER_POSITION(parser, pos);

                              { // Match character set "eE"
                                if (parser->pos < parser->input_len) {
                                  switch (parser->input[parser->pos]) {
                                  case 101: /* "e" */
                                  case 69:  /* "E" */
                                    parser->pos++;
                                    break;
                                  default:
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected one of "
                                            "\"eE\""
                                            " at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected one of "
                                          "\"eE\""
                                          " at position %zu but reached end of "
                                          "input",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (parser->success) {
                                { // At most 1 repetitions
                                  size_t rep_count = 0;

                                  while (rep_count < 1) {
                                    size_t before_pos = parser->pos;

                                    {
                                      { // Match character set "+-"
                                        if (parser->pos < parser->input_len) {
                                          switch (parser->input[parser->pos]) {
                                          case 43: /* "+" */
                                          case 45: /* "-" */
                                            parser->pos++;
                                            break;
                                          default:
#ifdef PGEN_ERRORS
                                            sprintf(parser->error_message,
                                                    "Expected one of "
                                                    "\"+-\""
                                                    " at position %zu",
                                                    parser->pos);
#endif
                                            parser->success = false;
                                          }
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected one of "
                                                  "\"+-\""
                                                  " at position %zu but "
                                                  "reached end of input",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                    }

                                    if (!parser->success ||
                                        before_pos == parser->pos) {
                                      // Break on failure or zero-width match
                                      parser->success = true;
                                      break;
                                    }

                                    rep_count += 1;
                                  }
                                }
                                if (parser->success) {
                                  { // At least 1 repetitions
                                    REMEMBER_POSITION(parser, pos);
                                    size_t rep_count = 0;

                                    while (true) {
                                      { // Match character range: "09"
                                        if (parser->pos < parser->input_len &&
                                            ((parser->input[parser->pos] >=
                                                  48 &&
                                              parser->input[parser->pos] <=
                                                  57))) {
                                          parser->pos++;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(
                                              parser->error_message,
                                              "Expected character in ranges ["
                                              "0"
                                              " - "
                                              "9"
                                              "] at position %zu",
                                              parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }

                                      if (!parser->success) {
                                        break;
                                      }

                                      rep_count += 1;
                                    }

                                    if (rep_count >= 1) {
                                      parser->success = true;
                                    } else {
                                      RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected 1 repetitions at "
                                              "position %zu",
                                              parser->pos);
#endif
                                    }
                                  }
                                }
                                if (!parser->success) {
                                  RESTORE_POSITION(parser, pos);
                                }
                              }
                            }
                          }

                          if (!parser->success || before_pos == parser->pos) {
                            // Break on failure or zero-width match
                            parser->success = true;
                            break;
                          }

                          rep_count += 1;
                        }
                      }
                    }
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Sequence with 3 patterns
                REMEMBER_POSITION(parser, pos);

                { // Match single character "."
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 46) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "."
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  { // At least 1 repetitions
                    REMEMBER_POSITION(parser, pos);
                    size_t rep_count = 0;

                    while (true) {
                      { // Match character range: "09"
                        if (parser->pos < parser->input_len &&
                            ((parser->input[parser->pos] >= 48 &&
                              parser->input[parser->pos] <= 57))) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character in ranges ["
                                  "0"
                                  " - "
                                  "9"
                                  "] at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }

                      if (!parser->success) {
                        break;
                      }

                      rep_count += 1;
                    }

                    if (rep_count >= 1) {
                      parser->success = true;
                    } else {
                      RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected 1 repetitions at position %zu",
                              parser->pos);
#endif
                    }
                  }
                  if (parser->success) {
                    { // At most 1 repetitions
                      size_t rep_count = 0;

                      while (rep_count < 1) {
                        size_t before_pos = parser->pos;

                        {
                          { // Sequence with 3 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Match character set "eE"
                              if (parser->pos < parser->input_len) {
                                switch (parser->input[parser->pos]) {
                                case 101: /* "e" */
                                case 69:  /* "E" */
                                  parser->pos++;
                                  break;
                                default:
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected one of "
                                          "\"eE\""
                                          " at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(
                                    parser->error_message,
                                    "Expected one of "
                                    "\"eE\""
                                    " at position %zu but reached end of input",
                                    parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                            if (parser->success) {
                              { // At most 1 repetitions
                                size_t rep_count = 0;

                                while (rep_count < 1) {
                                  size_t before_pos = parser->pos;

                                  {
                                    { // Match character set "+-"
                                      if (parser->pos < parser->input_len) {
                                        switch (parser->input[parser->pos]) {
                                        case 43: /* "+" */
                                        case 45: /* "-" */
                                          parser->pos++;
                                          break;
                                        default:
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected one of "
                                                  "\"+-\""
                                                  " at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected one of "
                                                "\"+-\""
                                                " at position %zu but reached "
                                                "end of input",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                  }

                                  if (!parser->success ||
                                      before_pos == parser->pos) {
                                    // Break on failure or zero-width match
                                    parser->success = true;
                                    break;
                                  }

                                  rep_count += 1;
                                }
                              }
                              if (parser->success) {
                                { // At least 1 repetitions
                                  REMEMBER_POSITION(parser, pos);
                                  size_t rep_count = 0;

                                  while (true) {
                                    { // Match character range: "09"
                                      if (parser->pos < parser->input_len &&
                                          ((parser->input[parser->pos] >= 48 &&
                                            parser->input[parser->pos] <=
                                                57))) {
                                        parser->pos++;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected character in ranges ["
                                                "0"
                                                " - "
                                                "9"
                                                "] at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }

                                    if (!parser->success) {
                                      break;
                                    }

                                    rep_count += 1;
                                  }

                                  if (rep_count >= 1) {
                                    parser->success = true;
                                  } else {
                                    RESTORE_POSITION(parser, pos);
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected 1 repetitions at "
                                            "position %zu",
                                            parser->pos);
#endif
                                  }
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }
                        }

                        if (!parser->success || before_pos == parser->pos) {
                          // Break on failure or zero-width match
                          parser->success = true;
                          break;
                        }

                        rep_count += 1;
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            // TODO: ensure stack has enough space for push
            lua_pushlstring(parser->L, parser->input + start_pos,
                            capture_length);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "Number", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "Number", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_functiondef(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "functiondef", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "function", 8);
      }
      if (parser->success) {
        { // Match literal "function"
          if (parser->pos + 8 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "function", 8) == 0) {
            parser->pos += 8;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected `"
                    "function"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          parse_funcbody(parser);
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "functiondef", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "functiondef", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_comment(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "comment", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match literal "--"
      if (parser->pos + 2 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "--", 2) == 0) {
        parser->pos += 2;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected `"
                "--"
                "` at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      {   // Choice
        { // Sequence with 3 patterns
          REMEMBER_POSITION(parser, pos);

          { // Match literal "[["
            if (parser->pos + 2 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "[[", 2) == 0) {
              parser->pos += 2;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected `"
                      "[["
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (parser->success) {
            { // Zero or more repetitions
              while (true) {
                { // Sequence with 2 patterns
                  REMEMBER_POSITION(parser, pos);

                  { // Negate (only match if pattern fails)
                    REMEMBER_POSITION(parser, pos);

                    { // Match literal "]]"
                      if (parser->pos + 2 <= parser->input_len &&
                          memcmp(parser->input + parser->pos, "]]", 2) == 0) {
                        parser->pos += 2;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected `"
                                "]]"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }

                    if (parser->success) {
                      // Pattern matched, so negate fails
                      RESTORE_POSITION(parser, pos);
                      parser->success = false;
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Negated pattern unexpectedly matched at "
                              "position %zu",
                              pos.pos);
#endif
                    } else {
                      // Pattern failed, so negate succeeds
                      parser->success = true;
                      RESTORE_POSITION(
                          parser,
                          pos); // Restore original position (technically not
                                // necessary since failed pattern should make no
                                // changes to position)
                    }
                  }
                  if (parser->success) {
                    { // Match any 1 characters
                      if (parser->pos + 1 <= parser->input_len) {
                        parser->pos += 1;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected at least 1 more characters at "
                                "position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }
                if (!parser->success) {
                  break;
                }
              }
              parser->success = true;
            }
            if (parser->success) {
              { // Match literal "]]"
                if (parser->pos + 2 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "]]", 2) == 0) {
                  parser->pos += 2;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected `"
                          "]]"
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 2 patterns
                REMEMBER_POSITION(parser, pos);

                { // Negate (only match if pattern fails)
                  REMEMBER_POSITION(parser, pos);

                  { // Match single character "\n"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 10) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              "\\n"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }

                  if (parser->success) {
                    // Pattern matched, so negate fails
                    RESTORE_POSITION(parser, pos);
                    parser->success = false;
#ifdef PGEN_ERRORS
                    sprintf(
                        parser->error_message,
                        "Negated pattern unexpectedly matched at position %zu",
                        pos.pos);
#endif
                  } else {
                    // Pattern failed, so negate succeeds
                    parser->success = true;
                    RESTORE_POSITION(
                        parser, pos); // Restore original position (technically
                                      // not necessary since failed pattern
                                      // should make no changes to position)
                  }
                }
                if (parser->success) {
                  { // Match any 1 characters
                    if (parser->pos + 1 <= parser->input_len) {
                      parser->pos += 1;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(
                          parser->error_message,
                          "Expected at least 1 more characters at position %zu",
                          parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "comment", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "comment", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_unop(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "unop", start);
#endif

  { // Capture
    size_t start_pos = parser->pos;
    {       // Choice
      {     // Choice
        {   // Choice
          { // Match single character "-"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 45) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character `"
                      "-"
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Match literal "not"
              if (parser->pos + 3 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "not", 3) == 0) {
                parser->pos += 3;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected `"
                        "not"
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Match single character "#"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 35) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character `"
                      "#"
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
        }
      }

      if (!parser->success) {
        parser->success = true;
        { // Match single character "~"
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 126) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character `"
                    "~"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
      }
    }

    if (parser->success) {
      size_t capture_length = parser->pos - start_pos;
      // TODO: ensure stack has enough space for push
      lua_pushlstring(parser->L, parser->input + start_pos, capture_length);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "unop", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "unop", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_retstat(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "retstat", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 4 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "return", 6);
      }
      if (parser->success) {
        { // Match literal "return"
          if (parser->pos + 6 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "return", 6) == 0) {
            parser->pos += 6;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected `"
                    "return"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          { // At most 1 repetitions
            size_t rep_count = 0;

            while (rep_count < 1) {
              size_t before_pos = parser->pos;

              {
                { // Sequence with 2 patterns
                  REMEMBER_POSITION(parser, pos);

                  parse_ws(parser);
                  if (parser->success) {
                    parse_explist(parser);
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }
              }

              if (!parser->success || before_pos == parser->pos) {
                // Break on failure or zero-width match
                parser->success = true;
                break;
              }

              rep_count += 1;
            }
          }
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  { // Sequence with 2 patterns
                    REMEMBER_POSITION(parser, pos);

                    parse_ws(parser);
                    if (parser->success) {
                      { // Match single character ";"
                        if (parser->pos < parser->input_len &&
                            parser->input[parser->pos] == 59) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character `"
                                  ";"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (!parser->success) {
                        RESTORE_POSITION(parser, pos);
                      }
                    }
                  }
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "retstat", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "retstat", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_prefixexp(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "prefixexp", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "prefixexp", 9);
      }
      if (parser->success) {
        parse_prefixexp_inner(parser);
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "prefixexp", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "prefixexp", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_block(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "block", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 5 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "block", 5);
      }
      if (parser->success) {
        parse_ws(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              parse_stat(parser);
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  parse_retstat(parser);
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
            if (parser->success) {
              parse_ws(parser);
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "block", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "block", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_fieldsep(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "fieldsep", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    parse_ws(parser);
    if (parser->success) {
      {   // Choice
        { // Match single character ","
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 44) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character `"
                    ","
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Match single character ";"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 59) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character `"
                      ";"
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
        }
      }
      if (parser->success) {
        parse_ws(parser);
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "fieldsep", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "fieldsep", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_funcbody(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "funcbody", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 11 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "funcbody", 8);
      }
      if (parser->success) {
        parse_ws(parser);
        if (parser->success) {
          { // Match single character "("
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 40) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character `"
                      "("
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (parser->success) {
            parse_ws(parser);
            if (parser->success) {
              { // At most 1 repetitions
                size_t rep_count = 0;

                while (rep_count < 1) {
                  size_t before_pos = parser->pos;

                  {
                    parse_parlist(parser);
                  }

                  if (!parser->success || before_pos == parser->pos) {
                    // Break on failure or zero-width match
                    parser->success = true;
                    break;
                  }

                  rep_count += 1;
                }
              }
              if (parser->success) {
                parse_ws(parser);
                if (parser->success) {
                  { // Match single character ")"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 41) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              ")"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_block(parser);
                      if (parser->success) {
                        parse_ws(parser);
                        if (parser->success) {
                          { // Match literal "end"
                            if (parser->pos + 3 <= parser->input_len &&
                                memcmp(parser->input + parser->pos, "end", 3) ==
                                    0) {
                              parser->pos += 3;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message,
                                      "Expected `"
                                      "end"
                                      "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "funcbody", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "funcbody", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_explist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "explist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "explist", 7);
      }
      if (parser->success) {
        parse_exp(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 4 patterns
                REMEMBER_POSITION(parser, pos);

                parse_ws(parser);
                if (parser->success) {
                  { // Match single character ","
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 44) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              ","
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_exp(parser);
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "explist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "explist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_namelist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "namelist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "namelist", 8);
      }
      if (parser->success) {
        parse_Name(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 4 patterns
                REMEMBER_POSITION(parser, pos);

                parse_ws(parser);
                if (parser->success) {
                  { // Match single character ","
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 44) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              ","
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_Name(parser);
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "namelist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "namelist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_call_suffix(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "call_suffix", start);
#endif

  {   // Choice
    { // Capture Table
      int initial_stack_size = lua_gettop(parser->L);
      { // Sequence with 7 patterns
        REMEMBER_POSITION(parser, pos);

        { // Constant Capture
          // A constant capture matches the empty string and produces all given
          // values
          lua_pushlstring(parser->L, "method", 6);
        }
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            { // Match single character ":"
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 58) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected character `"
                        ":"
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_Name(parser);
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    parse_args(parser);
                  }
                }
              }
            }
          }
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        int new_stack_size = lua_gettop(parser->L);
        int items_count = new_stack_size - initial_stack_size;
        int table_position = -items_count - 1;

        lua_createtable(parser->L, items_count, 0);

        lua_insert(parser->L, table_position);

        for (int i = items_count; i >= 1; --i) {
          lua_rawseti(parser->L, table_position, i);
          table_position += 1;
        }
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 3 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "call", 4);
          }
          if (parser->success) {
            parse_ws(parser);
            if (parser->success) {
              parse_args(parser);
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "call_suffix", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "call_suffix", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_String(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "String", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "string", 6);
      }
      if (parser->success) {
        {     // Choice
          {   // Choice
            { // Capture
              size_t start_pos = parser->pos;
              { // Sequence with 7 patterns
                REMEMBER_POSITION(parser, pos);

                { // Match single character "["
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 91) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "["
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  { // Zero or more repetitions
                    while (true) {
                      { // Match single character "="
                        if (parser->pos < parser->input_len &&
                            parser->input[parser->pos] == 61) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character `"
                                  "="
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (!parser->success) {
                        break;
                      }
                    }
                    parser->success = true;
                  }
                  if (parser->success) {
                    { // Match single character "["
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 91) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                "["
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (parser->success) {
                      { // Zero or more repetitions
                        while (true) {
                          { // Sequence with 2 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Negate (only match if pattern fails)
                              REMEMBER_POSITION(parser, pos);

                              { // Sequence with 3 patterns
                                REMEMBER_POSITION(parser, pos);

                                { // Match single character "]"
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 93) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected character `"
                                            "]"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  { // Zero or more repetitions
                                    while (true) {
                                      { // Match single character "="
                                        if (parser->pos < parser->input_len &&
                                            parser->input[parser->pos] == 61) {
                                          parser->pos++;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected character `"
                                                  "="
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                      if (!parser->success) {
                                        break;
                                      }
                                    }
                                    parser->success = true;
                                  }
                                  if (parser->success) {
                                    { // Match single character "]"
                                      if (parser->pos < parser->input_len &&
                                          parser->input[parser->pos] == 93) {
                                        parser->pos++;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected character `"
                                                "]"
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                  }
                                  if (!parser->success) {
                                    RESTORE_POSITION(parser, pos);
                                  }
                                }
                              }

                              if (parser->success) {
                                // Pattern matched, so negate fails
                                RESTORE_POSITION(parser, pos);
                                parser->success = false;
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Negated pattern unexpectedly matched "
                                        "at position %zu",
                                        pos.pos);
#endif
                              } else {
                                // Pattern failed, so negate succeeds
                                parser->success = true;
                                RESTORE_POSITION(
                                    parser,
                                    pos); // Restore original position
                                          // (technically not necessary since
                                          // failed pattern should make no
                                          // changes to position)
                              }
                            }
                            if (parser->success) {
                              { // Match any 1 characters
                                if (parser->pos + 1 <= parser->input_len) {
                                  parser->pos += 1;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected at least 1 more characters "
                                          "at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }
                          if (!parser->success) {
                            break;
                          }
                        }
                        parser->success = true;
                      }
                      if (parser->success) {
                        { // Match single character "]"
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 93) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character `"
                                    "]"
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          { // Zero or more repetitions
                            while (true) {
                              { // Match single character "="
                                if (parser->pos < parser->input_len &&
                                    parser->input[parser->pos] == 61) {
                                  parser->pos++;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected character `"
                                          "="
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (!parser->success) {
                                break;
                              }
                            }
                            parser->success = true;
                          }
                          if (parser->success) {
                            { // Match single character "]"
                              if (parser->pos < parser->input_len &&
                                  parser->input[parser->pos] == 93) {
                                parser->pos++;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected character `"
                                        "]"
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }

              if (parser->success) {
                size_t capture_length = parser->pos - start_pos;
                // TODO: ensure stack has enough space for push
                lua_pushlstring(parser->L, parser->input + start_pos,
                                capture_length);
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Capture
                size_t start_pos = parser->pos;
                { // Sequence with 3 patterns
                  REMEMBER_POSITION(parser, pos);

                  { // Match single character "'"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 39) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              "'"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    { // Zero or more repetitions
                      while (true) {
                        {   // Choice
                          { // Sequence with 2 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Match single character "\\"
                              if (parser->pos < parser->input_len &&
                                  parser->input[parser->pos] == 92) {
                                parser->pos++;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected character `"
                                        "\\\\"
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                            if (parser->success) {
                              { // Match any 1 characters
                                if (parser->pos + 1 <= parser->input_len) {
                                  parser->pos += 1;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected at least 1 more characters "
                                          "at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }

                          if (!parser->success) {
                            parser->success = true;
                            { // Sequence with 2 patterns
                              REMEMBER_POSITION(parser, pos);

                              { // Negate (only match if pattern fails)
                                REMEMBER_POSITION(parser, pos);

                                { // Match single character "'"
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 39) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected character `"
                                            "'"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }

                                if (parser->success) {
                                  // Pattern matched, so negate fails
                                  RESTORE_POSITION(parser, pos);
                                  parser->success = false;
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Negated pattern unexpectedly "
                                          "matched at position %zu",
                                          pos.pos);
#endif
                                } else {
                                  // Pattern failed, so negate succeeds
                                  parser->success = true;
                                  RESTORE_POSITION(
                                      parser,
                                      pos); // Restore original position
                                            // (technically not necessary since
                                            // failed pattern should make no
                                            // changes to position)
                                }
                              }
                              if (parser->success) {
                                { // Match any 1 characters
                                  if (parser->pos + 1 <= parser->input_len) {
                                    parser->pos += 1;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected at least 1 more "
                                            "characters at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (!parser->success) {
                                  RESTORE_POSITION(parser, pos);
                                }
                              }
                            }
                          }
                        }
                        if (!parser->success) {
                          break;
                        }
                      }
                      parser->success = true;
                    }
                    if (parser->success) {
                      { // Match single character "'"
                        if (parser->pos < parser->input_len &&
                            parser->input[parser->pos] == 39) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character `"
                                  "'"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                    }
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }

                if (parser->success) {
                  size_t capture_length = parser->pos - start_pos;
                  // TODO: ensure stack has enough space for push
                  lua_pushlstring(parser->L, parser->input + start_pos,
                                  capture_length);
                }
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Capture
              size_t start_pos = parser->pos;
              { // Sequence with 3 patterns
                REMEMBER_POSITION(parser, pos);

                { // Match single character "\""
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 34) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "\\\""
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  { // Zero or more repetitions
                    while (true) {
                      {   // Choice
                        { // Sequence with 2 patterns
                          REMEMBER_POSITION(parser, pos);

                          { // Match single character "\\"
                            if (parser->pos < parser->input_len &&
                                parser->input[parser->pos] == 92) {
                              parser->pos++;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message,
                                      "Expected character `"
                                      "\\\\"
                                      "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                          if (parser->success) {
                            { // Match any 1 characters
                              if (parser->pos + 1 <= parser->input_len) {
                                parser->pos += 1;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected at least 1 more characters "
                                        "at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                            if (!parser->success) {
                              RESTORE_POSITION(parser, pos);
                            }
                          }
                        }

                        if (!parser->success) {
                          parser->success = true;
                          { // Sequence with 2 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Negate (only match if pattern fails)
                              REMEMBER_POSITION(parser, pos);

                              { // Match single character "\""
                                if (parser->pos < parser->input_len &&
                                    parser->input[parser->pos] == 34) {
                                  parser->pos++;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected character `"
                                          "\\\""
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }

                              if (parser->success) {
                                // Pattern matched, so negate fails
                                RESTORE_POSITION(parser, pos);
                                parser->success = false;
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Negated pattern unexpectedly matched "
                                        "at position %zu",
                                        pos.pos);
#endif
                              } else {
                                // Pattern failed, so negate succeeds
                                parser->success = true;
                                RESTORE_POSITION(
                                    parser,
                                    pos); // Restore original position
                                          // (technically not necessary since
                                          // failed pattern should make no
                                          // changes to position)
                              }
                            }
                            if (parser->success) {
                              { // Match any 1 characters
                                if (parser->pos + 1 <= parser->input_len) {
                                  parser->pos += 1;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected at least 1 more characters "
                                          "at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }
                        }
                      }
                      if (!parser->success) {
                        break;
                      }
                    }
                    parser->success = true;
                  }
                  if (parser->success) {
                    { // Match single character "\""
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 34) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                "\\\""
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }

              if (parser->success) {
                size_t capture_length = parser->pos - start_pos;
                // TODO: ensure stack has enough space for push
                lua_pushlstring(parser->L, parser->input + start_pos,
                                capture_length);
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "String", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "String", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_exp(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "exp", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "exp", 3);
      }
      if (parser->success) {
        parse_simple_exp(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 4 patterns
                REMEMBER_POSITION(parser, pos);

                parse_ws(parser);
                if (parser->success) {
                  parse_binop(parser);
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_simple_exp(parser);
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "exp", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "exp", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_keyword(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "keyword", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    {                                           // Choice
      {                                         // Choice
        {                                       // Choice
          {                                     // Choice
            {                                   // Choice
              {                                 // Choice
                {                               // Choice
                  {                             // Choice
                    {                           // Choice
                      {                         // Choice
                        {                       // Choice
                          {                     // Choice
                            {                   // Choice
                              {                 // Choice
                                {               // Choice
                                  {             // Choice
                                    {           // Choice
                                      {         // Choice
                                        {       // Choice
                                          {     // Choice
                                            {   // Choice
                                              { // Match literal "and"
                                                if (parser->pos + 3 <=
                                                        parser->input_len &&
                                                    memcmp(parser->input +
                                                               parser->pos,
                                                           "and", 3) == 0) {
                                                  parser->pos += 3;
                                                } else {
#ifdef PGEN_ERRORS
                                                  sprintf(parser->error_message,
                                                          "Expected `"
                                                          "and"
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                  parser->success = false;
                                                }
                                              }

                                              if (!parser->success) {
                                                parser->success = true;
                                                { // Match literal "break"
                                                  if (parser->pos + 5 <=
                                                          parser->input_len &&
                                                      memcmp(parser->input +
                                                                 parser->pos,
                                                             "break", 5) == 0) {
                                                    parser->pos += 5;
                                                  } else {
#ifdef PGEN_ERRORS
                                                    sprintf(
                                                        parser->error_message,
                                                        "Expected `"
                                                        "break"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                    parser->success = false;
                                                  }
                                                }
                                              }
                                            }

                                            if (!parser->success) {
                                              parser->success = true;
                                              { // Match literal "do"
                                                if (parser->pos + 2 <=
                                                        parser->input_len &&
                                                    memcmp(parser->input +
                                                               parser->pos,
                                                           "do", 2) == 0) {
                                                  parser->pos += 2;
                                                } else {
#ifdef PGEN_ERRORS
                                                  sprintf(parser->error_message,
                                                          "Expected `"
                                                          "do"
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                  parser->success = false;
                                                }
                                              }
                                            }
                                          }

                                          if (!parser->success) {
                                            parser->success = true;
                                            { // Match literal "elseif"
                                              if (parser->pos + 6 <=
                                                      parser->input_len &&
                                                  memcmp(parser->input +
                                                             parser->pos,
                                                         "elseif", 6) == 0) {
                                                parser->pos += 6;
                                              } else {
#ifdef PGEN_ERRORS
                                                sprintf(parser->error_message,
                                                        "Expected `"
                                                        "elseif"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                parser->success = false;
                                              }
                                            }
                                          }
                                        }

                                        if (!parser->success) {
                                          parser->success = true;
                                          { // Match literal "else"
                                            if (parser->pos + 4 <=
                                                    parser->input_len &&
                                                memcmp(parser->input +
                                                           parser->pos,
                                                       "else", 4) == 0) {
                                              parser->pos += 4;
                                            } else {
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected `"
                                                      "else"
                                                      "` at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          }
                                        }
                                      }

                                      if (!parser->success) {
                                        parser->success = true;
                                        { // Match literal "end"
                                          if (parser->pos + 3 <=
                                                  parser->input_len &&
                                              memcmp(parser->input +
                                                         parser->pos,
                                                     "end", 3) == 0) {
                                            parser->pos += 3;
                                          } else {
#ifdef PGEN_ERRORS
                                            sprintf(parser->error_message,
                                                    "Expected `"
                                                    "end"
                                                    "` at position %zu",
                                                    parser->pos);
#endif
                                            parser->success = false;
                                          }
                                        }
                                      }
                                    }

                                    if (!parser->success) {
                                      parser->success = true;
                                      { // Match literal "false"
                                        if (parser->pos + 5 <=
                                                parser->input_len &&
                                            memcmp(parser->input + parser->pos,
                                                   "false", 5) == 0) {
                                          parser->pos += 5;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected `"
                                                  "false"
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                    }
                                  }

                                  if (!parser->success) {
                                    parser->success = true;
                                    { // Match literal "for"
                                      if (parser->pos + 3 <=
                                              parser->input_len &&
                                          memcmp(parser->input + parser->pos,
                                                 "for", 3) == 0) {
                                        parser->pos += 3;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected `"
                                                "for"
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                  }
                                }

                                if (!parser->success) {
                                  parser->success = true;
                                  { // Match literal "function"
                                    if (parser->pos + 8 <= parser->input_len &&
                                        memcmp(parser->input + parser->pos,
                                               "function", 8) == 0) {
                                      parser->pos += 8;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected `"
                                              "function"
                                              "` at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
                                }
                              }

                              if (!parser->success) {
                                parser->success = true;
                                { // Match literal "goto"
                                  if (parser->pos + 4 <= parser->input_len &&
                                      memcmp(parser->input + parser->pos,
                                             "goto", 4) == 0) {
                                    parser->pos += 4;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected `"
                                            "goto"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                              }
                            }

                            if (!parser->success) {
                              parser->success = true;
                              { // Match literal "if"
                                if (parser->pos + 2 <= parser->input_len &&
                                    memcmp(parser->input + parser->pos, "if",
                                           2) == 0) {
                                  parser->pos += 2;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected `"
                                          "if"
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                            }
                          }

                          if (!parser->success) {
                            parser->success = true;
                            { // Match literal "in"
                              if (parser->pos + 2 <= parser->input_len &&
                                  memcmp(parser->input + parser->pos, "in",
                                         2) == 0) {
                                parser->pos += 2;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected `"
                                        "in"
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                          }
                        }

                        if (!parser->success) {
                          parser->success = true;
                          { // Match literal "local"
                            if (parser->pos + 5 <= parser->input_len &&
                                memcmp(parser->input + parser->pos, "local",
                                       5) == 0) {
                              parser->pos += 5;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message,
                                      "Expected `"
                                      "local"
                                      "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                        }
                      }

                      if (!parser->success) {
                        parser->success = true;
                        { // Match literal "nil"
                          if (parser->pos + 3 <= parser->input_len &&
                              memcmp(parser->input + parser->pos, "nil", 3) ==
                                  0) {
                            parser->pos += 3;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected `"
                                    "nil"
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                      }
                    }

                    if (!parser->success) {
                      parser->success = true;
                      { // Match literal "not"
                        if (parser->pos + 3 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "not", 3) ==
                                0) {
                          parser->pos += 3;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected `"
                                  "not"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                    }
                  }

                  if (!parser->success) {
                    parser->success = true;
                    { // Match literal "or"
                      if (parser->pos + 2 <= parser->input_len &&
                          memcmp(parser->input + parser->pos, "or", 2) == 0) {
                        parser->pos += 2;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected `"
                                "or"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                }

                if (!parser->success) {
                  parser->success = true;
                  { // Match literal "repeat"
                    if (parser->pos + 6 <= parser->input_len &&
                        memcmp(parser->input + parser->pos, "repeat", 6) == 0) {
                      parser->pos += 6;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected `"
                              "repeat"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                }
              }

              if (!parser->success) {
                parser->success = true;
                { // Match literal "return"
                  if (parser->pos + 6 <= parser->input_len &&
                      memcmp(parser->input + parser->pos, "return", 6) == 0) {
                    parser->pos += 6;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected `"
                            "return"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Match literal "then"
                if (parser->pos + 4 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "then", 4) == 0) {
                  parser->pos += 4;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected `"
                          "then"
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Match literal "true"
              if (parser->pos + 4 <= parser->input_len &&
                  memcmp(parser->input + parser->pos, "true", 4) == 0) {
                parser->pos += 4;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected `"
                        "true"
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Match literal "until"
            if (parser->pos + 5 <= parser->input_len &&
                memcmp(parser->input + parser->pos, "until", 5) == 0) {
              parser->pos += 5;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected `"
                      "until"
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
        }
      }

      if (!parser->success) {
        parser->success = true;
        { // Match literal "while"
          if (parser->pos + 5 <= parser->input_len &&
              memcmp(parser->input + parser->pos, "while", 5) == 0) {
            parser->pos += 5;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected `"
                    "while"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
      }
    }
    if (parser->success) {
      { // Negate (only match if pattern fails)
        REMEMBER_POSITION(parser, pos);

        { // Match character range: "az,AZ,09,__"
          if (parser->pos < parser->input_len &&
              ((parser->input[parser->pos] >= 97 &&
                parser->input[parser->pos] <= 122) ||
               (parser->input[parser->pos] >= 65 &&
                parser->input[parser->pos] <= 90) ||
               (parser->input[parser->pos] >= 48 &&
                parser->input[parser->pos] <= 57) ||
               (parser->input[parser->pos] >= 95 &&
                parser->input[parser->pos] <= 95))) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character in ranges ["
                    "a"
                    " - "
                    "z"
                    ", "
                    "A"
                    " - "
                    "Z"
                    ", "
                    "0"
                    " - "
                    "9"
                    ", "
                    "_"
                    " - "
                    "_"
                    "] at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          // Pattern matched, so negate fails
          RESTORE_POSITION(parser, pos);
          parser->success = false;
#ifdef PGEN_ERRORS
          sprintf(parser->error_message,
                  "Negated pattern unexpectedly matched at position %zu",
                  pos.pos);
#endif
        } else {
          // Pattern failed, so negate succeeds
          parser->success = true;
          RESTORE_POSITION(
              parser,
              pos); // Restore original position (technically not necessary
                    // since failed pattern should make no changes to position)
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "keyword", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "keyword", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_simple_exp(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "simple_exp", start);
#endif

  {                   // Choice
    {                 // Choice
      {               // Choice
        {             // Choice
          {           // Choice
            {         // Choice
              {       // Choice
                {     // Choice
                  {   // Choice
                    { // Sequence with 2 patterns
                      REMEMBER_POSITION(parser, pos);

                      { // Match literal "nil"
                        if (parser->pos + 3 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "nil", 3) ==
                                0) {
                          parser->pos += 3;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected `"
                                  "nil"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (parser->success) {
                        { // Capture Table
                          int initial_stack_size = lua_gettop(parser->L);
                          { // Constant Capture
                            // A constant capture matches the empty string and
                            // produces all given values
                            lua_pushlstring(parser->L, "nil", 3);
                          }

                          if (parser->success) {
                            int new_stack_size = lua_gettop(parser->L);
                            int items_count =
                                new_stack_size - initial_stack_size;
                            int table_position = -items_count - 1;

                            lua_createtable(parser->L, items_count, 0);

                            lua_insert(parser->L, table_position);

                            for (int i = items_count; i >= 1; --i) {
                              lua_rawseti(parser->L, table_position, i);
                              table_position += 1;
                            }
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }

                    if (!parser->success) {
                      parser->success = true;
                      { // Sequence with 2 patterns
                        REMEMBER_POSITION(parser, pos);

                        { // Match literal "false"
                          if (parser->pos + 5 <= parser->input_len &&
                              memcmp(parser->input + parser->pos, "false", 5) ==
                                  0) {
                            parser->pos += 5;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected `"
                                    "false"
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          { // Capture Table
                            int initial_stack_size = lua_gettop(parser->L);
                            { // Constant Capture
                              // A constant capture matches the empty string and
                              // produces all given values
                              lua_pushlstring(parser->L, "boolean", 7);
                              lua_pushnil(parser->L);
                            }

                            if (parser->success) {
                              int new_stack_size = lua_gettop(parser->L);
                              int items_count =
                                  new_stack_size - initial_stack_size;
                              int table_position = -items_count - 1;

                              lua_createtable(parser->L, items_count, 0);

                              lua_insert(parser->L, table_position);

                              for (int i = items_count; i >= 1; --i) {
                                lua_rawseti(parser->L, table_position, i);
                                table_position += 1;
                              }
                            }
                          }
                          if (!parser->success) {
                            RESTORE_POSITION(parser, pos);
                          }
                        }
                      }
                    }
                  }

                  if (!parser->success) {
                    parser->success = true;
                    { // Sequence with 2 patterns
                      REMEMBER_POSITION(parser, pos);

                      { // Match literal "true"
                        if (parser->pos + 4 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "true", 4) ==
                                0) {
                          parser->pos += 4;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected `"
                                  "true"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (parser->success) {
                        { // Capture Table
                          int initial_stack_size = lua_gettop(parser->L);
                          { // Constant Capture
                            // A constant capture matches the empty string and
                            // produces all given values
                            lua_pushlstring(parser->L, "boolean", 7);
                            lua_pushnil(parser->L);
                          }

                          if (parser->success) {
                            int new_stack_size = lua_gettop(parser->L);
                            int items_count =
                                new_stack_size - initial_stack_size;
                            int table_position = -items_count - 1;

                            lua_createtable(parser->L, items_count, 0);

                            lua_insert(parser->L, table_position);

                            for (int i = items_count; i >= 1; --i) {
                              lua_rawseti(parser->L, table_position, i);
                              table_position += 1;
                            }
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }
                  }
                }

                if (!parser->success) {
                  parser->success = true;
                  parse_Number(parser);
                }
              }

              if (!parser->success) {
                parser->success = true;
                parse_String(parser);
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Sequence with 2 patterns
                REMEMBER_POSITION(parser, pos);

                { // Match literal "..."
                  if (parser->pos + 3 <= parser->input_len &&
                      memcmp(parser->input + parser->pos, "...", 3) == 0) {
                    parser->pos += 3;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected `"
                            "..."
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  { // Capture Table
                    int initial_stack_size = lua_gettop(parser->L);
                    { // Constant Capture
                      // A constant capture matches the empty string and
                      // produces all given values
                      lua_pushlstring(parser->L, "vararg", 6);
                    }

                    if (parser->success) {
                      int new_stack_size = lua_gettop(parser->L);
                      int items_count = new_stack_size - initial_stack_size;
                      int table_position = -items_count - 1;

                      lua_createtable(parser->L, items_count, 0);

                      lua_insert(parser->L, table_position);

                      for (int i = items_count; i >= 1; --i) {
                        lua_rawseti(parser->L, table_position, i);
                        table_position += 1;
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            parse_functiondef(parser);
          }
        }

        if (!parser->success) {
          parser->success = true;
          parse_prefixexp(parser);
        }
      }

      if (!parser->success) {
        parser->success = true;
        parse_tableconstructor(parser);
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 4 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "unop", 4);
          }
          if (parser->success) {
            parse_unop(parser);
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_exp(parser);
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "simple_exp", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "simple_exp", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_attnamelist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "attnamelist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 4 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "attnamelist", 11);
      }
      if (parser->success) {
        parse_Name(parser);
        if (parser->success) {
          { // At most 1 repetitions
            size_t rep_count = 0;

            while (rep_count < 1) {
              size_t before_pos = parser->pos;

              {
                parse_attrib(parser);
              }

              if (!parser->success || before_pos == parser->pos) {
                // Break on failure or zero-width match
                parser->success = true;
                break;
              }

              rep_count += 1;
            }
          }
          if (parser->success) {
            { // Zero or more repetitions
              while (true) {
                { // Sequence with 5 patterns
                  REMEMBER_POSITION(parser, pos);

                  parse_ws(parser);
                  if (parser->success) {
                    { // Match single character ","
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 44) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                ","
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (parser->success) {
                      parse_ws(parser);
                      if (parser->success) {
                        parse_Name(parser);
                        if (parser->success) {
                          { // At most 1 repetitions
                            size_t rep_count = 0;

                            while (rep_count < 1) {
                              size_t before_pos = parser->pos;

                              {
                                parse_attrib(parser);
                              }

                              if (!parser->success ||
                                  before_pos == parser->pos) {
                                // Break on failure or zero-width match
                                parser->success = true;
                                break;
                              }

                              rep_count += 1;
                            }
                          }
                        }
                      }
                    }
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }
                if (!parser->success) {
                  break;
                }
              }
              parser->success = true;
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "attnamelist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "attnamelist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_var(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "var", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "var", 3);
      }
      if (parser->success) {
        parse_prefixexp_inner(parser);
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "var", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "var", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_binop(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "binop", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "binop", 5);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          {                                         // Choice
            {                                       // Choice
              {                                     // Choice
                {                                   // Choice
                  {                                 // Choice
                    {                               // Choice
                      {                             // Choice
                        {                           // Choice
                          {                         // Choice
                            {                       // Choice
                              {                     // Choice
                                {                   // Choice
                                  {                 // Choice
                                    {               // Choice
                                      {             // Choice
                                        {           // Choice
                                          {         // Choice
                                            {       // Choice
                                              {     // Choice
                                                {   // Choice
                                                  { // Match literal "or"
                                                    if (parser->pos + 2 <=
                                                            parser->input_len &&
                                                        memcmp(parser->input +
                                                                   parser->pos,
                                                               "or", 2) == 0) {
                                                      parser->pos += 2;
                                                    } else {
#ifdef PGEN_ERRORS
                                                      sprintf(
                                                          parser->error_message,
                                                          "Expected `"
                                                          "or"
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                      parser->success = false;
                                                    }
                                                  }

                                                  if (!parser->success) {
                                                    parser->success = true;
                                                    { // Match literal "and"
                                                      if (parser->pos + 3 <=
                                                              parser
                                                                  ->input_len &&
                                                          memcmp(
                                                              parser->input +
                                                                  parser->pos,
                                                              "and", 3) == 0) {
                                                        parser->pos += 3;
                                                      } else {
#ifdef PGEN_ERRORS
                                                        sprintf(
                                                            parser
                                                                ->error_message,
                                                            "Expected `"
                                                            "and"
                                                            "` at position %zu",
                                                            parser->pos);
#endif
                                                        parser->success = false;
                                                      }
                                                    }
                                                  }
                                                }

                                                if (!parser->success) {
                                                  parser->success = true;
                                                  { // Match literal "<="
                                                    if (parser->pos + 2 <=
                                                            parser->input_len &&
                                                        memcmp(parser->input +
                                                                   parser->pos,
                                                               "<=", 2) == 0) {
                                                      parser->pos += 2;
                                                    } else {
#ifdef PGEN_ERRORS
                                                      sprintf(
                                                          parser->error_message,
                                                          "Expected `"
                                                          "<="
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                      parser->success = false;
                                                    }
                                                  }
                                                }
                                              }

                                              if (!parser->success) {
                                                parser->success = true;
                                                { // Match literal ">="
                                                  if (parser->pos + 2 <=
                                                          parser->input_len &&
                                                      memcmp(parser->input +
                                                                 parser->pos,
                                                             ">=", 2) == 0) {
                                                    parser->pos += 2;
                                                  } else {
#ifdef PGEN_ERRORS
                                                    sprintf(
                                                        parser->error_message,
                                                        "Expected `"
                                                        ">="
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                    parser->success = false;
                                                  }
                                                }
                                              }
                                            }

                                            if (!parser->success) {
                                              parser->success = true;
                                              { // Match literal "<<"
                                                if (parser->pos + 2 <=
                                                        parser->input_len &&
                                                    memcmp(parser->input +
                                                               parser->pos,
                                                           "<<", 2) == 0) {
                                                  parser->pos += 2;
                                                } else {
#ifdef PGEN_ERRORS
                                                  sprintf(parser->error_message,
                                                          "Expected `"
                                                          "<<"
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                  parser->success = false;
                                                }
                                              }
                                            }
                                          }

                                          if (!parser->success) {
                                            parser->success = true;
                                            { // Match literal ">>"
                                              if (parser->pos + 2 <=
                                                      parser->input_len &&
                                                  memcmp(parser->input +
                                                             parser->pos,
                                                         ">>", 2) == 0) {
                                                parser->pos += 2;
                                              } else {
#ifdef PGEN_ERRORS
                                                sprintf(parser->error_message,
                                                        "Expected `"
                                                        ">>"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                parser->success = false;
                                              }
                                            }
                                          }
                                        }

                                        if (!parser->success) {
                                          parser->success = true;
                                          { // Match single character "<"
                                            if (parser->pos <
                                                    parser->input_len &&
                                                parser->input[parser->pos] ==
                                                    60) {
                                              parser->pos++;
                                            } else {
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected character `"
                                                      "<"
                                                      "` at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          }
                                        }
                                      }

                                      if (!parser->success) {
                                        parser->success = true;
                                        { // Match single character ">"
                                          if (parser->pos < parser->input_len &&
                                              parser->input[parser->pos] ==
                                                  62) {
                                            parser->pos++;
                                          } else {
#ifdef PGEN_ERRORS
                                            sprintf(parser->error_message,
                                                    "Expected character `"
                                                    ">"
                                                    "` at position %zu",
                                                    parser->pos);
#endif
                                            parser->success = false;
                                          }
                                        }
                                      }
                                    }

                                    if (!parser->success) {
                                      parser->success = true;
                                      { // Match literal "~="
                                        if (parser->pos + 2 <=
                                                parser->input_len &&
                                            memcmp(parser->input + parser->pos,
                                                   "~=", 2) == 0) {
                                          parser->pos += 2;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected `"
                                                  "~="
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                    }
                                  }

                                  if (!parser->success) {
                                    parser->success = true;
                                    { // Match literal "=="
                                      if (parser->pos + 2 <=
                                              parser->input_len &&
                                          memcmp(parser->input + parser->pos,
                                                 "==", 2) == 0) {
                                        parser->pos += 2;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected `"
                                                "=="
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                  }
                                }

                                if (!parser->success) {
                                  parser->success = true;
                                  { // Match single character "|"
                                    if (parser->pos < parser->input_len &&
                                        parser->input[parser->pos] == 124) {
                                      parser->pos++;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected character `"
                                              "|"
                                              "` at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
                                }
                              }

                              if (!parser->success) {
                                parser->success = true;
                                { // Match single character "~"
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 126) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected character `"
                                            "~"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                              }
                            }

                            if (!parser->success) {
                              parser->success = true;
                              { // Match single character "&"
                                if (parser->pos < parser->input_len &&
                                    parser->input[parser->pos] == 38) {
                                  parser->pos++;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected character `"
                                          "&"
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                            }
                          }

                          if (!parser->success) {
                            parser->success = true;
                            { // Match literal ".."
                              if (parser->pos + 2 <= parser->input_len &&
                                  memcmp(parser->input + parser->pos, "..",
                                         2) == 0) {
                                parser->pos += 2;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected `"
                                        ".."
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                          }
                        }

                        if (!parser->success) {
                          parser->success = true;
                          { // Match literal "\/\/"
                            if (parser->pos + 2 <= parser->input_len &&
                                memcmp(parser->input + parser->pos, "//", 2) ==
                                    0) {
                              parser->pos += 2;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message,
                                      "Expected `"
                                      "//"
                                      "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                        }
                      }

                      if (!parser->success) {
                        parser->success = true;
                        { // Match single character "\/"
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 47) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character `"
                                    "/"
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                      }
                    }

                    if (!parser->success) {
                      parser->success = true;
                      { // Match single character "+"
                        if (parser->pos < parser->input_len &&
                            parser->input[parser->pos] == 43) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character `"
                                  "+"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                    }
                  }

                  if (!parser->success) {
                    parser->success = true;
                    { // Match single character "-"
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 45) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                "-"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                }

                if (!parser->success) {
                  parser->success = true;
                  { // Match single character "*"
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 42) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              "*"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                }
              }

              if (!parser->success) {
                parser->success = true;
                { // Match single character "%"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 37) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "%"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Match single character "^"
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 94) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected character `"
                          "^"
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            // TODO: ensure stack has enough space for push
            lua_pushlstring(parser->L, parser->input + start_pos,
                            capture_length);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "binop", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "binop", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_fieldlist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "fieldlist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 4 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "fields", 6);
      }
      if (parser->success) {
        parse_field(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 2 patterns
                REMEMBER_POSITION(parser, pos);

                parse_fieldsep(parser);
                if (parser->success) {
                  parse_field(parser);
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  parse_fieldsep(parser);
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "fieldlist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "fieldlist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_functioncall(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "functioncall", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "call", 4);
      }
      if (parser->success) {
        parse_prefixexp_inner(parser);
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "functioncall", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "functioncall", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_prefixexp_inner(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "prefixexp_inner", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_primary(parser);
    if (parser->success) {
      { // Zero or more repetitions
        while (true) {
          parse_suffix(parser);
          if (!parser->success) {
            break;
          }
        }
        parser->success = true;
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "prefixexp_inner", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "prefixexp_inner", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_stat(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "stat", start);
#endif

  { // Sequence with 3 patterns
    REMEMBER_POSITION(parser, pos);

    parse_ws(parser);
    if (parser->success) {
      {                             // Choice
        {                           // Choice
          {                         // Choice
            {                       // Choice
              {                     // Choice
                {                   // Choice
                  {                 // Choice
                    {               // Choice
                      {             // Choice
                        {           // Choice
                          {         // Choice
                            {       // Choice
                              {     // Choice
                                {   // Choice
                                  { // Sequence with 2 patterns
                                    REMEMBER_POSITION(parser, pos);

                                    { // Match single character ";"
                                      if (parser->pos < parser->input_len &&
                                          parser->input[parser->pos] == 59) {
                                        parser->pos++;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected character `"
                                                ";"
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                    if (parser->success) {
                                      { // Capture Table
                                        int initial_stack_size =
                                            lua_gettop(parser->L);
                                        { // Constant Capture
                                          // A constant capture matches the
                                          // empty string and produces all given
                                          // values
                                          lua_pushlstring(parser->L, "empty",
                                                          5);
                                        }

                                        if (parser->success) {
                                          int new_stack_size =
                                              lua_gettop(parser->L);
                                          int items_count = new_stack_size -
                                                            initial_stack_size;
                                          int table_position = -items_count - 1;

                                          lua_createtable(parser->L,
                                                          items_count, 0);

                                          lua_insert(parser->L, table_position);

                                          for (int i = items_count; i >= 1;
                                               --i) {
                                            lua_rawseti(parser->L,
                                                        table_position, i);
                                            table_position += 1;
                                          }
                                        }
                                      }
                                      if (!parser->success) {
                                        RESTORE_POSITION(parser, pos);
                                      }
                                    }
                                  }

                                  if (!parser->success) {
                                    parser->success = true;
                                    { // Capture Table
                                      int initial_stack_size =
                                          lua_gettop(parser->L);
                                      { // Sequence with 6 patterns
                                        REMEMBER_POSITION(parser, pos);

                                        { // Constant Capture
                                          // A constant capture matches the
                                          // empty string and produces all given
                                          // values
                                          lua_pushlstring(parser->L, "assign",
                                                          6);
                                        }
                                        if (parser->success) {
                                          parse_varlist(parser);
                                          if (parser->success) {
                                            parse_ws(parser);
                                            if (parser->success) {
                                              { // Match single character "="
                                                if (parser->pos <
                                                        parser->input_len &&
                                                    parser->input[parser
                                                                      ->pos] ==
                                                        61) {
                                                  parser->pos++;
                                                } else {
#ifdef PGEN_ERRORS
                                                  sprintf(parser->error_message,
                                                          "Expected character `"
                                                          "="
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                  parser->success = false;
                                                }
                                              }
                                              if (parser->success) {
                                                parse_ws(parser);
                                                if (parser->success) {
                                                  parse_explist(parser);
                                                }
                                              }
                                            }
                                          }
                                          if (!parser->success) {
                                            RESTORE_POSITION(parser, pos);
                                          }
                                        }
                                      }

                                      if (parser->success) {
                                        int new_stack_size =
                                            lua_gettop(parser->L);
                                        int items_count =
                                            new_stack_size - initial_stack_size;
                                        int table_position = -items_count - 1;

                                        lua_createtable(parser->L, items_count,
                                                        0);

                                        lua_insert(parser->L, table_position);

                                        for (int i = items_count; i >= 1; --i) {
                                          lua_rawseti(parser->L, table_position,
                                                      i);
                                          table_position += 1;
                                        }
                                      }
                                    }
                                  }
                                }

                                if (!parser->success) {
                                  parser->success = true;
                                  parse_functioncall(parser);
                                }
                              }

                              if (!parser->success) {
                                parser->success = true;
                                { // Capture Table
                                  int initial_stack_size =
                                      lua_gettop(parser->L);
                                  { // Sequence with 6 patterns
                                    REMEMBER_POSITION(parser, pos);

                                    { // Constant Capture
                                      // A constant capture matches the empty
                                      // string and produces all given values
                                      lua_pushlstring(parser->L, "label", 5);
                                    }
                                    if (parser->success) {
                                      { // Match literal "::"
                                        if (parser->pos + 2 <=
                                                parser->input_len &&
                                            memcmp(parser->input + parser->pos,
                                                   "::", 2) == 0) {
                                          parser->pos += 2;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected `"
                                                  "::"
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                      if (parser->success) {
                                        parse_ws(parser);
                                        if (parser->success) {
                                          parse_Name(parser);
                                          if (parser->success) {
                                            parse_ws(parser);
                                            if (parser->success) {
                                              { // Match literal "::"
                                                if (parser->pos + 2 <=
                                                        parser->input_len &&
                                                    memcmp(parser->input +
                                                               parser->pos,
                                                           "::", 2) == 0) {
                                                  parser->pos += 2;
                                                } else {
#ifdef PGEN_ERRORS
                                                  sprintf(parser->error_message,
                                                          "Expected `"
                                                          "::"
                                                          "` at position %zu",
                                                          parser->pos);
#endif
                                                  parser->success = false;
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                      if (!parser->success) {
                                        RESTORE_POSITION(parser, pos);
                                      }
                                    }
                                  }

                                  if (parser->success) {
                                    int new_stack_size = lua_gettop(parser->L);
                                    int items_count =
                                        new_stack_size - initial_stack_size;
                                    int table_position = -items_count - 1;

                                    lua_createtable(parser->L, items_count, 0);

                                    lua_insert(parser->L, table_position);

                                    for (int i = items_count; i >= 1; --i) {
                                      lua_rawseti(parser->L, table_position, i);
                                      table_position += 1;
                                    }
                                  }
                                }
                              }
                            }

                            if (!parser->success) {
                              parser->success = true;
                              { // Capture Table
                                int initial_stack_size = lua_gettop(parser->L);
                                { // Sequence with 2 patterns
                                  REMEMBER_POSITION(parser, pos);

                                  { // Constant Capture
                                    // A constant capture matches the empty
                                    // string and produces all given values
                                    lua_pushlstring(parser->L, "break", 5);
                                  }
                                  if (parser->success) {
                                    { // Match literal "break"
                                      if (parser->pos + 5 <=
                                              parser->input_len &&
                                          memcmp(parser->input + parser->pos,
                                                 "break", 5) == 0) {
                                        parser->pos += 5;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected `"
                                                "break"
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                    if (!parser->success) {
                                      RESTORE_POSITION(parser, pos);
                                    }
                                  }
                                }

                                if (parser->success) {
                                  int new_stack_size = lua_gettop(parser->L);
                                  int items_count =
                                      new_stack_size - initial_stack_size;
                                  int table_position = -items_count - 1;

                                  lua_createtable(parser->L, items_count, 0);

                                  lua_insert(parser->L, table_position);

                                  for (int i = items_count; i >= 1; --i) {
                                    lua_rawseti(parser->L, table_position, i);
                                    table_position += 1;
                                  }
                                }
                              }
                            }
                          }

                          if (!parser->success) {
                            parser->success = true;
                            { // Capture Table
                              int initial_stack_size = lua_gettop(parser->L);
                              { // Sequence with 4 patterns
                                REMEMBER_POSITION(parser, pos);

                                { // Constant Capture
                                  // A constant capture matches the empty string
                                  // and produces all given values
                                  lua_pushlstring(parser->L, "goto", 4);
                                }
                                if (parser->success) {
                                  { // Match literal "goto"
                                    if (parser->pos + 4 <= parser->input_len &&
                                        memcmp(parser->input + parser->pos,
                                               "goto", 4) == 0) {
                                      parser->pos += 4;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected `"
                                              "goto"
                                              "` at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
                                  if (parser->success) {
                                    parse_ws(parser);
                                    if (parser->success) {
                                      parse_Name(parser);
                                    }
                                  }
                                  if (!parser->success) {
                                    RESTORE_POSITION(parser, pos);
                                  }
                                }
                              }

                              if (parser->success) {
                                int new_stack_size = lua_gettop(parser->L);
                                int items_count =
                                    new_stack_size - initial_stack_size;
                                int table_position = -items_count - 1;

                                lua_createtable(parser->L, items_count, 0);

                                lua_insert(parser->L, table_position);

                                for (int i = items_count; i >= 1; --i) {
                                  lua_rawseti(parser->L, table_position, i);
                                  table_position += 1;
                                }
                              }
                            }
                          }
                        }

                        if (!parser->success) {
                          parser->success = true;
                          { // Capture Table
                            int initial_stack_size = lua_gettop(parser->L);
                            { // Sequence with 4 patterns
                              REMEMBER_POSITION(parser, pos);

                              { // Constant Capture
                                // A constant capture matches the empty string
                                // and produces all given values
                                lua_pushlstring(parser->L, "do", 2);
                              }
                              if (parser->success) {
                                { // Match literal "do"
                                  if (parser->pos + 2 <= parser->input_len &&
                                      memcmp(parser->input + parser->pos, "do",
                                             2) == 0) {
                                    parser->pos += 2;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected `"
                                            "do"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_block(parser);
                                  if (parser->success) {
                                    { // Match literal "end"
                                      if (parser->pos + 3 <=
                                              parser->input_len &&
                                          memcmp(parser->input + parser->pos,
                                                 "end", 3) == 0) {
                                        parser->pos += 3;
                                      } else {
#ifdef PGEN_ERRORS
                                        sprintf(parser->error_message,
                                                "Expected `"
                                                "end"
                                                "` at position %zu",
                                                parser->pos);
#endif
                                        parser->success = false;
                                      }
                                    }
                                  }
                                }
                                if (!parser->success) {
                                  RESTORE_POSITION(parser, pos);
                                }
                              }
                            }

                            if (parser->success) {
                              int new_stack_size = lua_gettop(parser->L);
                              int items_count =
                                  new_stack_size - initial_stack_size;
                              int table_position = -items_count - 1;

                              lua_createtable(parser->L, items_count, 0);

                              lua_insert(parser->L, table_position);

                              for (int i = items_count; i >= 1; --i) {
                                lua_rawseti(parser->L, table_position, i);
                                table_position += 1;
                              }
                            }
                          }
                        }
                      }

                      if (!parser->success) {
                        parser->success = true;
                        { // Capture Table
                          int initial_stack_size = lua_gettop(parser->L);
                          { // Sequence with 8 patterns
                            REMEMBER_POSITION(parser, pos);

                            { // Constant Capture
                              // A constant capture matches the empty string and
                              // produces all given values
                              lua_pushlstring(parser->L, "while", 5);
                            }
                            if (parser->success) {
                              { // Match literal "while"
                                if (parser->pos + 5 <= parser->input_len &&
                                    memcmp(parser->input + parser->pos, "while",
                                           5) == 0) {
                                  parser->pos += 5;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected `"
                                          "while"
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (parser->success) {
                                parse_ws(parser);
                                if (parser->success) {
                                  parse_exp(parser);
                                  if (parser->success) {
                                    parse_ws(parser);
                                    if (parser->success) {
                                      { // Match literal "do"
                                        if (parser->pos + 2 <=
                                                parser->input_len &&
                                            memcmp(parser->input + parser->pos,
                                                   "do", 2) == 0) {
                                          parser->pos += 2;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected `"
                                                  "do"
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                      if (parser->success) {
                                        parse_block(parser);
                                        if (parser->success) {
                                          { // Match literal "end"
                                            if (parser->pos + 3 <=
                                                    parser->input_len &&
                                                memcmp(parser->input +
                                                           parser->pos,
                                                       "end", 3) == 0) {
                                              parser->pos += 3;
                                            } else {
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected `"
                                                      "end"
                                                      "` at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                              if (!parser->success) {
                                RESTORE_POSITION(parser, pos);
                              }
                            }
                          }

                          if (parser->success) {
                            int new_stack_size = lua_gettop(parser->L);
                            int items_count =
                                new_stack_size - initial_stack_size;
                            int table_position = -items_count - 1;

                            lua_createtable(parser->L, items_count, 0);

                            lua_insert(parser->L, table_position);

                            for (int i = items_count; i >= 1; --i) {
                              lua_rawseti(parser->L, table_position, i);
                              table_position += 1;
                            }
                          }
                        }
                      }
                    }

                    if (!parser->success) {
                      parser->success = true;
                      { // Capture Table
                        int initial_stack_size = lua_gettop(parser->L);
                        { // Sequence with 6 patterns
                          REMEMBER_POSITION(parser, pos);

                          { // Constant Capture
                            // A constant capture matches the empty string and
                            // produces all given values
                            lua_pushlstring(parser->L, "repeat", 6);
                          }
                          if (parser->success) {
                            { // Match literal "repeat"
                              if (parser->pos + 6 <= parser->input_len &&
                                  memcmp(parser->input + parser->pos, "repeat",
                                         6) == 0) {
                                parser->pos += 6;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected `"
                                        "repeat"
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                            if (parser->success) {
                              parse_block(parser);
                              if (parser->success) {
                                { // Match literal "until"
                                  if (parser->pos + 5 <= parser->input_len &&
                                      memcmp(parser->input + parser->pos,
                                             "until", 5) == 0) {
                                    parser->pos += 5;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected `"
                                            "until"
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_ws(parser);
                                  if (parser->success) {
                                    parse_exp(parser);
                                  }
                                }
                              }
                            }
                            if (!parser->success) {
                              RESTORE_POSITION(parser, pos);
                            }
                          }
                        }

                        if (parser->success) {
                          int new_stack_size = lua_gettop(parser->L);
                          int items_count = new_stack_size - initial_stack_size;
                          int table_position = -items_count - 1;

                          lua_createtable(parser->L, items_count, 0);

                          lua_insert(parser->L, table_position);

                          for (int i = items_count; i >= 1; --i) {
                            lua_rawseti(parser->L, table_position, i);
                            table_position += 1;
                          }
                        }
                      }
                    }
                  }

                  if (!parser->success) {
                    parser->success = true;
                    { // Capture Table
                      int initial_stack_size = lua_gettop(parser->L);
                      { // Sequence with 10 patterns
                        REMEMBER_POSITION(parser, pos);

                        { // Constant Capture
                          // A constant capture matches the empty string and
                          // produces all given values
                          lua_pushlstring(parser->L, "if", 2);
                        }
                        if (parser->success) {
                          { // Match literal "if"
                            if (parser->pos + 2 <= parser->input_len &&
                                memcmp(parser->input + parser->pos, "if", 2) ==
                                    0) {
                              parser->pos += 2;
                            } else {
#ifdef PGEN_ERRORS
                              sprintf(parser->error_message,
                                      "Expected `"
                                      "if"
                                      "` at position %zu",
                                      parser->pos);
#endif
                              parser->success = false;
                            }
                          }
                          if (parser->success) {
                            parse_ws(parser);
                            if (parser->success) {
                              parse_exp(parser);
                              if (parser->success) {
                                parse_ws(parser);
                                if (parser->success) {
                                  { // Match literal "then"
                                    if (parser->pos + 4 <= parser->input_len &&
                                        memcmp(parser->input + parser->pos,
                                               "then", 4) == 0) {
                                      parser->pos += 4;
                                    } else {
#ifdef PGEN_ERRORS
                                      sprintf(parser->error_message,
                                              "Expected `"
                                              "then"
                                              "` at position %zu",
                                              parser->pos);
#endif
                                      parser->success = false;
                                    }
                                  }
                                  if (parser->success) {
                                    parse_block(parser);
                                    if (parser->success) {
                                      { // Zero or more repetitions
                                        while (true) {
                                          { // Sequence with 6 patterns
                                            REMEMBER_POSITION(parser, pos);

                                            { // Match literal "elseif"
                                              if (parser->pos + 6 <=
                                                      parser->input_len &&
                                                  memcmp(parser->input +
                                                             parser->pos,
                                                         "elseif", 6) == 0) {
                                                parser->pos += 6;
                                              } else {
#ifdef PGEN_ERRORS
                                                sprintf(parser->error_message,
                                                        "Expected `"
                                                        "elseif"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                parser->success = false;
                                              }
                                            }
                                            if (parser->success) {
                                              parse_ws(parser);
                                              if (parser->success) {
                                                parse_exp(parser);
                                                if (parser->success) {
                                                  parse_ws(parser);
                                                  if (parser->success) {
                                                    { // Match literal "then"
                                                      if (parser->pos + 4 <=
                                                              parser
                                                                  ->input_len &&
                                                          memcmp(
                                                              parser->input +
                                                                  parser->pos,
                                                              "then", 4) == 0) {
                                                        parser->pos += 4;
                                                      } else {
#ifdef PGEN_ERRORS
                                                        sprintf(
                                                            parser
                                                                ->error_message,
                                                            "Expected `"
                                                            "then"
                                                            "` at position %zu",
                                                            parser->pos);
#endif
                                                        parser->success = false;
                                                      }
                                                    }
                                                    if (parser->success) {
                                                      parse_block(parser);
                                                    }
                                                  }
                                                }
                                              }
                                              if (!parser->success) {
                                                RESTORE_POSITION(parser, pos);
                                              }
                                            }
                                          }
                                          if (!parser->success) {
                                            break;
                                          }
                                        }
                                        parser->success = true;
                                      }
                                      if (parser->success) {
                                        { // At most 1 repetitions
                                          size_t rep_count = 0;

                                          while (rep_count < 1) {
                                            size_t before_pos = parser->pos;

                                            {
                                              { // Sequence with 2 patterns
                                                REMEMBER_POSITION(parser, pos);

                                                { // Match literal "else"
                                                  if (parser->pos + 4 <=
                                                          parser->input_len &&
                                                      memcmp(parser->input +
                                                                 parser->pos,
                                                             "else", 4) == 0) {
                                                    parser->pos += 4;
                                                  } else {
#ifdef PGEN_ERRORS
                                                    sprintf(
                                                        parser->error_message,
                                                        "Expected `"
                                                        "else"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                    parser->success = false;
                                                  }
                                                }
                                                if (parser->success) {
                                                  parse_block(parser);
                                                  if (!parser->success) {
                                                    RESTORE_POSITION(parser,
                                                                     pos);
                                                  }
                                                }
                                              }
                                            }

                                            if (!parser->success ||
                                                before_pos == parser->pos) {
                                              // Break on failure or zero-width
                                              // match
                                              parser->success = true;
                                              break;
                                            }

                                            rep_count += 1;
                                          }
                                        }
                                        if (parser->success) {
                                          { // Match literal "end"
                                            if (parser->pos + 3 <=
                                                    parser->input_len &&
                                                memcmp(parser->input +
                                                           parser->pos,
                                                       "end", 3) == 0) {
                                              parser->pos += 3;
                                            } else {
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected `"
                                                      "end"
                                                      "` at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                          if (!parser->success) {
                            RESTORE_POSITION(parser, pos);
                          }
                        }
                      }

                      if (parser->success) {
                        int new_stack_size = lua_gettop(parser->L);
                        int items_count = new_stack_size - initial_stack_size;
                        int table_position = -items_count - 1;

                        lua_createtable(parser->L, items_count, 0);

                        lua_insert(parser->L, table_position);

                        for (int i = items_count; i >= 1; --i) {
                          lua_rawseti(parser->L, table_position, i);
                          table_position += 1;
                        }
                      }
                    }
                  }
                }

                if (!parser->success) {
                  parser->success = true;
                  { // Capture Table
                    int initial_stack_size = lua_gettop(parser->L);
                    { // Sequence with 16 patterns
                      REMEMBER_POSITION(parser, pos);

                      { // Constant Capture
                        // A constant capture matches the empty string and
                        // produces all given values
                        lua_pushlstring(parser->L, "for_num", 7);
                      }
                      if (parser->success) {
                        { // Match literal "for"
                          if (parser->pos + 3 <= parser->input_len &&
                              memcmp(parser->input + parser->pos, "for", 3) ==
                                  0) {
                            parser->pos += 3;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected `"
                                    "for"
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          parse_ws(parser);
                          if (parser->success) {
                            parse_Name(parser);
                            if (parser->success) {
                              parse_ws(parser);
                              if (parser->success) {
                                { // Match single character "="
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 61) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected character `"
                                            "="
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_ws(parser);
                                  if (parser->success) {
                                    parse_exp(parser);
                                    if (parser->success) {
                                      { // Match single character ","
                                        if (parser->pos < parser->input_len &&
                                            parser->input[parser->pos] == 44) {
                                          parser->pos++;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected character `"
                                                  ","
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                      if (parser->success) {
                                        parse_ws(parser);
                                        if (parser->success) {
                                          parse_exp(parser);
                                          if (parser->success) {
                                            { // At most 1 repetitions
                                              size_t rep_count = 0;

                                              while (rep_count < 1) {
                                                size_t before_pos = parser->pos;

                                                {
                                                  { // Sequence with 3 patterns
                                                    REMEMBER_POSITION(parser,
                                                                      pos);

                                                    { // Match single character
                                                      // ","
                                                      if (parser->pos <
                                                              parser
                                                                  ->input_len &&
                                                          parser->input
                                                                  [parser
                                                                       ->pos] ==
                                                              44) {
                                                        parser->pos++;
                                                      } else {
#ifdef PGEN_ERRORS
                                                        sprintf(
                                                            parser
                                                                ->error_message,
                                                            "Expected "
                                                            "character `"
                                                            ","
                                                            "` at position %zu",
                                                            parser->pos);
#endif
                                                        parser->success = false;
                                                      }
                                                    }
                                                    if (parser->success) {
                                                      parse_ws(parser);
                                                      if (parser->success) {
                                                        parse_exp(parser);
                                                      }
                                                      if (!parser->success) {
                                                        RESTORE_POSITION(parser,
                                                                         pos);
                                                      }
                                                    }
                                                  }
                                                }

                                                if (!parser->success ||
                                                    before_pos == parser->pos) {
                                                  // Break on failure or
                                                  // zero-width match
                                                  parser->success = true;
                                                  break;
                                                }

                                                rep_count += 1;
                                              }
                                            }
                                            if (parser->success) {
                                              parse_ws(parser);
                                              if (parser->success) {
                                                { // Match literal "do"
                                                  if (parser->pos + 2 <=
                                                          parser->input_len &&
                                                      memcmp(parser->input +
                                                                 parser->pos,
                                                             "do", 2) == 0) {
                                                    parser->pos += 2;
                                                  } else {
#ifdef PGEN_ERRORS
                                                    sprintf(
                                                        parser->error_message,
                                                        "Expected `"
                                                        "do"
                                                        "` at position %zu",
                                                        parser->pos);
#endif
                                                    parser->success = false;
                                                  }
                                                }
                                                if (parser->success) {
                                                  parse_block(parser);
                                                  if (parser->success) {
                                                    { // Match literal "end"
                                                      if (parser->pos + 3 <=
                                                              parser
                                                                  ->input_len &&
                                                          memcmp(
                                                              parser->input +
                                                                  parser->pos,
                                                              "end", 3) == 0) {
                                                        parser->pos += 3;
                                                      } else {
#ifdef PGEN_ERRORS
                                                        sprintf(
                                                            parser
                                                                ->error_message,
                                                            "Expected `"
                                                            "end"
                                                            "` at position %zu",
                                                            parser->pos);
#endif
                                                        parser->success = false;
                                                      }
                                                    }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }

                    if (parser->success) {
                      int new_stack_size = lua_gettop(parser->L);
                      int items_count = new_stack_size - initial_stack_size;
                      int table_position = -items_count - 1;

                      lua_createtable(parser->L, items_count, 0);

                      lua_insert(parser->L, table_position);

                      for (int i = items_count; i >= 1; --i) {
                        lua_rawseti(parser->L, table_position, i);
                        table_position += 1;
                      }
                    }
                  }
                }
              }

              if (!parser->success) {
                parser->success = true;
                { // Capture Table
                  int initial_stack_size = lua_gettop(parser->L);
                  { // Sequence with 12 patterns
                    REMEMBER_POSITION(parser, pos);

                    { // Constant Capture
                      // A constant capture matches the empty string and
                      // produces all given values
                      lua_pushlstring(parser->L, "for_in", 6);
                    }
                    if (parser->success) {
                      { // Match literal "for"
                        if (parser->pos + 3 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "for", 3) ==
                                0) {
                          parser->pos += 3;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected `"
                                  "for"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (parser->success) {
                        parse_ws(parser);
                        if (parser->success) {
                          parse_namelist(parser);
                          if (parser->success) {
                            parse_ws(parser);
                            if (parser->success) {
                              { // Match literal "in"
                                if (parser->pos + 2 <= parser->input_len &&
                                    memcmp(parser->input + parser->pos, "in",
                                           2) == 0) {
                                  parser->pos += 2;
                                } else {
#ifdef PGEN_ERRORS
                                  sprintf(parser->error_message,
                                          "Expected `"
                                          "in"
                                          "` at position %zu",
                                          parser->pos);
#endif
                                  parser->success = false;
                                }
                              }
                              if (parser->success) {
                                parse_ws(parser);
                                if (parser->success) {
                                  parse_explist(parser);
                                  if (parser->success) {
                                    parse_ws(parser);
                                    if (parser->success) {
                                      { // Match literal "do"
                                        if (parser->pos + 2 <=
                                                parser->input_len &&
                                            memcmp(parser->input + parser->pos,
                                                   "do", 2) == 0) {
                                          parser->pos += 2;
                                        } else {
#ifdef PGEN_ERRORS
                                          sprintf(parser->error_message,
                                                  "Expected `"
                                                  "do"
                                                  "` at position %zu",
                                                  parser->pos);
#endif
                                          parser->success = false;
                                        }
                                      }
                                      if (parser->success) {
                                        parse_block(parser);
                                        if (parser->success) {
                                          { // Match literal "end"
                                            if (parser->pos + 3 <=
                                                    parser->input_len &&
                                                memcmp(parser->input +
                                                           parser->pos,
                                                       "end", 3) == 0) {
                                              parser->pos += 3;
                                            } else {
#ifdef PGEN_ERRORS
                                              sprintf(parser->error_message,
                                                      "Expected `"
                                                      "end"
                                                      "` at position %zu",
                                                      parser->pos);
#endif
                                              parser->success = false;
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                      if (!parser->success) {
                        RESTORE_POSITION(parser, pos);
                      }
                    }
                  }

                  if (parser->success) {
                    int new_stack_size = lua_gettop(parser->L);
                    int items_count = new_stack_size - initial_stack_size;
                    int table_position = -items_count - 1;

                    lua_createtable(parser->L, items_count, 0);

                    lua_insert(parser->L, table_position);

                    for (int i = items_count; i >= 1; --i) {
                      lua_rawseti(parser->L, table_position, i);
                      table_position += 1;
                    }
                  }
                }
              }
            }

            if (!parser->success) {
              parser->success = true;
              { // Capture Table
                int initial_stack_size = lua_gettop(parser->L);
                { // Sequence with 5 patterns
                  REMEMBER_POSITION(parser, pos);

                  { // Constant Capture
                    // A constant capture matches the empty string and produces
                    // all given values
                    lua_pushlstring(parser->L, "function", 8);
                  }
                  if (parser->success) {
                    { // Match literal "function"
                      if (parser->pos + 8 <= parser->input_len &&
                          memcmp(parser->input + parser->pos, "function", 8) ==
                              0) {
                        parser->pos += 8;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected `"
                                "function"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (parser->success) {
                      parse_ws(parser);
                      if (parser->success) {
                        parse_funcname(parser);
                        if (parser->success) {
                          parse_funcbody(parser);
                        }
                      }
                    }
                    if (!parser->success) {
                      RESTORE_POSITION(parser, pos);
                    }
                  }
                }

                if (parser->success) {
                  int new_stack_size = lua_gettop(parser->L);
                  int items_count = new_stack_size - initial_stack_size;
                  int table_position = -items_count - 1;

                  lua_createtable(parser->L, items_count, 0);

                  lua_insert(parser->L, table_position);

                  for (int i = items_count; i >= 1; --i) {
                    lua_rawseti(parser->L, table_position, i);
                    table_position += 1;
                  }
                }
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Capture Table
              int initial_stack_size = lua_gettop(parser->L);
              { // Sequence with 7 patterns
                REMEMBER_POSITION(parser, pos);

                { // Constant Capture
                  // A constant capture matches the empty string and produces
                  // all given values
                  lua_pushlstring(parser->L, "local_function", 14);
                }
                if (parser->success) {
                  { // Match literal "local"
                    if (parser->pos + 5 <= parser->input_len &&
                        memcmp(parser->input + parser->pos, "local", 5) == 0) {
                      parser->pos += 5;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected `"
                              "local"
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      { // Match literal "function"
                        if (parser->pos + 8 <= parser->input_len &&
                            memcmp(parser->input + parser->pos, "function",
                                   8) == 0) {
                          parser->pos += 8;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected `"
                                  "function"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (parser->success) {
                        parse_ws(parser);
                        if (parser->success) {
                          parse_Name(parser);
                          if (parser->success) {
                            parse_funcbody(parser);
                          }
                        }
                      }
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }

              if (parser->success) {
                int new_stack_size = lua_gettop(parser->L);
                int items_count = new_stack_size - initial_stack_size;
                int table_position = -items_count - 1;

                lua_createtable(parser->L, items_count, 0);

                lua_insert(parser->L, table_position);

                for (int i = items_count; i >= 1; --i) {
                  lua_rawseti(parser->L, table_position, i);
                  table_position += 1;
                }
              }
            }
          }
        }

        if (!parser->success) {
          parser->success = true;
          { // Capture Table
            int initial_stack_size = lua_gettop(parser->L);
            { // Sequence with 5 patterns
              REMEMBER_POSITION(parser, pos);

              { // Constant Capture
                // A constant capture matches the empty string and produces all
                // given values
                lua_pushlstring(parser->L, "local", 5);
              }
              if (parser->success) {
                { // Match literal "local"
                  if (parser->pos + 5 <= parser->input_len &&
                      memcmp(parser->input + parser->pos, "local", 5) == 0) {
                    parser->pos += 5;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected `"
                            "local"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    parse_attnamelist(parser);
                    if (parser->success) {
                      { // At most 1 repetitions
                        size_t rep_count = 0;

                        while (rep_count < 1) {
                          size_t before_pos = parser->pos;

                          {
                            { // Sequence with 4 patterns
                              REMEMBER_POSITION(parser, pos);

                              parse_ws(parser);
                              if (parser->success) {
                                { // Match single character "="
                                  if (parser->pos < parser->input_len &&
                                      parser->input[parser->pos] == 61) {
                                    parser->pos++;
                                  } else {
#ifdef PGEN_ERRORS
                                    sprintf(parser->error_message,
                                            "Expected character `"
                                            "="
                                            "` at position %zu",
                                            parser->pos);
#endif
                                    parser->success = false;
                                  }
                                }
                                if (parser->success) {
                                  parse_ws(parser);
                                  if (parser->success) {
                                    parse_explist(parser);
                                  }
                                }
                                if (!parser->success) {
                                  RESTORE_POSITION(parser, pos);
                                }
                              }
                            }
                          }

                          if (!parser->success || before_pos == parser->pos) {
                            // Break on failure or zero-width match
                            parser->success = true;
                            break;
                          }

                          rep_count += 1;
                        }
                      }
                    }
                  }
                }
                if (!parser->success) {
                  RESTORE_POSITION(parser, pos);
                }
              }
            }

            if (parser->success) {
              int new_stack_size = lua_gettop(parser->L);
              int items_count = new_stack_size - initial_stack_size;
              int table_position = -items_count - 1;

              lua_createtable(parser->L, items_count, 0);

              lua_insert(parser->L, table_position);

              for (int i = items_count; i >= 1; --i) {
                lua_rawseti(parser->L, table_position, i);
                table_position += 1;
              }
            }
          }
        }
      }
      if (parser->success) {
        parse_ws(parser);
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "stat", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "stat", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_ws(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "ws", start);
#endif

  { // Zero or more repetitions
    while (true) {
      {   // Choice
        { // Match character set " \t\n\r"
          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 32: /* " " */
            case 9:  /* "\t" */
            case 10: /* "\n" */
            case 13: /* "\r" */
              parser->pos++;
              break;
            default:
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected one of "
                      "\" \\t\\n\\r\""
                      " at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected one of "
                    "\" \\t\\n\\r\""
                    " at position %zu but reached end of input",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (!parser->success) {
          parser->success = true;
          parse_comment(parser);
        }
      }
      if (!parser->success) {
        break;
      }
    }
    parser->success = true;
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "ws", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "ws", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_varlist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "varlist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 3 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "varlist", 7);
      }
      if (parser->success) {
        parse_var(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 4 patterns
                REMEMBER_POSITION(parser, pos);

                parse_ws(parser);
                if (parser->success) {
                  { // Match single character ","
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 44) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              ","
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_var(parser);
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "varlist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "varlist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_funcname(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "funcname", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 4 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "funcname", 8);
      }
      if (parser->success) {
        parse_Name(parser);
        if (parser->success) {
          { // Zero or more repetitions
            while (true) {
              { // Sequence with 4 patterns
                REMEMBER_POSITION(parser, pos);

                parse_ws(parser);
                if (parser->success) {
                  { // Match single character "."
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 46) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              "."
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_Name(parser);
                    }
                  }
                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }
              if (!parser->success) {
                break;
              }
            }
            parser->success = true;
          }
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  { // Sequence with 4 patterns
                    REMEMBER_POSITION(parser, pos);

                    parse_ws(parser);
                    if (parser->success) {
                      { // Match single character ":"
                        if (parser->pos < parser->input_len &&
                            parser->input[parser->pos] == 58) {
                          parser->pos++;
                        } else {
#ifdef PGEN_ERRORS
                          sprintf(parser->error_message,
                                  "Expected character `"
                                  ":"
                                  "` at position %zu",
                                  parser->pos);
#endif
                          parser->success = false;
                        }
                      }
                      if (parser->success) {
                        parse_ws(parser);
                        if (parser->success) {
                          parse_Name(parser);
                        }
                      }
                      if (!parser->success) {
                        RESTORE_POSITION(parser, pos);
                      }
                    }
                  }
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "funcname", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "funcname", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_primary(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "primary", start);
#endif

  { // Choice
    parse_Name(parser);

    if (!parser->success) {
      parser->success = true;
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 6 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "paren", 5);
          }
          if (parser->success) {
            { // Match single character "("
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 40) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected character `"
                        "("
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_exp(parser);
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    { // Match single character ")"
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 41) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                ")"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                }
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "primary", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "primary", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_var_suffix(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "var_suffix", start);
#endif

  {   // Choice
    { // Capture Table
      int initial_stack_size = lua_gettop(parser->L);
      { // Sequence with 7 patterns
        REMEMBER_POSITION(parser, pos);

        { // Constant Capture
          // A constant capture matches the empty string and produces all given
          // values
          lua_pushlstring(parser->L, "index", 5);
        }
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            { // Match single character "["
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 91) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected character `"
                        "["
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_exp(parser);
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    { // Match single character "]"
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 93) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                "]"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                }
              }
            }
          }
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        int new_stack_size = lua_gettop(parser->L);
        int items_count = new_stack_size - initial_stack_size;
        int table_position = -items_count - 1;

        lua_createtable(parser->L, items_count, 0);

        lua_insert(parser->L, table_position);

        for (int i = items_count; i >= 1; --i) {
          lua_rawseti(parser->L, table_position, i);
          table_position += 1;
        }
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 5 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "field", 5);
          }
          if (parser->success) {
            parse_ws(parser);
            if (parser->success) {
              { // Match single character "."
                if (parser->pos < parser->input_len &&
                    parser->input[parser->pos] == 46) {
                  parser->pos++;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected character `"
                          "."
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
              if (parser->success) {
                parse_ws(parser);
                if (parser->success) {
                  parse_Name(parser);
                }
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "var_suffix", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "var_suffix", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_tableconstructor(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "tableconstructor", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 6 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "table", 5);
      }
      if (parser->success) {
        { // Match single character "{"
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 123) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character `"
                    "{"
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  parse_fieldlist(parser);
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                { // Match single character "}"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 125) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            "}"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "tableconstructor", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "tableconstructor", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_suffix(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "suffix", start);
#endif

  { // Choice
    parse_var_suffix(parser);

    if (!parser->success) {
      parser->success = true;
      parse_call_suffix(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "suffix", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "suffix", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_attrib(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "attrib", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_ws(parser);
    if (parser->success) {
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 6 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "attrib", 6);
          }
          if (parser->success) {
            { // Match single character "<"
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 60) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected character `"
                        "<"
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_Name(parser);
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    { // Match single character ">"
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 62) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                ">"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                  }
                }
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "attrib", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "attrib", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_Name(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "Name", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "name", 4);
      }
      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

            { // Negate (only match if pattern fails)
              REMEMBER_POSITION(parser, pos);

              parse_keyword(parser);

              if (parser->success) {
                // Pattern matched, so negate fails
                RESTORE_POSITION(parser, pos);
                parser->success = false;
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Negated pattern unexpectedly matched at position %zu",
                        pos.pos);
#endif
              } else {
                // Pattern failed, so negate succeeds
                parser->success = true;
                RESTORE_POSITION(parser,
                                 pos); // Restore original position (technically
                                       // not necessary since failed pattern
                                       // should make no changes to position)
              }
            }
            if (parser->success) {
              parse_ident(parser);
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }

          if (parser->success) {
            size_t capture_length = parser->pos - start_pos;
            // TODO: ensure stack has enough space for push
            lua_pushlstring(parser->L, parser->input + start_pos,
                            capture_length);
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "Name", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "Name", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_args(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "args", start);
#endif

  {     // Choice
    {   // Choice
      { // Sequence with 5 patterns
        REMEMBER_POSITION(parser, pos);

        { // Match single character "("
          if (parser->pos < parser->input_len &&
              parser->input[parser->pos] == 40) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character `"
                    "("
                    "` at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
        if (parser->success) {
          parse_ws(parser);
          if (parser->success) {
            { // At most 1 repetitions
              size_t rep_count = 0;

              while (rep_count < 1) {
                size_t before_pos = parser->pos;

                {
                  parse_explist(parser);
                }

                if (!parser->success || before_pos == parser->pos) {
                  // Break on failure or zero-width match
                  parser->success = true;
                  break;
                }

                rep_count += 1;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                { // Match single character ")"
                  if (parser->pos < parser->input_len &&
                      parser->input[parser->pos] == 41) {
                    parser->pos++;
                  } else {
#ifdef PGEN_ERRORS
                    sprintf(parser->error_message,
                            "Expected character `"
                            ")"
                            "` at position %zu",
                            parser->pos);
#endif
                    parser->success = false;
                  }
                }
              }
            }
          }
          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (!parser->success) {
        parser->success = true;
        parse_tableconstructor(parser);
      }
    }

    if (!parser->success) {
      parser->success = true;
      parse_String(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "args", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "args", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_chunk(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "chunk", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    parse_block(parser);
    if (parser->success) {
      { // Negate (only match if pattern fails)
        REMEMBER_POSITION(parser, pos);

        { // Match any 1 characters
          if (parser->pos + 1 <= parser->input_len) {
            parser->pos += 1;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected at least 1 more characters at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (parser->success) {
          // Pattern matched, so negate fails
          RESTORE_POSITION(parser, pos);
          parser->success = false;
#ifdef PGEN_ERRORS
          sprintf(parser->error_message,
                  "Negated pattern unexpectedly matched at position %zu",
                  pos.pos);
#endif
        } else {
          // Pattern failed, so negate succeeds
          parser->success = true;
          RESTORE_POSITION(
              parser,
              pos); // Restore original position (technically not necessary
                    // since failed pattern should make no changes to position)
        }
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "chunk", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "chunk", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_ident(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "ident", start);
#endif

  { // Sequence with 2 patterns
    REMEMBER_POSITION(parser, pos);

    { // Match character range: "az,AZ,__"
      if (parser->pos < parser->input_len &&
          ((parser->input[parser->pos] >= 97 &&
            parser->input[parser->pos] <= 122) ||
           (parser->input[parser->pos] >= 65 &&
            parser->input[parser->pos] <= 90) ||
           (parser->input[parser->pos] >= 95 &&
            parser->input[parser->pos] <= 95))) {
        parser->pos++;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected character in ranges ["
                "a"
                " - "
                "z"
                ", "
                "A"
                " - "
                "Z"
                ", "
                "_"
                " - "
                "_"
                "] at position %zu",
                parser->pos);
#endif
        parser->success = false;
      }
    }
    if (parser->success) {
      { // Zero or more repetitions
        while (true) {
          { // Match character range: "az,AZ,09,__"
            if (parser->pos < parser->input_len &&
                ((parser->input[parser->pos] >= 97 &&
                  parser->input[parser->pos] <= 122) ||
                 (parser->input[parser->pos] >= 65 &&
                  parser->input[parser->pos] <= 90) ||
                 (parser->input[parser->pos] >= 48 &&
                  parser->input[parser->pos] <= 57) ||
                 (parser->input[parser->pos] >= 95 &&
                  parser->input[parser->pos] <= 95))) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character in ranges ["
                      "a"
                      " - "
                      "z"
                      ", "
                      "A"
                      " - "
                      "Z"
                      ", "
                      "0"
                      " - "
                      "9"
                      ", "
                      "_"
                      " - "
                      "_"
                      "] at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }
          if (!parser->success) {
            break;
          }
        }
        parser->success = true;
      }
      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "ident", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "ident", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_parlist(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "parlist", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence with 2 patterns
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "params", 6);
      }
      if (parser->success) {
        {   // Choice
          { // Sequence with 2 patterns
            REMEMBER_POSITION(parser, pos);

            parse_namelist(parser);
            if (parser->success) {
              { // At most 1 repetitions
                size_t rep_count = 0;

                while (rep_count < 1) {
                  size_t before_pos = parser->pos;

                  {
                    { // Sequence with 4 patterns
                      REMEMBER_POSITION(parser, pos);

                      parse_ws(parser);
                      if (parser->success) {
                        { // Match single character ","
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 44) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character `"
                                    ","
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          parse_ws(parser);
                          if (parser->success) {
                            { // Match literal "..."
                              if (parser->pos + 3 <= parser->input_len &&
                                  memcmp(parser->input + parser->pos, "...",
                                         3) == 0) {
                                parser->pos += 3;
                              } else {
#ifdef PGEN_ERRORS
                                sprintf(parser->error_message,
                                        "Expected `"
                                        "..."
                                        "` at position %zu",
                                        parser->pos);
#endif
                                parser->success = false;
                              }
                            }
                          }
                        }
                        if (!parser->success) {
                          RESTORE_POSITION(parser, pos);
                        }
                      }
                    }
                  }

                  if (!parser->success || before_pos == parser->pos) {
                    // Break on failure or zero-width match
                    parser->success = true;
                    break;
                  }

                  rep_count += 1;
                }
              }
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }

          if (!parser->success) {
            parser->success = true;
            { // Sequence with 2 patterns
              REMEMBER_POSITION(parser, pos);

              { // Match literal "..."
                if (parser->pos + 3 <= parser->input_len &&
                    memcmp(parser->input + parser->pos, "...", 3) == 0) {
                  parser->pos += 3;
                } else {
#ifdef PGEN_ERRORS
                  sprintf(parser->error_message,
                          "Expected `"
                          "..."
                          "` at position %zu",
                          parser->pos);
#endif
                  parser->success = false;
                }
              }
              if (parser->success) {
                { // Capture Table
                  int initial_stack_size = lua_gettop(parser->L);
                  { // Constant Capture
                    // A constant capture matches the empty string and produces
                    // all given values
                    lua_pushlstring(parser->L, "vararg", 6);
                  }

                  if (parser->success) {
                    int new_stack_size = lua_gettop(parser->L);
                    int items_count = new_stack_size - initial_stack_size;
                    int table_position = -items_count - 1;

                    lua_createtable(parser->L, items_count, 0);

                    lua_insert(parser->L, table_position);

                    for (int i = items_count; i >= 1; --i) {
                      lua_rawseti(parser->L, table_position, i);
                      table_position += 1;
                    }
                  }
                }
                if (!parser->success) {
                  RESTORE_POSITION(parser, pos);
                }
              }
            }
          }
        }
        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      int new_stack_size = lua_gettop(parser->L);
      int items_count = new_stack_size - initial_stack_size;
      int table_position = -items_count - 1;

      lua_createtable(parser->L, items_count, 0);

      lua_insert(parser->L, table_position);

      for (int i = items_count; i >= 1; --i) {
        lua_rawseti(parser->L, table_position, i);
        table_position += 1;
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "parlist", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "parlist", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_field(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "field", start);
#endif

  {     // Choice
    {   // Choice
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 10 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "index_field", 11);
          }
          if (parser->success) {
            { // Match single character "["
              if (parser->pos < parser->input_len &&
                  parser->input[parser->pos] == 91) {
                parser->pos++;
              } else {
#ifdef PGEN_ERRORS
                sprintf(parser->error_message,
                        "Expected character `"
                        "["
                        "` at position %zu",
                        parser->pos);
#endif
                parser->success = false;
              }
            }
            if (parser->success) {
              parse_ws(parser);
              if (parser->success) {
                parse_exp(parser);
                if (parser->success) {
                  parse_ws(parser);
                  if (parser->success) {
                    { // Match single character "]"
                      if (parser->pos < parser->input_len &&
                          parser->input[parser->pos] == 93) {
                        parser->pos++;
                      } else {
#ifdef PGEN_ERRORS
                        sprintf(parser->error_message,
                                "Expected character `"
                                "]"
                                "` at position %zu",
                                parser->pos);
#endif
                        parser->success = false;
                      }
                    }
                    if (parser->success) {
                      parse_ws(parser);
                      if (parser->success) {
                        { // Match single character "="
                          if (parser->pos < parser->input_len &&
                              parser->input[parser->pos] == 61) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character `"
                                    "="
                                    "` at position %zu",
                                    parser->pos);
#endif
                            parser->success = false;
                          }
                        }
                        if (parser->success) {
                          parse_ws(parser);
                          if (parser->success) {
                            parse_exp(parser);
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }

      if (!parser->success) {
        parser->success = true;
        { // Capture Table
          int initial_stack_size = lua_gettop(parser->L);
          { // Sequence with 6 patterns
            REMEMBER_POSITION(parser, pos);

            { // Constant Capture
              // A constant capture matches the empty string and produces all
              // given values
              lua_pushlstring(parser->L, "name_field", 10);
            }
            if (parser->success) {
              parse_Name(parser);
              if (parser->success) {
                parse_ws(parser);
                if (parser->success) {
                  { // Match single character "="
                    if (parser->pos < parser->input_len &&
                        parser->input[parser->pos] == 61) {
                      parser->pos++;
                    } else {
#ifdef PGEN_ERRORS
                      sprintf(parser->error_message,
                              "Expected character `"
                              "="
                              "` at position %zu",
                              parser->pos);
#endif
                      parser->success = false;
                    }
                  }
                  if (parser->success) {
                    parse_ws(parser);
                    if (parser->success) {
                      parse_exp(parser);
                    }
                  }
                }
              }
              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }

          if (parser->success) {
            int new_stack_size = lua_gettop(parser->L);
            int items_count = new_stack_size - initial_stack_size;
            int table_position = -items_count - 1;

            lua_createtable(parser->L, items_count, 0);

            lua_insert(parser->L, table_position);

            for (int i = items_count; i >= 1; --i) {
              lua_rawseti(parser->L, table_position, i);
              table_position += 1;
            }
          }
        }
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence with 2 patterns
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "exp_field", 9);
          }
          if (parser->success) {
            parse_exp(parser);
            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          int new_stack_size = lua_gettop(parser->L);
          int items_count = new_stack_size - initial_stack_size;
          int table_position = -items_count - 1;

          lua_createtable(parser->L, items_count, 0);

          lua_insert(parser->L, table_position);

          for (int i = items_count; i >= 1; --i) {
            lua_rawseti(parser->L, table_position, i);
            table_position += 1;
          }
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "field", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "field", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *lua_parser_init(const char *input, lua_State *L) {
  Parser *parser = (Parser *)malloc(sizeof(Parser));
  if (!parser) {
    // Handle allocation failure if necessary, though often parser might exit
    return NULL;
  }
  parser->input = input;
  parser->input_len = strlen(input);
  parser->pos = 0;
  parser->depth = 0;
  parser->success = true;
  parser->error_message[0] = '\0';
  parser->L = L;
  return parser;
}

// Free parser
static void lua_parser_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_lua_parser_parse(lua_State *L) {
  // Check type and get the input string
  if (!lua_isstring(L, 1)) {
    return luaL_error(L, "Expected string argument for parsing");
  }
  const char *input = lua_tostring(L, 1);
  if (!input) {
    // Should not happen if lua_isstring passed, but good practice
    return luaL_error(L, "Failed to get string argument");
  }

  // Initialize the parser directly
  Parser *parser = lua_parser_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_chunk(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size &&
           "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    lua_parser_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    lua_parser_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  lua_parser_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg lua_parser_module[] = {
    {"parse",
     l_lua_parser_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}          // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_lua_parser(lua_State *L) {
  luaL_newlib(L, lua_parser_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_lua_parser(lua_State *L) {
  luaL_register(L, "lua_parser",
                lua_parser_module); // Registers functions in global table (or
                                    // package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o lua_parser.so -fPIC lua_parser.c `pkg-config --cflags --libs
lua5.1`

To use in Lua:
local lua_parser = require "lua_parser"
local result = lua_parser.parse("your input string")
*/
