"""Inventory statically declared strings passed to indirect UI translators.

This is deliberately not a C++ parser or a claim about arbitrary runtime data.
The producer families below are explicit: Explorer commands, context-menu item
rows, labelled toolbar arrays and shared file-sort captions.
"""
import json,re

STRING=re.compile(r'"(?:\\.|[^"\\])*"')
TOKENS=re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
def code_only(text):return TOKENS.sub(lambda m:' '*len(m.group()),text)
def invariant_key(value):
    # Application names and language-independent key combinations are not
    # missing translations. "Maj", "Entrée", "Suppr" remain translatable.
    return value in {'MicroPython','RTL-SDR','Maze3D'} or bool(re.fullmatch(r'(?:Ctrl\+|Alt\+)*(?:[A-Z.]|F\d{1,2})',value))
def literals(text):
    return [json.loads(m.group()) for m in TOKENS.finditer(text) if m.group().startswith('"')]
def arguments(text,start):
    """Split balanced C++ call/initializer arguments, skipping quoted strings."""
    depth=1;cursor=start;begin=start;parts=[];quote=None
    while cursor<len(text):
        c=text[cursor]
        if quote:
            if c=='\\':cursor+=2;continue
            if c==quote:quote=None
        elif c in ('"',"'"):quote=c
        elif c in '([{':depth+=1
        elif c in ')]}':
            depth-=1
            if depth==0:return parts+[text[begin:cursor]],cursor
        elif c==',' and depth==1:parts.append(text[begin:cursor]);begin=cursor+1
        cursor+=1
    raise ValueError('Unterminated UI producer arguments')
def inventory(path):
    source=path.read_text(encoding='utf8');rows=[]
    def add(value,offset,kind):
        if value:rows.append({'key':value,'location':path.name+':'+str(source.count('\n',0,offset)+1),'producer':kind})
    if path.name=='Tab5ExplorerMenus.inc':
        for match in re.finditer(r'\badd\s*\(\s*FileCommand::',source):
            parts,_=arguments(source,source.index('(',match.start())+1)
            for part in parts[1:3]:
                for value in literals(part):add(value,match.start(),'Explorer command title/shortcut')
    if path.name=='Tab5CredentialUi.inc':
        for match in re.finditer(r'\b(?:label|button|note)\s*\(',source):
            parts,_=arguments(source,match.end())
            for value in literals(parts[0]):add(value,match.start(),'credential UI translated helper')
    for match in re.finditer(r'const\s+char\s*\*\s*(?:const\s+)?(?:tips|titles)\s*\[\s*\]\s*=\s*\{',source):
        parts,_=arguments(source,match.end())
        for part in parts:
            for value in literals(part):add(value,match.start(),'translated toolbar/title array')
    if path.name=='Tab5FileSort.h':
        match=re.search(r'\blabels\s*\[\s*\]\s*=\s*\{',source)
        if not match:raise ValueError('Shared sort caption producer changed')
        parts,_=arguments(source,match.end())
        for part in parts:
            for value in literals(part):add(value,match.start(),'shared sort caption')
    if 'Tab5ContextMenu::Item' in source:
        for match in re.finditer(r'\bitems\s*\[[^\]]+\]\s*=\s*\{',source):
            parts,_=arguments(source,match.end())
            for part in parts[:2]:
                for value in literals(part):add(value,match.start(),'context menu title/shortcut')
    return rows
