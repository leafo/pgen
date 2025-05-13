#define PGEN_ERRORS 1
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// json_parser - generated parser

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
static bool parse_number(Parser *parser);
static bool parse_string(Parser *parser);
static bool parse_ws(Parser *parser);
static bool parse_escape(Parser *parser);
static bool parse_false(Parser *parser);
static bool parse_member(Parser *parser);
static bool parse_hex(Parser *parser);
static bool parse_char(Parser *parser);
static bool parse_true(Parser *parser);
static bool parse_object(Parser *parser);
static bool parse_unicode(Parser *parser);
static bool parse_value(Parser *parser);
static bool parse_array(Parser *parser);
static bool parse_null(Parser *parser);
static bool parse_json(Parser *parser);

// Rule functions
static bool parse_number(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "number", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Constant Capture
        // A constant capture matches the empty string and produces all given
        // values
        lua_pushlstring(parser->L, "number", 6);
      }

      if (parser->success) {
        { // Capture
          size_t start_pos = parser->pos;
          { // Sequence
            REMEMBER_POSITION(parser, pos);

            { // Sequence
              REMEMBER_POSITION(parser, pos);

              { // Sequence
                REMEMBER_POSITION(parser, pos);

                { // At most 1 repetitions
                  size_t rep_count = 0;

                  while (rep_count < 1) {
                    size_t before_pos = parser->pos;

                    {
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

                    if (!parser->success || before_pos == parser->pos) {
                      // Break on failure or zero-width match
                      parser->success = true;
                      break;
                    }

                    rep_count += 1;
                  }
                }

                if (parser->success) {
                  {   // Choice
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

                    if (!parser->success) {
                      parser->success = true;
                      { // Sequence
                        REMEMBER_POSITION(parser, pos);

                        { // Match character range: "19"
                          if (parser->pos < parser->input_len &&
                              ((parser->input[parser->pos] >= 49 &&
                                parser->input[parser->pos] <= 57))) {
                            parser->pos++;
                          } else {
#ifdef PGEN_ERRORS
                            sprintf(parser->error_message,
                                    "Expected character in ranges ["
                                    "1"
                                    " - "
                                    "9"
                                    "] at position %zu",
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
                  }

                  if (!parser->success) {
                    RESTORE_POSITION(parser, pos);
                  }
                }
              }

              if (parser->success) {
                { // At most 1 repetitions
                  size_t rep_count = 0;

                  while (rep_count < 1) {
                    size_t before_pos = parser->pos;

                    {
                      { // Sequence
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

            if (parser->success) {
              { // At most 1 repetitions
                size_t rep_count = 0;

                while (rep_count < 1) {
                  size_t before_pos = parser->pos;

                  {
                    { // Sequence
                      REMEMBER_POSITION(parser, pos);

                      { // Sequence
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
                                            " at position %zu but reached end "
                                            "of input",
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

                          if (!parser->success) {
                            RESTORE_POSITION(parser, pos);
                          }
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
            "", "number", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "number", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_string(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "string", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
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
        { // Capture Table
          int initial_stack_size = lua_gettop(parser->L);
          { // Sequence
            REMEMBER_POSITION(parser, pos);

            { // Constant Capture
              // A constant capture matches the empty string and produces all
              // given values
              lua_pushlstring(parser->L, "string", 6);
            }

            if (parser->success) {
              { // Capture
                size_t start_pos = parser->pos;
                { // Zero or more repetitions
                  while (true) {
                    parse_char(parser);
                    if (!parser->success) {
                      break;
                    }
                  }
                  parser->success = true;
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

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
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

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "string", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "string", parser->pos);
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

static bool parse_escape(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "escape", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Match single character "\\"
      if (parser->pos < parser->input_len && parser->input[parser->pos] == 92) {
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
      {   // Choice
        { // Match character set "\"\\\/bfnrt"
          if (parser->pos < parser->input_len) {
            switch (parser->input[parser->pos]) {
            case 34:  /* "\"" */
            case 92:  /* "\\" */
            case 47:  /* "/" */
            case 98:  /* "b" */
            case 102: /* "f" */
            case 110: /* "n" */
            case 114: /* "r" */
            case 116: /* "t" */
              parser->pos++;
              break;
            default:
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected one of "
                      "\"\\\"\\\\/bfnrt\""
                      " at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected one of "
                    "\"\\\"\\\\/bfnrt\""
                    " at position %zu but reached end of input",
                    parser->pos);
#endif
            parser->success = false;
          }
        }

        if (!parser->success) {
          parser->success = true;
          parse_unicode(parser);
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
            "", "escape", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "escape", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_false(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "false", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Match literal "false"
      if (parser->pos + 5 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "false", 5) == 0) {
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
        { // Sequence
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "boolean", 7);
          }

          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all
              // given values
              lua_pushboolean(parser->L, 0);
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
            "", "false", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "false", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_member(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "member", start);
#endif

  { // Capture Table
    int initial_stack_size = lua_gettop(parser->L);
    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Sequence
          REMEMBER_POSITION(parser, pos);

          { // Sequence
            REMEMBER_POSITION(parser, pos);

            { // Sequence
              REMEMBER_POSITION(parser, pos);

              { // Constant Capture
                // A constant capture matches the empty string and produces all
                // given values
                lua_pushlstring(parser->L, "member", 6);
              }

              if (parser->success) {
                parse_string(parser);

                if (!parser->success) {
                  RESTORE_POSITION(parser, pos);
                }
              }
            }

            if (parser->success) {
              parse_ws(parser);

              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
              }
            }
          }

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

            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          parse_ws(parser);

          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        parse_value(parser);

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
            "", "member", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "member", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_hex(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "hex", start);
#endif

  {     // Choice
    {   // Choice
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
        parser->success = true;
        { // Match character range: "af"
          if (parser->pos < parser->input_len &&
              ((parser->input[parser->pos] >= 97 &&
                parser->input[parser->pos] <= 102))) {
            parser->pos++;
          } else {
#ifdef PGEN_ERRORS
            sprintf(parser->error_message,
                    "Expected character in ranges ["
                    "a"
                    " - "
                    "f"
                    "] at position %zu",
                    parser->pos);
#endif
            parser->success = false;
          }
        }
      }
    }

    if (!parser->success) {
      parser->success = true;
      { // Match character range: "AF"
        if (parser->pos < parser->input_len &&
            ((parser->input[parser->pos] >= 65 &&
              parser->input[parser->pos] <= 70))) {
          parser->pos++;
        } else {
#ifdef PGEN_ERRORS
          sprintf(parser->error_message,
                  "Expected character in ranges ["
                  "A"
                  " - "
                  "F"
                  "] at position %zu",
                  parser->pos);
#endif
          parser->success = false;
        }
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "hex", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "hex", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_char(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "char", start);
#endif

  { // Choice
    parse_escape(parser);

    if (!parser->success) {
      parser->success = true;
      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Negate (only match if pattern fails)
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
                             pos); // Restore original position (technically not
                                   // necessary since failed pattern should make
                                   // no changes to position)
          }
        }

        if (parser->success) {
          { // Sequence
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

              if (!parser->success) {
                RESTORE_POSITION(parser, pos);
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

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "char", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "char", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_true(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "true", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

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

    if (parser->success) {
      { // Capture Table
        int initial_stack_size = lua_gettop(parser->L);
        { // Sequence
          REMEMBER_POSITION(parser, pos);

          { // Constant Capture
            // A constant capture matches the empty string and produces all
            // given values
            lua_pushlstring(parser->L, "boolean", 7);
          }

          if (parser->success) {
            { // Constant Capture
              // A constant capture matches the empty string and produces all
              // given values
              lua_pushboolean(parser->L, 1);
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
            "", "true", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "true", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_object(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "object", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Sequence
          REMEMBER_POSITION(parser, pos);

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

            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          { // Capture Table
            int initial_stack_size = lua_gettop(parser->L);
            { // Sequence
              REMEMBER_POSITION(parser, pos);

              { // Constant Capture
                // A constant capture matches the empty string and produces all
                // given values
                lua_pushlstring(parser->L, "object", 6);
              }

              if (parser->success) {
                { // At most 1 repetitions
                  size_t rep_count = 0;

                  while (rep_count < 1) {
                    size_t before_pos = parser->pos;

                    {
                      { // Sequence
                        REMEMBER_POSITION(parser, pos);

                        parse_member(parser);

                        if (parser->success) {
                          { // Zero or more repetitions
                            while (true) {
                              { // Sequence
                                REMEMBER_POSITION(parser, pos);

                                { // Sequence
                                  REMEMBER_POSITION(parser, pos);

                                  { // Sequence
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

                                      if (!parser->success) {
                                        RESTORE_POSITION(parser, pos);
                                      }
                                    }
                                  }

                                  if (parser->success) {
                                    parse_ws(parser);

                                    if (!parser->success) {
                                      RESTORE_POSITION(parser, pos);
                                    }
                                  }
                                }

                                if (parser->success) {
                                  parse_member(parser);

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

      if (parser->success) {
        parse_ws(parser);

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "object", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "object", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_unicode(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "unicode", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Sequence
          REMEMBER_POSITION(parser, pos);

          { // Match single character "u"
            if (parser->pos < parser->input_len &&
                parser->input[parser->pos] == 117) {
              parser->pos++;
            } else {
#ifdef PGEN_ERRORS
              sprintf(parser->error_message,
                      "Expected character `"
                      "u"
                      "` at position %zu",
                      parser->pos);
#endif
              parser->success = false;
            }
          }

          if (parser->success) {
            parse_hex(parser);

            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          parse_hex(parser);

          if (!parser->success) {
            RESTORE_POSITION(parser, pos);
          }
        }
      }

      if (parser->success) {
        parse_hex(parser);

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

    if (parser->success) {
      parse_hex(parser);

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "unicode", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "unicode", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_value(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "value", start);
#endif

  {           // Choice
    {         // Choice
      {       // Choice
        {     // Choice
          {   // Choice
            { // Choice
              parse_object(parser);

              if (!parser->success) {
                parser->success = true;
                parse_array(parser);
              }
            }

            if (!parser->success) {
              parser->success = true;
              parse_string(parser);
            }
          }

          if (!parser->success) {
            parser->success = true;
            parse_number(parser);
          }
        }

        if (!parser->success) {
          parser->success = true;
          parse_true(parser);
        }
      }

      if (!parser->success) {
        parser->success = true;
        parse_false(parser);
      }
    }

    if (!parser->success) {
      parser->success = true;
      parse_null(parser);
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "value", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "value", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_array(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "array", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        { // Sequence
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
            parse_ws(parser);

            if (!parser->success) {
              RESTORE_POSITION(parser, pos);
            }
          }
        }

        if (parser->success) {
          { // Capture Table
            int initial_stack_size = lua_gettop(parser->L);
            { // Sequence
              REMEMBER_POSITION(parser, pos);

              { // Constant Capture
                // A constant capture matches the empty string and produces all
                // given values
                lua_pushlstring(parser->L, "array", 5);
              }

              if (parser->success) {
                { // At most 1 repetitions
                  size_t rep_count = 0;

                  while (rep_count < 1) {
                    size_t before_pos = parser->pos;

                    {
                      { // Sequence
                        REMEMBER_POSITION(parser, pos);

                        parse_value(parser);

                        if (parser->success) {
                          { // Zero or more repetitions
                            while (true) {
                              { // Sequence
                                REMEMBER_POSITION(parser, pos);

                                { // Sequence
                                  REMEMBER_POSITION(parser, pos);

                                  { // Sequence
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

                                      if (!parser->success) {
                                        RESTORE_POSITION(parser, pos);
                                      }
                                    }
                                  }

                                  if (parser->success) {
                                    parse_ws(parser);

                                    if (!parser->success) {
                                      RESTORE_POSITION(parser, pos);
                                    }
                                  }
                                }

                                if (parser->success) {
                                  parse_value(parser);

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

      if (parser->success) {
        parse_ws(parser);

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
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

      if (!parser->success) {
        RESTORE_POSITION(parser, pos);
      }
    }
  }

#ifdef PGEN_DEBUG
  if (parser->success) {
    fprintf(stderr, "%*sRule %s matched range: %zu-%zu\n", (int)parser->depth,
            "", "array", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "array", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_null(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "null", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Match literal "null"
      if (parser->pos + 4 <= parser->input_len &&
          memcmp(parser->input + parser->pos, "null", 4) == 0) {
        parser->pos += 4;
      } else {
#ifdef PGEN_ERRORS
        sprintf(parser->error_message,
                "Expected `"
                "null"
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
          // A constant capture matches the empty string and produces all given
          // values
          lua_pushlstring(parser->L, "null", 4);
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
            "", "null", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "null", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

static bool parse_json(Parser *parser) {
  size_t start = parser->pos;

#ifdef PGEN_DEBUG
  parser->depth += 1;
  fprintf(stderr, "%*sEntering rule %s at position %zu\n", (int)parser->depth,
          "", "json", start);
#endif

  { // Sequence
    REMEMBER_POSITION(parser, pos);

    { // Sequence
      REMEMBER_POSITION(parser, pos);

      { // Sequence
        REMEMBER_POSITION(parser, pos);

        parse_ws(parser);

        if (parser->success) {
          { // Capture Table
            int initial_stack_size = lua_gettop(parser->L);
            { // Sequence
              REMEMBER_POSITION(parser, pos);

              { // Constant Capture
                // A constant capture matches the empty string and produces all
                // given values
                lua_pushlstring(parser->L, "json", 4);
              }

              if (parser->success) {
                parse_value(parser);

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

      if (parser->success) {
        parse_ws(parser);

        if (!parser->success) {
          RESTORE_POSITION(parser, pos);
        }
      }
    }

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
            "", "json", start, parser->pos);
    fprintf(stderr, "%*s\t%.*s\n", (int)parser->depth, "",
            (int)(parser->pos - start), parser->input + start);
  } else {
    fprintf(stderr, "%*sRule %s failed at position %zu\n", (int)parser->depth,
            "", "json", parser->pos);
  }
  parser->depth -= 1;
#endif

  return parser->success;
}

// Initialize parser
static Parser *json_parser_init(const char *input, lua_State *L) {
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
static void json_parser_free(Parser *parser) {
  // Check for NULL in case _init failed or was called with NULL
  if (parser) {
    free(parser);
  }
}

// --- Lua Module Interface ---

// Lua wrapper function
static int l_json_parser_parse(lua_State *L) {
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
  Parser *parser = json_parser_init(input, L);
  if (!parser) {
    lua_pushnil(L);
    lua_pushstring(L, "Parser initialization failed (memory allocation?)");
    return 2;
  }

  int initial_stack_size = lua_gettop(parser->L);

  parse_json(parser);

  int final_stack_size = lua_gettop(parser->L);

  // Return nil and error message on failure, true on success
  if (!parser->success) {
    assert(final_stack_size == initial_stack_size &&
           "Unexpected stack size change on parse failure.");
    lua_pushnil(L);
    lua_pushstring(L, parser->error_message);
    json_parser_free(parser);
    return 2; // Return nil and error message
  }

  // If stack size has changed, use new items as return values
  if (final_stack_size > initial_stack_size) {
    json_parser_free(parser);
    return final_stack_size - initial_stack_size; // Return new items
  }

  // Success case with no stack change
  lua_pushinteger(L, parser->pos + 1);
  json_parser_free(parser);
  return 1; // Return position of consumed input
}

// Lua module function registration table
static const struct luaL_Reg json_parser_module[] = {
    {"parse",
     l_json_parser_parse}, // Expose l_parsername_parse as "parse" in Lua
    {NULL, NULL}           // Sentinel
};

// Lua module entry point (compatible with Lua 5.1+)
// Note: LUA_VERSION_NUM wasn't defined before 5.1
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
// Lua 5.2+ uses luaL_setfuncs
int luaopen_json_parser(lua_State *L) {
  luaL_newlib(L, json_parser_module); // Creates table and registers functions
  return 1;
}
#else
// Lua 5.1 uses luaL_register
int luaopen_json_parser(lua_State *L) {
  luaL_register(L, "json_parser",
                json_parser_module); // Registers functions in global table (or
                                     // package table)
  return 1;
}
#endif

/*
To compile as a Lua module:
gcc -shared -o json_parser.so -fPIC json_parser.c `pkg-config --cflags --libs
lua5.1`

To use in Lua:
local json_parser = require "json_parser"
local result = json_parser.parse("your input string")
*/
