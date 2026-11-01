import re
def closing(text,start,left='{',right='}'):
    depth=0;quote=None;escape=False;line=False;block=False;i=start
    while i<len(text):
        c=text[i];n=text[i:i+2]
        if line:
            if c=='\n':line=False
        elif block:
            if n=='*/':block=False;i+=1
        elif quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif n=='//':line=True;i+=1
        elif n=='/*':block=True;i+=1
        elif c in "\"'":quote=c
        elif c==left:depth+=1
        elif c==right:
            depth-=1
            if depth==0:return i+1
        i+=1
    raise ValueError('unbalanced source')
def extract(text,name):
    m=re.search(r'static [^\n]+\b'+name+r'\([^;]*?\)\s*\{',text)
    if not m:raise ValueError(name)
    return text[m.start():closing(text,text.index('{',m.start()))]
