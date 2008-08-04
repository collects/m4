dnl Use `m4 -DNUL to print a NUL byte surrounded in [], then exit
ifdef(`NUL', `[ ]m4exit`'')dnl
# This file tests m4 behavior on NUL bytes.
dnl Use `m4 -Dm4exit' to test rest of file.  NUL not a number, needs to warn
m4exit(`2 2')dnl
dnl Raw pass-through:
raw: - -
dnl Embedded in quotes:
quoted: `- -'
dnl Embedded in comments:
commented: #- -
dnl Passed through $1, $*, $@:
define(`echo', `.$1.$*.$@.')define(`', `empty')dnl
define(`-', `dash')define(`--', `dashes')dnl
user: echo(- -,`1 1')
dnl Macro name of define:
define(`- -', ``$0': $1')dnl
dnl All macros matching __*__ take no arguments, and never produce NUL.
dnl First argument of builtin:
errprint(`builtin:
')builtin(`- -')dnl
dnl Remaining arguments of builtin:
`builtin:' builtin(`len', - -)
dnl Single-byte delimiter in changecom:
`changecom:' changecom( ,/) echo/changecom(`/',` ')/echo dnl
dnl Multi-byte delimiter in changecom:
 changecom(`- -', ` - ')- -echo  - changecom(`#')
dnl Single-byte delimiter in changequote:
`changequote:' changequote( ,/) echo/changequote`'dnl
changequote(`/',` ')/echo changequote`'dnl
dnl Multi-byte delimiter in changequote:
 changequote(`- -', ` - ')- -echo  - changequote`'
dnl Quotes in trace and dump output:
errprint(`changequote:
')traceon(`dumpdef')dumpdef(`echo'changequote( ,/))changequote`'dnl
traceoff(`dumpdef')dnl
dnl Warning from changeresyntax: not tested yet. No resyntax includes NUL, needs to warn
dnl Macro name in changesyntax:
`changesyntax:' changesyntax(`W+ -')-  - - -(-)`'changesyntax()dnl
dnl Escape in changesyntax:
 changesyntax(`@ ')echo  echo  -- changesyntax(`O= ')dnl
dnl Active in changesyntax:
 changesyntax(`A ') define(` ', `nul') `'changesyntax(`A- ')undefine(` ')
dnl Warning from changesyntax:
errprint(`changesyntax:
')changesyntax( )dnl
dnl Ignored by changesyntax: TODO - support ignored category?
dnl Warning from debugfile: not tested yet. No file name includes NUL, needs to warn
dnl Warning from debugmode: not tested yet. NUL not a valid mode, needs to warn
dnl Warning from decr: not tested yet. NUL not a number, needs to warn
dnl Definition of define:
`define:' define(`body', `- -')body
dnl Undefined argument of defn:
errprint(`defn:
')defn(` - ')dnl
dnl Defined macro name in defn:
`defn:' defn(`- -')dnl
dnl Macro contents in defn:
 defn(`body')
dnl Argument to divert: not tested yet. NUL not a number, needs to warn
dnl Passed through diversion by divert:
divert(`1')`divert:' - -
divert`'undivert(`1')dnl
dnl Divnum takes no arguments, and never produces NUL.
dnl Discarded by dnl: - -
dnl Undefined argument of dumpdef:
errprint(`dumpdef:
')dumpdef(` - ')dnl
dnl Defined macro names in dumpdef:
dumpdef(`- -', `-', `', `- -', `--')dnl
dnl Macro contents in dumpdef:
dumpdef(`body')dnl
dnl Passed through errprint:
errprint(`errprint:' - -, `- -
')dnl
dnl Passed to esyscmd: not tested yet. NUL truncates string, needs to warn
dnl Generated from esyscmd:
`esyscmd:' esyscmd(__program__` -DNUL '__file__) sysval
dnl First argument of eval: not tested yet. NUL not a number, needs to warn
dnl Other arguments of eval: not tested yet. NUL not a number, needs to warn
dnl First argument to format: not tested yet
dnl Invalid specifier in format: not tested yet, needs to warn
dnl Numeric and string arguments to format: not tested yet, needs to warn
dnl Character argument to format: not tested yet, %c semantics needed
dnl Macro name in ifdef, passed through ifdef:
`ifdef:' ifdef(`- -', `yes: - -', `oops: - -')dnl
 ifdef( , `oops: - -', `no: - -')
dnl Compared in ifelse, passed through ifelse:
`ifelse:' ifelse(`-', `- -', `oops', `- -', - -, `yes: - -')
dnl Warning from include: not tested yet. No file name includes NUL, needs to warn
dnl Warning from incr: not tested yet. NUL not a number, needs to warn
dnl Passed through index:
`index:' index(`a b', `b') index(`-', ` ') index(` ', `-')dnl
 index(`                -', `        -')
dnl Defined first argument of indir:
`indir:' indir(`- -', 1 1)dnl
dnl Undefined first argument of indir:
errprint(`indir:
')indir(` - ')dnl
dnl Warning issued via indir:
 define(`  %%', defn(`divnum'))indir(`  %%', `extra')dnl
dnl Other arguments of indir:
 indir(`len', `- -')
dnl Passed through len:
`len:' len( ) len(- -)
dnl Test m4exit separately from m4wrap; see above.
dnl Undefined macro name in m4symbols:
`m4symbols:'m4symbols(` - ', ` - ')dnl
dnl Defined macro name in m4symbols:
 m4symbols(`- -')
dnl Passed through m4wrap:
m4wrap(``m4wrap:' - -
')dnl
dnl Warning from maketemp: not tested yet. No file name includes NUL, needs to warn
dnl Warning from mkdtemp: not tested yet. No file name includes NUL, needs to warn
dnl Warning from mkstemp: not tested yet. No file name includes NUL, needs to warn
dnl Bad regex in patsubst: not tested yet
dnl First argument of patsubst:
`patsubst:' patsubst(`- -', `-', `.')dnl
dnl Matching via meta-character in patsubst:
 patsubst(`- -', `[^-]')dnl
dnl Second argument of patsubst:
 patsubst(`abc', ` b', `-') patsubst(`- -', ` ', `!')dnl
dnl Third argument of patsubst: not tested yet
dnl Syntax argument of patsubst: not tested yet, needs to warn
dnl Replacement via reference in patsubst:
 patsubst(`-- --', `-\(.\)-', `\1-\1')
dnl Defined argument of popdef:
`popdef:' popdef(`- -')ifdef(`- -', `oops', `ok')
dnl Undefined argument of popdef: not tested yet. Should it warn?
dnl Macro name of pushdef:
`pushdef:' pushdef(`- -', `strange: $1')ifdef(`- -', `ok', `oops')`'dnl
dnl Definition of pushdef:
 pushdef(`body', ` - ')body
dnl Bad regex in regexp: not tested yet
dnl First argument of regexp:
`regexp:' regexp(`a b', `b')dnl
dnl Matching via meta-character in regexp:
 regexp(`- -', `[^-]', `!')dnl
dnl Second argument of regexp:
 regexp(`- -', ` ')dnl
dnl Third argument of regexp: not tested yet
dnl Syntax argument of patsubst: not tested yet, needs to warn
dnl Replacement via reference in regexp:
 regexp(`-- --', `-\(.\)-', `\1-\1')
dnl Bad regex in renamesyms: not tested yet
dnl Direct rename via renamesyms: not tested yet
dnl Meta-character rename via renamesyms: not tested yet
dnl Passed through shift:
`shift:' shift(`hi', `- -', - -)
dnl Warning from sinclude: not tested yet. No file name includes NUL, needs to warn
dnl First argument of substr:
`substr:' substr(`-- --', `1', `3')
dnl Other arguments of substr: not tested yet. NUL not a number, needs to warn.
dnl Warning from syncoutput: not tested yet. No mode contains NUL, needs to warn
dnl Passed to syscmd: not tested yet. NUL truncates string, needs to warn
dnl Sysval takes no arguments, and never produces NUL.
dnl Passed to traceoff:
traceoff(`- -', ` ')dnl
dnl Macro name and arguments of traceon:
`traceon:' errprint(`traceon:
')traceon(`- -')indir(`- -', `- -')dnl
dnl Defined text of traceon:
 traceon(`body')body
dnl First argument of translit: not tested yet
dnl Single character in other arguments of translit: not tested yet
dnl Character ranges of translit: not tested yet
dnl Defined argument of undefine:
`undefine:' undefine(`- -')ifdef(`- -', `oops', `ok')
dnl Undefined argument of undefine: not tested yet. Should it warn?
dnl Warning from undivert: not tested yet. No file name or number includes NUL, needs to warn
dnl other modules need to be tested independently
