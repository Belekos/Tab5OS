// Separate, sorted dictionaries. Binary search performs no allocation per token.
static const char* const Ckeywords[]={"_Alignas","_Alignof","_Atomic","_Generic","_Noreturn","_Static_assert","_Thread_local","auto","break","case","const","continue","default","do","else","enum","extern","for","goto","if","inline","register","restrict","return","sizeof","static","struct","switch","typedef","union","volatile","while"};
static const char* const Ctypes[]={"FILE","_Bool","_Complex","_Imaginary","bool","char","double","float","int","int16_t","int32_t","int64_t","int8_t","intptr_t","long","ptrdiff_t","short","signed","size_t","uint16_t","uint32_t","uint64_t","uint8_t","uintptr_t","unsigned","va_list","void","wchar_t"};
static const char* const Cbuiltins[]={"EOF","NULL","abort","abs","acos","aligned_alloc","asin","assert","atan","atan2","atexit","atof","atoi","atol","bsearch","calloc","ceil","clock","cos","cosh","difftime","div","errno","exit","exp","fabs","false","fclose","fflush","fgets","floor","fmod","fopen","fprintf","fputs","fread","free","fscanf","fseek","ftell","fwrite","getchar","gmtime","isfinite","isinf","isnan","labs","llabs","localtime","log","log10","malloc","memchr","memcmp","memcpy","memmove","memset","mktime","offsetof","pow","printf","putchar","puts","qsort","rand","realloc","remove","rename","rewind","round","scanf","sin","sinh","snprintf","sprintf","sqrt","srand","sscanf","stderr","stdin","stdout","strcat","strchr","strcmp","strcpy","strftime","strlen","strncat","strncmp","strncpy","strrchr","strstr","strtod","strtok","strtol","strtoll","strtoul","strtoull","tan","tanh","time","true","trunc","va_arg","va_copy","va_end","va_start","vfprintf","vprintf","vsnprintf","vsprintf"};
static const char* const Cppkeywords[]={"alignas","alignof","and","and_eq","asm","auto","bitand","bitor","break","case","catch","class","co_await","co_return","co_yield","compl","concept","const","const_cast","consteval","constexpr","constinit","continue","decltype","default","delete","do","dynamic_cast","else","enum","explicit","export","extern","final","for","friend","goto","if","inline","mutable","namespace","new","noexcept","not","not_eq","operator","or","or_eq","override","private","protected","public","register","reinterpret_cast","requires","return","sizeof","static","static_assert","static_cast","struct","switch","template","this","thread_local","throw","try","typedef","typeid","typename","union","using","virtual","volatile","while","xor","xor_eq"};
static const char* const Cpptypes[]={"FILE","File","HardwareSerial","IPAddress","JsonArray","JsonDocument","JsonObject","Print","SPIClass","Stream","String","TwoWire","WiFiClient","WiFiClientSecure","array","bool","char","char16_t","char32_t","char8_t","deque","double","exception","float","function","ifstream","int","int16_t","int32_t","int64_t","int8_t","intptr_t","istream","list","long","map","ofstream","optional","ostream","pair","ptrdiff_t","set","shared_ptr","short","signed","size_t","span","string","string_view","tuple","uint16_t","uint32_t","uint64_t","uint8_t","uintptr_t","unique_ptr","unordered_map","unordered_set","unsigned","variant","vector","void","wchar_t","weak_ptr"};
static const char* const Cppbuiltins[]={"ESP","F","HIGH","INPUT","INPUT_PULLDOWN","INPUT_PULLUP","LED_BUILTIN","LOW","M5","NULL","OUTPUT","PROGMEM","PSTR","SD","SPI","Serial","Serial1","Serial2","WiFi","Wire","abs","analogRead","analogReadResolution","analogWrite","analogWriteResolution","attachInterrupt","begin","calloc","ceil","cerr","cin","clamp","constrain","cos","cout","delay","delayMicroseconds","detachInterrupt","digitalRead","digitalWrite","empty","end","endl","false","find","floor","forward","fprintf","free","interrupts","make_shared","make_unique","malloc","map","max","memcmp","memcpy","memmove","memset","micros","millis","min","move","noInterrupts","noTone","nullptr","pinMode","pow","printf","puts","realloc","round","scanf","sin","size","snprintf","sort","sqrt","std","strcmp","strcpy","strlen","strncpy","swap","tan","tone","true","yield"};
static const char* const Pythonkeywords[]={"False","None","True","and","as","assert","async","await","break","case","class","continue","def","del","elif","else","except","finally","for","from","global","if","import","in","is","lambda","match","nonlocal","not","or","pass","raise","return","try","while","with","yield"};
static const char* const Pythontypes[]={"AssertionError","AttributeError","BaseException","Exception","IOError","ImportError","IndexError","KeyError","NameError","OSError","RuntimeError","StopIteration","SyntaxError","TypeError","UnicodeError","ValueError","ZeroDivisionError","bool","bytearray","bytes","classmethod","complex","dict","float","frozenset","int","list","memoryview","object","property","range","set","slice","staticmethod","str","super","tuple","type"};
static const char* const Pythonbuiltins[]={"Ellipsis","NotImplemented","__call__","__debug__","__doc__","__enter__","__exit__","__file__","__getitem__","__import__","__init__","__iter__","__len__","__name__","__new__","__next__","__package__","__repr__","__setitem__","__str__","abs","aiter","all","anext","any","ascii","bin","breakpoint","callable","chr","compile","delattr","dir","divmod","enumerate","eval","exec","filter","format","getattr","globals","hasattr","hash","help","hex","id","input","isinstance","issubclass","iter","len","locals","map","max","micropython","min","next","oct","open","ord","pow","print","repr","reversed","round","setattr","sorted","sum","vars","zip"};
static const char* const JavaScriptkeywords[]={"async","await","break","case","catch","class","const","continue","debugger","default","delete","do","else","export","extends","finally","for","function","if","import","in","instanceof","let","new","of","return","static","super","switch","this","throw","try","typeof","var","void","while","with","yield"};
static const char* const JavaScripttypes[]={"Array","ArrayBuffer","BigInt","BigInt64Array","BigUint64Array","Boolean","DataView","Date","Error","EvalError","Float32Array","Float64Array","Function","Int16Array","Int32Array","Int8Array","JSON","Map","Math","Number","Object","Promise","Proxy","RangeError","ReferenceError","RegExp","Set","String","Symbol","SyntaxError","TypeError","URIError","URL","Uint16Array","Uint32Array","Uint8Array","Uint8ClampedArray","WeakMap","WeakSet"};
static const char* const JavaScriptbuiltins[]={"Infinity","NaN","arguments","clearInterval","clearTimeout","console","decodeURI","decodeURIComponent","document","encodeURI","encodeURIComponent","escape","eval","exports","false","fetch","globalThis","isFinite","isNaN","module","null","parseFloat","parseInt","queueMicrotask","require","self","setInterval","setTimeout","true","undefined","window"};
static const char* const TypeScriptkeywords[]={"abstract","any","as","asserts","async","await","break","case","catch","class","const","constructor","continue","debugger","declare","default","delete","do","else","enum","export","extends","finally","for","from","function","get","if","implements","import","in","infer","instanceof","interface","is","keyof","let","namespace","never","new","of","override","private","protected","public","readonly","return","satisfies","set","static","super","switch","this","throw","try","type","typeof","unique","unknown","var","void","while","with","yield"};
static const char* const TypeScripttypes[]={"Array","ArrayBuffer","Boolean","Date","Error","Exclude","Extract","Function","JSON","Map","Math","NonNullable","Number","Object","Omit","Parameters","Partial","Pick","Promise","ReadonlyArray","Record","RegExp","Required","ReturnType","Set","String","Symbol","URL","Uint8Array","bigint","boolean","null","number","object","string","symbol","undefined"};
static const char* const TypeScriptbuiltins[]={"Infinity","NaN","arguments","clearInterval","clearTimeout","console","decodeURI","decodeURIComponent","document","encodeURI","encodeURIComponent","exports","false","fetch","globalThis","isFinite","isNaN","module","null","parseFloat","parseInt","require","self","setInterval","setTimeout","true","undefined","window"};
static const char* const Rustkeywords[]={"Self","abstract","as","async","await","become","box","break","const","continue","crate","do","dyn","else","enum","extern","final","fn","for","if","impl","in","let","loop","macro","match","mod","move","mut","override","priv","pub","ref","return","self","static","struct","super","trait","try","type","typeof","unsafe","unsized","use","virtual","where","while","yield"};
static const char* const Rusttypes[]={"Arc","Box","Cell","Clone","Copy","Debug","Default","Display","HashMap","HashSet","IntoIterator","Iterator","Option","Rc","RefCell","Result","Send","Sized","String","Sync","Vec","bool","char","f32","f64","i128","i16","i32","i64","i8","isize","str","u128","u16","u32","u64","u8","usize"};
static const char* const Rustbuiltins[]={"Err","None","Ok","Some","assert","assert_eq","assert_ne","cfg","column","concat","dbg","debug_assert","env","eprint","eprintln","false","file","format","include","include_bytes","include_str","line","option_env","panic","print","println","stringify","todo","true","unimplemented","unreachable","vec"};
static const char* const Gokeywords[]={"break","case","chan","const","continue","default","defer","else","fallthrough","for","func","go","goto","if","import","interface","map","package","range","return","select","struct","switch","type","var"};
static const char* const Gotypes[]={"any","bool","byte","comparable","complex128","complex64","error","float32","float64","int","int16","int32","int64","int8","rune","string","uint","uint16","uint32","uint64","uint8","uintptr"};
static const char* const Gobuiltins[]={"append","cap","clear","close","complex","copy","delete","false","imag","iota","len","make","max","min","new","nil","panic","print","println","real","recover","true"};
static const char* const Javakeywords[]={"abstract","assert","break","case","catch","class","const","continue","default","do","else","enum","exports","extends","final","finally","for","goto","if","implements","import","instanceof","interface","module","native","new","non-sealed","open","opens","package","permits","private","protected","provides","public","record","requires","return","sealed","static","strictfp","super","switch","synchronized","this","throw","throws","to","transient","transitive","try","uses","var","volatile","while","with","yield"};
static const char* const Javatypes[]={"ArrayList","Boolean","Byte","Character","Class","Double","Exception","Float","HashMap","HashSet","IOException","Integer","List","Long","Map","Math","Object","Optional","RuntimeException","Set","Short","Stream","String","StringBuilder","System","Throwable","boolean","byte","char","double","float","int","long","short","void"};
static const char* const Javabuiltins[]={"System","err","false","in","null","out","super","this","true"};
static const char* const Kotlinkeywords[]={"abstract","actual","annotation","as","break","by","catch","class","companion","const","constructor","continue","crossinline","data","delegate","do","dynamic","else","enum","expect","external","field","file","final","finally","for","fun","get","if","import","in","infix","init","inline","inner","interface","internal","is","lateinit","noinline","object","open","operator","out","override","package","param","private","property","protected","public","receiver","reified","return","sealed","set","setparam","super","suspend","tailrec","this","throw","try","typealias","typeof","val","var","vararg","when","where","while"};
static const char* const Kotlintypes[]={"Any","Array","Boolean","Byte","Char","Double","Float","Int","List","Long","Map","MutableList","MutableMap","MutableSet","Nothing","Number","Pair","Result","Sequence","Set","Short","String","Triple","Unit"};
static const char* const Kotlinbuiltins[]={"TODO","also","apply","arrayOf","check","error","false","lazy","let","listOf","mapOf","mutableListOf","mutableMapOf","null","print","println","readLine","repeat","require","run","setOf","takeIf","takeUnless","true","with"};
static const char* const Swiftkeywords[]={"Self","actor","any","as","associatedtype","async","await","borrowing","break","case","catch","class","consuming","continue","convenience","default","defer","deinit","didSet","distributed","do","dynamic","else","enum","extension","fallthrough","fileprivate","final","for","func","get","guard","if","import","in","indirect","infix","init","inout","internal","is","isolated","lazy","let","macro","mutating","nonisolated","nonmutating","open","operator","optional","override","package","postfix","precedencegroup","prefix","private","protocol","public","repeat","required","rethrows","return","self","set","some","static","struct","subscript","super","switch","throw","throws","try","typealias","unowned","var","weak","where","while","willSet"};
static const char* const Swifttypes[]={"Any","AnyObject","Array","Bool","Character","Dictionary","Double","Error","Float","Int","Int16","Int32","Int64","Int8","Never","Optional","Result","Set","String","UInt","UInt16","UInt32","UInt64","UInt8","Void"};
static const char* const Swiftbuiltins[]={"abs","assert","assertionFailure","debugPrint","dump","false","fatalError","max","min","nil","precondition","print","sequence","stride","true","type","zip"};
static const char* const Luakeywords[]={"and","break","do","else","elseif","end","for","function","goto","if","in","local","not","or","repeat","return","then","until","while"};
static const char* const Luabuiltins[]={"_G","_VERSION","assert","collectgarbage","coroutine","debug","dofile","error","false","getmetatable","io","ipairs","load","loadfile","math","next","nil","os","package","pairs","pcall","print","rawequal","rawget","rawlen","rawset","require","select","setmetatable","string","table","tonumber","tostring","true","type","utf8","xpcall"};
static const char* const Rubykeywords[]={"BEGIN","END","alias","and","begin","break","case","class","def","defined","do","else","elsif","end","ensure","for","if","in","module","next","not","or","redo","rescue","retry","return","self","super","then","undef","unless","until","when","while","yield"};
static const char* const Rubytypes[]={"Array","BasicObject","Class","Complex","Dir","Encoding","Enumerable","Enumerator","Exception","File","Float","Hash","IO","Integer","Kernel","Math","Method","Module","NilClass","Numeric","Object","Proc","Range","Rational","Regexp","String","Struct","Symbol","Thread","Time"};
static const char* const Rubybuiltins[]={"__ENCODING__","__FILE__","__LINE__","attr_accessor","attr_reader","attr_writer","extend","fail","false","gets","include","lambda","load","loop","nil","p","prepend","print","printf","private","proc","protected","public","puts","raise","require","require_relative","true"};
static const char* const Phpkeywords[]={"abstract","and","array","as","break","callable","case","catch","class","clone","const","continue","declare","default","die","do","echo","else","elseif","empty","enddeclare","endfor","endforeach","endif","endswitch","endwhile","enum","eval","exit","extends","final","finally","fn","for","foreach","function","global","goto","if","implements","include","include_once","instanceof","insteadof","interface","isset","list","match","namespace","new","or","print","private","protected","public","readonly","require","require_once","return","static","switch","throw","trait","try","unset","use","var","while","xor","yield"};
static const char* const Phptypes[]={"array","bool","boolean","double","float","int","integer","iterable","mixed","never","object","parent","self","string","void"};
static const char* const Phpbuiltins[]={"FALSE","NULL","TRUE","__CLASS__","__DIR__","__FILE__","__FUNCTION__","__LINE__","__METHOD__","__NAMESPACE__","__TRAIT__","array_filter","array_map","array_merge","count","empty","explode","false","fclose","file_get_contents","file_put_contents","fopen","fread","fwrite","implode","in_array","isset","json_decode","json_encode","null","printf","sprintf","str_replace","strlen","strpos","substr","true"};
static const char* const Shellkeywords[]={"case","coproc","do","done","elif","else","esac","fi","for","function","if","in","select","then","time","until","while"};
static const char* const Shellbuiltins[]={"alias","bg","bind","break","builtin","cd","command","compgen","complete","continue","declare","dirs","disown","echo","enable","eval","exec","exit","export","false","fc","fg","getopts","hash","help","history","jobs","kill","let","local","logout","mapfile","popd","printf","pushd","pwd","read","readarray","readonly","return","set","shift","shopt","source","test","times","trap","true","type","typeset","ulimit","umask","unalias","unset","wait"};
static const char* const PowerShellkeywords[]={"begin","break","catch","class","clean","continue","data","define","do","dynamicparam","else","elseif","end","enum","exit","filter","finally","for","foreach","from","function","hidden","if","in","parallel","param","process","return","sequence","static","switch","throw","trap","try","until","using","var","while","workflow"};
static const char* const PowerShelltypes[]={"bool","boolean","byte","char","datetime","decimal","double","float","guid","hashtable","int","int16","int32","int64","ipaddress","long","object","pscredential","pscustomobject","psobject","regex","sbyte","scriptblock","securestring","short","single","string","switch","timespan","type","uint16","uint32","uint64","ulong","ushort","void"};
static const char* const PowerShellbuiltins[]={"Error","HOME","Host","LastExitCode","Matches","MyInvocation","PID","PROFILE","PSBoundParameters","PSCommandPath","PSCulture","PSDebugContext","PSEdition","PSHOME","PSScriptRoot","PSVersionTable","args","false","foreach","input","null","pwd","switch","this","true"};
static const char* const Sqlkeywords[]={"ADD","ALL","ALTER","AND","ANY","AS","ASC","BEGIN","BETWEEN","BY","CASE","CHECK","COLLATE","COLUMN","COMMIT","CONSTRAINT","CREATE","CROSS","CURRENT_DATE","CURRENT_TIME","CURRENT_TIMESTAMP","DATABASE","DEFAULT","DELETE","DESC","DISTINCT","DROP","ELSE","END","ESCAPE","EXCEPT","EXISTS","EXPLAIN","FALSE","FETCH","FOREIGN","FROM","FULL","GROUP","HAVING","IF","IN","INDEX","INNER","INSERT","INTERSECT","INTO","IS","JOIN","KEY","LEFT","LIKE","LIMIT","NATURAL","NOT","NULL","OFFSET","ON","OR","ORDER","OUTER","OVER","PARTITION","PRIMARY","REFERENCES","RETURNING","RIGHT","ROLLBACK","ROW","ROWS","SELECT","SET","TABLE","THEN","TO","TRANSACTION","TRIGGER","TRUE","UNION","UNIQUE","UPDATE","USING","VALUES","VIEW","WHEN","WHERE","WITH"};
static const char* const Sqltypes[]={"BIGINT","BINARY","BIT","BLOB","BOOLEAN","CHAR","CHARACTER","CLOB","DATE","DATETIME","DECIMAL","DOUBLE","FLOAT","INT","INTEGER","INTERVAL","JSON","NCHAR","NUMERIC","REAL","SMALLINT","TEXT","TIME","TIMESTAMP","TINYINT","UUID","VARCHAR"};
static const char* const Sqlbuiltins[]={"ABS","AVG","CAST","CEIL","COALESCE","CONCAT","COUNT","CURRENT_USER","DENSE_RANK","EXTRACT","FLOOR","IFNULL","LAG","LEAD","LENGTH","LOWER","MAX","MIN","MOD","NOW","NULLIF","RANK","REPLACE","ROUND","ROW_NUMBER","SQRT","SUBSTR","SUBSTRING","SUM","TRIM","UPPER"};
static const char* const CMakekeywords[]={"block","break","continue","else","elseif","endblock","endforeach","endfunction","endif","endmacro","endwhile","foreach","function","if","macro","return","while"};
static const char* const CMakebuiltins[]={"add_custom_command","add_custom_target","add_executable","add_library","add_subdirectory","add_test","cmake_minimum_required","configure_file","enable_testing","export","file","find_file","find_library","find_package","find_path","find_program","get_property","get_target_property","include","include_directories","install","link_directories","list","message","option","project","set","set_property","set_target_properties","string","target_compile_definitions","target_compile_features","target_compile_options","target_include_directories","target_link_libraries","target_sources","unset"};
static const char* const Csskeywords[]={"auto","currentColor","important","inherit","initial","none","normal","revert","transparent","unset"};
static const char* const Cssbuiltins[]={"attr","calc","clamp","color","color-mix","conic-gradient","env","hsl","hsla","hwb","lab","lch","linear-gradient","max","min","oklab","oklch","radial-gradient","repeat","rgb","rgba","url","var"};
static const char* const Yamlkeywords[]={"false","null","true"};
static const char* const Tomlkeywords[]={"false","true"};
static const char* const Jsonkeywords[]={"false","null","true"};
static const char* const JsonCommentskeywords[]={"false","null","true"};
static const char* const Makefilekeywords[]={"define","else","endef","endif","export","ifdef","ifeq","ifndef","ifneq","include","override","private","unexport","vpath"};
static const char* const Makefilebuiltins[]={"abspath","addprefix","addsuffix","and","basename","call","dir","error","eval","filter","filter-out","findstring","firstword","flavor","foreach","if","info","join","lastword","notdir","or","origin","patsubst","realpath","shell","sort","strip","subst","suffix","value","warning","wildcard","word","wordlist","words"};
struct Vocabulary { const char* const* words=nullptr; size_t count=0; };
Vocabulary vocabulary(SyntaxLanguage language, SyntaxToken token){
 switch(language){
 case SyntaxLanguage::C: switch(token){
 case SyntaxToken::Keyword: return {Ckeywords,sizeof(Ckeywords)/sizeof(Ckeywords[0])};
 case SyntaxToken::Type: return {Ctypes,sizeof(Ctypes)/sizeof(Ctypes[0])};
 case SyntaxToken::Builtin: return {Cbuiltins,sizeof(Cbuiltins)/sizeof(Cbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Cpp: switch(token){
 case SyntaxToken::Keyword: return {Cppkeywords,sizeof(Cppkeywords)/sizeof(Cppkeywords[0])};
 case SyntaxToken::Type: return {Cpptypes,sizeof(Cpptypes)/sizeof(Cpptypes[0])};
 case SyntaxToken::Builtin: return {Cppbuiltins,sizeof(Cppbuiltins)/sizeof(Cppbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Python: switch(token){
 case SyntaxToken::Keyword: return {Pythonkeywords,sizeof(Pythonkeywords)/sizeof(Pythonkeywords[0])};
 case SyntaxToken::Type: return {Pythontypes,sizeof(Pythontypes)/sizeof(Pythontypes[0])};
 case SyntaxToken::Builtin: return {Pythonbuiltins,sizeof(Pythonbuiltins)/sizeof(Pythonbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::JavaScript: switch(token){
 case SyntaxToken::Keyword: return {JavaScriptkeywords,sizeof(JavaScriptkeywords)/sizeof(JavaScriptkeywords[0])};
 case SyntaxToken::Type: return {JavaScripttypes,sizeof(JavaScripttypes)/sizeof(JavaScripttypes[0])};
 case SyntaxToken::Builtin: return {JavaScriptbuiltins,sizeof(JavaScriptbuiltins)/sizeof(JavaScriptbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::TypeScript: switch(token){
 case SyntaxToken::Keyword: return {TypeScriptkeywords,sizeof(TypeScriptkeywords)/sizeof(TypeScriptkeywords[0])};
 case SyntaxToken::Type: return {TypeScripttypes,sizeof(TypeScripttypes)/sizeof(TypeScripttypes[0])};
 case SyntaxToken::Builtin: return {TypeScriptbuiltins,sizeof(TypeScriptbuiltins)/sizeof(TypeScriptbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Rust: switch(token){
 case SyntaxToken::Keyword: return {Rustkeywords,sizeof(Rustkeywords)/sizeof(Rustkeywords[0])};
 case SyntaxToken::Type: return {Rusttypes,sizeof(Rusttypes)/sizeof(Rusttypes[0])};
 case SyntaxToken::Builtin: return {Rustbuiltins,sizeof(Rustbuiltins)/sizeof(Rustbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Go: switch(token){
 case SyntaxToken::Keyword: return {Gokeywords,sizeof(Gokeywords)/sizeof(Gokeywords[0])};
 case SyntaxToken::Type: return {Gotypes,sizeof(Gotypes)/sizeof(Gotypes[0])};
 case SyntaxToken::Builtin: return {Gobuiltins,sizeof(Gobuiltins)/sizeof(Gobuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Java: switch(token){
 case SyntaxToken::Keyword: return {Javakeywords,sizeof(Javakeywords)/sizeof(Javakeywords[0])};
 case SyntaxToken::Type: return {Javatypes,sizeof(Javatypes)/sizeof(Javatypes[0])};
 case SyntaxToken::Builtin: return {Javabuiltins,sizeof(Javabuiltins)/sizeof(Javabuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Kotlin: switch(token){
 case SyntaxToken::Keyword: return {Kotlinkeywords,sizeof(Kotlinkeywords)/sizeof(Kotlinkeywords[0])};
 case SyntaxToken::Type: return {Kotlintypes,sizeof(Kotlintypes)/sizeof(Kotlintypes[0])};
 case SyntaxToken::Builtin: return {Kotlinbuiltins,sizeof(Kotlinbuiltins)/sizeof(Kotlinbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Swift: switch(token){
 case SyntaxToken::Keyword: return {Swiftkeywords,sizeof(Swiftkeywords)/sizeof(Swiftkeywords[0])};
 case SyntaxToken::Type: return {Swifttypes,sizeof(Swifttypes)/sizeof(Swifttypes[0])};
 case SyntaxToken::Builtin: return {Swiftbuiltins,sizeof(Swiftbuiltins)/sizeof(Swiftbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Lua: switch(token){
 case SyntaxToken::Keyword: return {Luakeywords,sizeof(Luakeywords)/sizeof(Luakeywords[0])};
 case SyntaxToken::Builtin: return {Luabuiltins,sizeof(Luabuiltins)/sizeof(Luabuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Ruby: switch(token){
 case SyntaxToken::Keyword: return {Rubykeywords,sizeof(Rubykeywords)/sizeof(Rubykeywords[0])};
 case SyntaxToken::Type: return {Rubytypes,sizeof(Rubytypes)/sizeof(Rubytypes[0])};
 case SyntaxToken::Builtin: return {Rubybuiltins,sizeof(Rubybuiltins)/sizeof(Rubybuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Php: switch(token){
 case SyntaxToken::Keyword: return {Phpkeywords,sizeof(Phpkeywords)/sizeof(Phpkeywords[0])};
 case SyntaxToken::Type: return {Phptypes,sizeof(Phptypes)/sizeof(Phptypes[0])};
 case SyntaxToken::Builtin: return {Phpbuiltins,sizeof(Phpbuiltins)/sizeof(Phpbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Shell: switch(token){
 case SyntaxToken::Keyword: return {Shellkeywords,sizeof(Shellkeywords)/sizeof(Shellkeywords[0])};
 case SyntaxToken::Builtin: return {Shellbuiltins,sizeof(Shellbuiltins)/sizeof(Shellbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::PowerShell: switch(token){
 case SyntaxToken::Keyword: return {PowerShellkeywords,sizeof(PowerShellkeywords)/sizeof(PowerShellkeywords[0])};
 case SyntaxToken::Type: return {PowerShelltypes,sizeof(PowerShelltypes)/sizeof(PowerShelltypes[0])};
 case SyntaxToken::Builtin: return {PowerShellbuiltins,sizeof(PowerShellbuiltins)/sizeof(PowerShellbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Sql: switch(token){
 case SyntaxToken::Keyword: return {Sqlkeywords,sizeof(Sqlkeywords)/sizeof(Sqlkeywords[0])};
 case SyntaxToken::Type: return {Sqltypes,sizeof(Sqltypes)/sizeof(Sqltypes[0])};
 case SyntaxToken::Builtin: return {Sqlbuiltins,sizeof(Sqlbuiltins)/sizeof(Sqlbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::CMake: switch(token){
 case SyntaxToken::Keyword: return {CMakekeywords,sizeof(CMakekeywords)/sizeof(CMakekeywords[0])};
 case SyntaxToken::Builtin: return {CMakebuiltins,sizeof(CMakebuiltins)/sizeof(CMakebuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Css: switch(token){
 case SyntaxToken::Keyword: return {Csskeywords,sizeof(Csskeywords)/sizeof(Csskeywords[0])};
 case SyntaxToken::Builtin: return {Cssbuiltins,sizeof(Cssbuiltins)/sizeof(Cssbuiltins[0])};
 default:return {}; }
 case SyntaxLanguage::Yaml: switch(token){
 case SyntaxToken::Keyword: return {Yamlkeywords,sizeof(Yamlkeywords)/sizeof(Yamlkeywords[0])};
 default:return {}; }
 case SyntaxLanguage::Toml: switch(token){
 case SyntaxToken::Keyword: return {Tomlkeywords,sizeof(Tomlkeywords)/sizeof(Tomlkeywords[0])};
 default:return {}; }
 case SyntaxLanguage::Json: switch(token){
 case SyntaxToken::Keyword: return {Jsonkeywords,sizeof(Jsonkeywords)/sizeof(Jsonkeywords[0])};
 default:return {}; }
 case SyntaxLanguage::JsonComments: switch(token){
 case SyntaxToken::Keyword: return {JsonCommentskeywords,sizeof(JsonCommentskeywords)/sizeof(JsonCommentskeywords[0])};
 default:return {}; }
 case SyntaxLanguage::Ini: switch(token){
 default:return {}; }
 case SyntaxLanguage::Markup: switch(token){
 default:return {}; }
 case SyntaxLanguage::Makefile: switch(token){
 case SyntaxToken::Keyword: return {Makefilekeywords,sizeof(Makefilekeywords)/sizeof(Makefilekeywords[0])};
 case SyntaxToken::Builtin: return {Makefilebuiltins,sizeof(Makefilebuiltins)/sizeof(Makefilebuiltins[0])};
 default:return {}; }
 default:return {};
 }
}
