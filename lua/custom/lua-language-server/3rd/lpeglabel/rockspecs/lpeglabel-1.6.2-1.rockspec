package = "LPegLabel"
version = "1.6.2-1"
source = {
   url = "https://github.com/sqmedeiros/lpeglabel/archive/v1.6.2-1.tar.gz",
   tag = "v1.6.2-1",
   dir = "lpeglabel-1.6.2-1",
}
description = {
   summary = "Parsing Expression Grammars For Lua with Labeled Failures",
   detailed = [[
     LPegLabel is a conservative extension of the LPeg library that provides
     an implementation of Parsing Expression Grammars (PEGs) with labeled failures.
     By using labeled failures we can properly report syntactical errors.
     We can also recover from such errors by describing a grammar rule with
     the same name of a given label.
     LPegLabel also reports the farthest failure position in case of an ordinary failure.
   ]],
   homepage = "https://github.com/sqmedeiros/lpeglabel/",
   maintainer = "Sergio Medeiros <sqmedeiros@gmail.com>",
   license = "MIT/X11"
}
dependencies = {
   "lua >= 5.1",
}
build = {
   type = "builtin",
   modules = {
      lpeglabel = {
         "lplcap.c", "lplcode.c", "lplprint.c", "lpltree.c", "lplvm.c"
      },
      relabel = "relabel.lua"
   }
}
