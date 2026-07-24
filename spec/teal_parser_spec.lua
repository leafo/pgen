describe("teal_parser", function()
  local pgen = require "pgen"
  local teal_parser = pgen.require("spec.parsers.teal_parser")

  local function check_parse(input, success_expected)
    local result = teal_parser.parse(input)
    if success_expected then
      assert.is_not_nil(result, "Expected successful parse for: " .. input)
    else
      assert.is_nil(result, "Expected parse to fail for: " .. input)
    end
    return result
  end

  -- shorthand for a type node holding a single nominal type
  local function nominal(name)
    return {"type", {"nominal", name}}
  end

  describe("lua compatibility", function()
    it("parses plain Lua code", function()
      local result = check_parse("local x = 1 print(x) return x + 1", true)
      assert.same({"block",
        {"local", {"attnamelist", {"name", "x"}}, {"explist", {"exp", {"number", "1"}}}},
        {"call", {"name", "print"}, {"call", {"explist", {"exp", {"prefixexp", {"name", "x"}}}}}},
        {"return", {"explist", {"exp",
          {"prefixexp", {"name", "x"}}, {"binop", "+"}, {"number", "1"}
        }}}
      }, result)
    end)

    it("parses control structures", function()
      local result = check_parse("for i = 1, 10 do if i > 5 then break end end", true)
      assert.same({"block", {"for_num",
        {"name", "i"},
        {"exp", {"number", "1"}},
        {"exp", {"number", "10"}},
        {"block", {"if",
          {"exp", {"prefixexp", {"name", "i"}}, {"binop", ">"}, {"number", "5"}},
          {"block", {"break"}}
        }}
      }}, result)
    end)

    it("parses comments", function()
      local result = check_parse("-- line comment\n--[[ long\ncomment ]]\nreturn 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)
    end)
  end)

  describe("typed locals", function()
    it("parses a simple type annotation", function()
      local result = check_parse("local x: number = 1", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}},
        {"typelist", nominal("number")},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)
    end)

    it("parses annotation without value", function()
      local result = check_parse("local x: string", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}},
        {"typelist", nominal("string")}
      }}, result)
    end)

    it("parses multiple names and types", function()
      local result = check_parse("local a, b: number, string", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "a"}, {"name", "b"}},
        {"typelist", nominal("number"), nominal("string")}
      }}, result)
    end)

    it("parses attributes with types", function()
      local result = check_parse("local x <const>: number = 1", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "x"}, {"attrib", {"name", "const"}}},
        {"typelist", nominal("number")},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)
    end)

    it("parses array types", function()
      local result = check_parse("local t: {string} = {}", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "t"}},
        {"typelist", {"type", {"array", nominal("string")}}},
        {"explist", {"exp", {"table"}}}
      }}, result)
    end)

    it("parses map types", function()
      local result = check_parse("local m: {string: number}", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "m"}},
        {"typelist", {"type", {"map", nominal("string"), nominal("number")}}}
      }}, result)
    end)

    it("parses tuple types", function()
      local result = check_parse("local p: {number, string}", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "p"}},
        {"typelist", {"type", {"tuple", nominal("number"), nominal("string")}}}
      }}, result)
    end)

    it("parses union types", function()
      local result = check_parse("local u: number | nil", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "u"}},
        {"typelist", {"type", {"nominal", "number"}, {"nil"}}}
      }}, result)
    end)

    it("parses dotted nominal types with type arguments", function()
      local result = check_parse("local s: tl.Stack<number>", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "s"}},
        {"typelist", {"type", {"nominal", "tl.Stack", {"typeargs", nominal("number")}}}}
      }}, result)
    end)
  end)

  describe("global declarations", function()
    it("parses typed global", function()
      local result = check_parse("global x: number = 1", true)
      assert.same({"block", {"global",
        {"attnamelist", {"name", "x"}},
        {"typelist", nominal("number")},
        {"explist", {"exp", {"number", "1"}}}
      }}, result)
    end)

    it("parses global with value only", function()
      local result = check_parse("global y = 2", true)
      assert.same({"block", {"global",
        {"attnamelist", {"name", "y"}},
        {"explist", {"exp", {"number", "2"}}}
      }}, result)
    end)

    it("rejects global without type or value", function()
      check_parse("global z", false)
    end)

    it("parses global function", function()
      local result = check_parse("global function greet(name: string) print(name) end", true)
      assert.same({"block", {"global_function", {"name", "greet"}, {"funcbody",
        {"params", {"param", {"name", "name"}, nominal("string")}},
        {"block", {"call", {"name", "print"}, {"call", {"explist", {"exp", {"prefixexp", {"name", "name"}}}}}}}
      }}}, result)
    end)

    it("parses global record", function()
      local result = check_parse("global record Config debug: boolean end", true)
      assert.same({"block", {"global_record", {"name", "Config"}, {"recordbody",
        {"field", {"name", "debug"}, nominal("boolean")}
      }}}, result)
    end)

    it("parses global enum", function()
      local result = check_parse("global enum Level 'low' 'high' end", true)
      assert.same({"block", {"global_enum", {"name", "Level"}, {"enumbody",
        {"string", "'low'"}, {"string", "'high'"}
      }}}, result)
    end)
  end)

  describe("typed functions", function()
    it("parses typed parameters and return type", function()
      local result = check_parse("local function add(a: number, b: number): number return a + b end", true)
      assert.same({"block", {"local_function", {"name", "add"}, {"funcbody",
        {"params",
          {"param", {"name", "a"}, nominal("number")},
          {"param", {"name", "b"}, nominal("number")}},
        {"retlist", {"typelist", nominal("number")}},
        {"block", {"return", {"explist", {"exp",
          {"prefixexp", {"name", "a"}}, {"binop", "+"}, {"prefixexp", {"name", "b"}}
        }}}}
      }}}, result)
    end)

    it("parses optional parameters", function()
      local result = check_parse("local function f(x?: number, y?) end", true)
      assert.same({"block", {"local_function", {"name", "f"}, {"funcbody",
        {"params",
          {"param", {"name", "x"}, "?", nominal("number")},
          {"param", {"name", "y"}, "?"}},
        {"block"}
      }}}, result)
    end)

    it("parses typed vararg", function()
      local result = check_parse("function f(a, ...: string) end", true)
      assert.same({"block", {"function", {"funcname", {"name", "f"}}, {"funcbody",
        {"params",
          {"param", {"name", "a"}},
          {"vararg", nominal("string")}},
        {"block"}
      }}}, result)
    end)

    it("parses generic functions", function()
      local result = check_parse("local function first<T>(xs: {T}): T return xs[1] end", true)
      assert.same({"block", {"local_function", {"name", "first"}, {"funcbody",
        {"typeargs", {"name", "T"}},
        {"params", {"param", {"name", "xs"}, {"type", {"array", nominal("T")}}}},
        {"retlist", {"typelist", nominal("T")}},
        {"block", {"return", {"explist", {"exp",
          {"prefixexp", {"name", "xs"}, {"index", {"exp", {"number", "1"}}}}
        }}}}
      }}}, result)
    end)

    it("parses multiple return types", function()
      local result = check_parse("local function pair(): (number, string) return 1, 'x' end", true)
      assert.same({"block", {"local_function", {"name", "pair"}, {"funcbody",
        {"retlist", {"typelist", nominal("number"), nominal("string")}},
        {"block", {"return", {"explist",
          {"exp", {"number", "1"}},
          {"exp", {"string", "'x'"}}
        }}}
      }}}, result)
    end)

    it("parses variadic return types", function()
      local result = check_parse("local function tail(): string... return 'x' end", true)
      assert.same({"block", {"local_function", {"name", "tail"}, {"funcbody",
        {"retlist", {"typelist", nominal("string")}, "..."},
        {"block", {"return", {"explist", {"exp", {"string", "'x'"}}}}}
      }}}, result)
    end)
  end)

  describe("function types", function()
    it("parses parameter and return types", function()
      local result = check_parse("local f: function(number, string): boolean", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype", "()",
          {"partypes",
            {"partype", nominal("number")},
            {"partype", nominal("string")}},
          {"retlist", {"typelist", nominal("boolean")}}
        }}}
      }}, result)
    end)

    it("parses a bare function type", function()
      local result = check_parse("local f: function", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype"}}}
      }}, result)
    end)

    it("parses named and optional parameter types", function()
      local result = check_parse("local f: function(x: number, y?: string)", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype", "()",
          {"partypes",
            {"partype", {"name", "x"}, nominal("number")},
            {"partype", {"name", "y"}, "?", nominal("string")}}
        }}}
      }}, result)
    end)

    it("parses generic function types with varargs", function()
      local result = check_parse("local f: function<T>(...: T): T...", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype",
          {"typeargs", {"name", "T"}}, "()",
          {"partypes", {"vararg", nominal("T")}},
          {"retlist", {"typelist", nominal("T")}, "..."}
        }}}
      }}, result)
    end)
  end)

  describe("records", function()
    it("parses a simple record", function()
      local result = check_parse([[
        local record Point
          x: number
          y: number
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "Point"}, {"recordbody",
        {"field", {"name", "x"}, nominal("number")},
        {"field", {"name", "y"}, nominal("number")}
      }}}, result)
    end)

    it("parses generic records", function()
      local result = check_parse([[
        local record Stack<T>
          items: {T}
          push: function(Stack<T>, T)
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "Stack"}, {"recordbody",
        {"typeargs", {"name", "T"}},
        {"field", {"name", "items"}, {"type", {"array", nominal("T")}}},
        {"field", {"name", "push"}, {"type", {"functiontype", "()", {"partypes",
          {"partype", {"type", {"nominal", "Stack", {"typeargs", nominal("T")}}}},
          {"partype", nominal("T")}
        }}}}
      }}}, result)
    end)

    it("parses nested records and enums", function()
      local result = check_parse([[
        local record Shape
          record Meta
            version: number
          end
          enum Kind
            "circle"
            "square"
          end
          kind: Kind
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "Shape"}, {"recordbody",
        {"record", {"name", "Meta"}, {"recordbody",
          {"field", {"name", "version"}, nominal("number")}
        }},
        {"enum", {"name", "Kind"}, {"enumbody",
          {"string", '"circle"'}, {"string", '"square"'}
        }},
        {"field", {"name", "kind"}, nominal("Kind")}
      }}}, result)
    end)

    it("parses record entries: userdata, typedef, litkey and metamethod", function()
      local result = check_parse([[
        local record Buffer
          userdata
          type Chunk = {string}
          ["content-type"]: string
          metamethod __len: function(Buffer): integer
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "Buffer"}, {"recordbody",
        {"userdata"},
        {"typedef", {"name", "Chunk"}, {"type", {"array", nominal("string")}}},
        {"field", {"litkey", {"string", '"content-type"'}}, nominal("string")},
        {"metamethod", {"name", "__len"}, {"type", {"functiontype", "()",
          {"partypes", {"partype", nominal("Buffer")}},
          {"retlist", {"typelist", nominal("integer")}}
        }}}
      }}}, result)
    end)

    it("parses interface inheritance and where clauses", function()
      local result = check_parse([[
        local record Circle is Shape where self.kind == "circle"
          radius: number
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "Circle"}, {"recordbody",
        {"interfacelist", {"nominal", "Shape"}},
        {"where", {"exp",
          {"prefixexp", {"name", "self"}, {"field", {"name", "kind"}}},
          {"binop", "=="},
          {"string", '"circle"'}
        }},
        {"field", {"name", "radius"}, nominal("number")}
      }}}, result)
    end)

    it("parses array-backed records", function()
      local result = check_parse("local record Args is {string} end", true)
      assert.same({"block", {"local_record", {"name", "Args"}, {"recordbody",
        {"interfacelist", {"array", nominal("string")}}
      }}}, result)
    end)
  end)

  describe("interfaces", function()
    it("parses an interface", function()
      local result = check_parse([[
        local interface Serializable
          serialize: function(Serializable): string
        end
      ]], true)
      assert.same({"block", {"local_interface", {"name", "Serializable"}, {"recordbody",
        {"field", {"name", "serialize"}, {"type", {"functiontype", "()",
          {"partypes", {"partype", nominal("Serializable")}},
          {"retlist", {"typelist", nominal("string")}}
        }}}
      }}}, result)
    end)

    it("parses global interface with multiple parents", function()
      local result = check_parse("global interface Node is Comparable, Serializable end", true)
      assert.same({"block", {"global_interface", {"name", "Node"}, {"recordbody",
        {"interfacelist", {"nominal", "Comparable"}, {"nominal", "Serializable"}}
      }}}, result)
    end)
  end)

  describe("enums", function()
    it("parses an enum", function()
      local result = check_parse([[
        local enum Direction
          "north"
          "south"
          "east"
          "west"
        end
      ]], true)
      assert.same({"block", {"local_enum", {"name", "Direction"}, {"enumbody",
        {"string", '"north"'},
        {"string", '"south"'},
        {"string", '"east"'},
        {"string", '"west"'}
      }}}, result)
    end)

    it("parses an empty enum", function()
      local result = check_parse("local enum Nothing end", true)
      assert.same({"block", {"local_enum", {"name", "Nothing"}, {"enumbody"}}}, result)
    end)

    it("rejects non-string enum values", function()
      check_parse("local enum Bad 5 end", false)
    end)
  end)

  describe("type declarations", function()
    it("parses a type alias", function()
      local result = check_parse("local type Ages = {string: integer}", true)
      assert.same({"block", {"local_type", {"name", "Ages"},
        {"type", {"map", nominal("string"), nominal("integer")}}
      }}, result)
    end)

    it("parses a require type", function()
      local result = check_parse("local type Point = require('shapes').Point", true)
      assert.same({"block", {"local_type", {"name", "Point"},
        {"typerequire", {"string", "'shapes'"}, {"name", "Point"}}
      }}, result)
    end)

    it("parses inline record and enum types", function()
      local result = check_parse("local type R = record x: number end", true)
      assert.same({"block", {"local_type", {"name", "R"},
        {"newrecord", {"recordbody", {"field", {"name", "x"}, nominal("number")}}}
      }}, result)

      result = check_parse("local type E = enum 'a' 'b' end", true)
      assert.same({"block", {"local_type", {"name", "E"},
        {"newenum", {"enumbody", {"string", "'a'"}, {"string", "'b'"}}}
      }}, result)
    end)

    it("parses global type declarations", function()
      local result = check_parse("global type Alias = Point", true)
      assert.same({"block", {"global_type", {"name", "Alias"}, nominal("Point")}}, result)

      result = check_parse("global type Opaque", true)
      assert.same({"block", {"global_type", {"name", "Opaque"}}}, result)
    end)
  end)

  describe("as and is expressions", function()
    it("parses as casts", function()
      local result = check_parse("return x as number", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"prefixexp", {"name", "x"}},
        {"as", nominal("number")}
      }}}}, result)
    end)

    it("parses multi-value as casts", function()
      local result = check_parse("return f() as (number, string)", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"prefixexp", {"name", "f"}, {"call"}},
        {"as", {"typelist", nominal("number"), nominal("string")}}
      }}}}, result)
    end)

    it("parses as within larger expressions", function()
      local result = check_parse("return x as number + 1", true)
      assert.same({"block", {"return", {"explist", {"exp",
        {"prefixexp", {"name", "x"}},
        {"as", nominal("number")},
        {"binop", "+"},
        {"number", "1"}
      }}}}, result)
    end)

    it("parses is checks", function()
      local result = check_parse("if x is string then return end", true)
      assert.same({"block", {"if",
        {"exp", {"prefixexp", {"name", "x"}}, {"is", nominal("string")}},
        {"block", {"return"}}
      }}, result)
    end)
  end)

  describe("table constructors", function()
    it("parses typed fields", function()
      local result = check_parse("return {x: number = 1, y = 2}", true)
      assert.same({"block", {"return", {"explist", {"exp", {"table", {"fields",
        {"name_field", {"name", "x"}, nominal("number"), {"exp", {"number", "1"}}},
        {"name_field", {"name", "y"}, {"exp", {"number", "2"}}}
      }}}}}}, result)
    end)
  end)

  describe("macroexp", function()
    it("parses local macroexp", function()
      local result = check_parse([[
        local macroexp square(x: number): number
          return x * x
        end
      ]], true)
      assert.same({"block", {"local_macroexp", {"name", "square"}, {"macroexpbody",
        {"params", {"param", {"name", "x"}, nominal("number")}},
        {"retlist", {"typelist", nominal("number")}},
        {"return", {"explist", {"exp",
          {"prefixexp", {"name", "x"}}, {"binop", "*"}, {"prefixexp", {"name", "x"}}
        }}}
      }}}, result)
    end)

    it("parses macroexp record entries", function()
      local result = check_parse([[
        local record M
          double: function(number): number = macroexp(x: number): number
            return x * 2
          end
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "M"}, {"recordbody",
        {"macroexp", {"name", "double"},
          {"functiontype", "()",
            {"partypes", {"partype", nominal("number")}},
            {"retlist", {"typelist", nominal("number")}}},
          {"macroexpbody",
            {"params", {"param", {"name", "x"}, nominal("number")}},
            {"retlist", {"typelist", nominal("number")}},
            {"return", {"explist", {"exp",
              {"prefixexp", {"name", "x"}}, {"binop", "*"}, {"number", "2"}
            }}}}}
      }}}, result)
    end)
  end)

  describe("contextual keywords", function()
    it("allows type-related words as identifiers", function()
      local result = check_parse("local type = 5", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "type"}},
        {"explist", {"exp", {"number", "5"}}}
      }}, result)

      result = check_parse("record = {}", true)
      assert.same({"block", {"assign",
        {"varlist", {"var", {"name", "record"}}},
        {"explist", {"exp", {"table"}}}
      }}, result)
    end)

    it("reserves as, is and global", function()
      check_parse("local as = 1", false)
      check_parse("local is = 1", false)
      check_parse("local global = 1", false)
    end)
  end)

  -- constructs used by the tl compiler sources that the grammar doc omits
  describe("tl source constructs", function()
    it("skips shebang lines", function()
      local result = check_parse("#!/usr/bin/env -S tl run\nreturn 1", true)
      assert.same({"block", {"return", {"explist", {"exp", {"number", "1"}}}}}, result)
    end)

    it("parses interface record entries", function()
      local result = check_parse([[
        local record types
          interface Node
          end
          n: Node
        end
      ]], true)
      assert.same({"block", {"local_record", {"name", "types"}, {"recordbody",
        {"interface", {"name", "Node"}, {"recordbody"}},
        {"field", {"name", "n"}, nominal("Node")}
      }}}, result)
    end)

    it("parses inline array records", function()
      local result = check_parse("local record Output {string} n: integer end", true)
      assert.same({"block", {"local_record", {"name", "Output"}, {"recordbody",
        {"array", nominal("string")},
        {"field", {"name", "n"}, nominal("integer")}
      }}}, result)
    end)

    it("parses standalone and spaced optional markers", function()
      local result = check_parse("local f: function(? boolean | integer)", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype", "()",
          {"partypes", {"partype", "?", {"type", {"nominal", "boolean"}, {"nominal", "integer"}}}}
        }}}
      }}, result)

      result = check_parse("local function g(x ?: number) end", true)
      assert.same({"block", {"local_function", {"name", "g"}, {"funcbody",
        {"params", {"param", {"name", "x"}, "?", nominal("number")}},
        {"block"}
      }}}, result)
    end)

    it("parses variadic parameter types", function()
      local result = check_parse("local f: function(any...): any...", true)
      assert.same({"block", {"local",
        {"attnamelist", {"name", "f"}},
        {"typelist", {"type", {"functiontype", "()",
          {"partypes", {"partype", {"type", {"nominal", "any"}}, "..."}},
          {"retlist", {"typelist", nominal("any")}, "..."}
        }}}
      }}, result)
    end)

    it("parses constrained type arguments", function()
      local result = check_parse("local function f<T is Comparable>(x: T): T return x end", true)
      assert.same({"block", {"local_function", {"name", "f"}, {"funcbody",
        {"typeargs", {"name", "T"}, {"is", nominal("Comparable")}},
        {"params", {"param", {"name", "x"}, nominal("T")}},
        {"retlist", {"typelist", nominal("T")}},
        {"block", {"return", {"explist", {"exp", {"prefixexp", {"name", "x"}}}}}}
      }}}, result)
    end)

    it("parses generic type aliases", function()
      local result = check_parse("local type M<K> = {K: boolean}", true)
      assert.same({"block", {"local_type", {"name", "M"},
        {"typeargs", {"name", "K"}},
        {"type", {"map", nominal("K"), nominal("boolean")}}
      }}, result)
    end)

    it("starts a new statement at a parenthesis on a following line", function()
      local result = check_parse("local t = f(x)\n(t as FunctionType).y = 1", true)
      assert.same({"block",
        {"local", {"attnamelist", {"name", "t"}},
          {"explist", {"exp", {"prefixexp",
            {"name", "f"},
            {"call", {"explist", {"exp", {"prefixexp", {"name", "x"}}}}}
          }}}},
        {"assign",
          {"varlist", {"var",
            {"paren", {"exp", {"prefixexp", {"name", "t"}}, {"as", nominal("FunctionType")}}},
            {"field", {"name", "y"}}}},
          {"explist", {"exp", {"number", "1"}}}}
      }, result)
    end)
  end)

  describe("complex programs", function()
    it("parses a complete Teal module", function()
      local result = check_parse([[
        local record Queue<T>
          items: {T}
          size: integer
        end

        function Queue.new<T>(): Queue<T>
          local self: Queue<T> = {items = {}, size = 0}
          return self
        end

        local function push<T>(q: Queue<T>, v: T)
          q.size = q.size + 1
          q.items[q.size] = v
        end

        global VERSION = "1.0"

        return Queue
      ]], true)

      local tags = {}
      for i = 2, #result do
        tags[#tags + 1] = result[i][1]
      end
      assert.same({"local_record", "function", "local_function", "global", "return"}, tags)
    end)
  end)

  describe("error cases", function()
    it("rejects unclosed record", function()
      check_parse("local record R x: number", false)
    end)

    it("rejects missing type after colon", function()
      check_parse("local x: = 1", false)
    end)

    it("rejects incomplete as cast", function()
      check_parse("return x as", false)
    end)

    it("rejects is without a type", function()
      check_parse("if x is then end", false)
    end)
  end)
end)
