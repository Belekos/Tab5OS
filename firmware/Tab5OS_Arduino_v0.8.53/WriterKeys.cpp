#include "WriterKeys.h"

#include <cstring>
namespace writing {
KeyEvent KeyDecoder::press(uint8_t u, uint8_t m) {
  KeyEvent e;
  e.shift = (m & 0x22) != 0;
  e.ctrl = (m & 0x11) != 0;
  e.meta = (m & 0x88) != 0;
  const bool altGr = (french_||mappingCount_) && (m & 0x40);
  e.alt = (m & 0x44) != 0;
  if (altGr) {
    e.ctrl = false;
    e.alt = false;
  }
  const bool commandChord = !altGr && (e.ctrl || e.meta || e.alt);
  if (commandChord) dead_ = 0;
  switch (u) {
    case 0x28:
      e.key = Key::Enter;
      return e;
    case 0x29:
      e.key = Key::Escape;
      dead_ = 0;
      return e;
    case 0x2a:
      e.key = Key::Backspace;
      return e;
    case 0x2b:
      e.key = Key::Tab;
      return e;
    case 0x49:
      e.key = Key::Insert;
      return e;
    case 0x4c:
      e.key = Key::Delete;
      return e;
    case 0x4f:
      e.key = Key::Right;
      return e;
    case 0x50:
      e.key = Key::Left;
      return e;
    case 0x51:
      e.key = Key::Down;
      return e;
    case 0x52:
      e.key = Key::Up;
      return e;
    case 0x4a:
      e.key = Key::Home;
      return e;
    case 0x4d:
      e.key = Key::End;
      return e;
    case 0x4b:
      e.key = Key::PageUp;
      return e;
    case 0x4e:
      e.key = Key::PageDown;
      return e;
    case 0x3e:
      e.key = Key::F5;
      return e;
    case 0x3f:
      e.key = Key::F6;
      return e;
    case 0x75:
    case 0x3a:
      e.key = Key::F1;
      return e;
    case 0x65:
      e.key = Key::ContextMenu;
      return e;
    case 0x3b:
      e.key = Key::F2;
      return e;
    case 0x43:
      if (e.shift && !e.ctrl && !e.alt && !e.meta) e.key = Key::ContextMenu;
      return e;
    case 0xe3:
    case 0xe7:
      e.key = Key::Command;
      return e;
    case 0x39:
      caps_ = !caps_;
      return e;
    default:
      break;
  }
  const char* value = nullptr;
  char letter[2] = {};
  const KeyMapping* mapped=nullptr;
  for(size_t i=0;i<mappingCount_;++i)if(mapping_[i].usage==u){mapped=&mapping_[i];break;}
  if(mapped){
    unsigned layer=altGr?2:(mapped->letter?(e.ctrl||e.meta?false:e.shift!=caps_):e.shift)?1:0;
    value=mapped->values[layer];if(!*value)return e;
    bool nonAscii=false;
    const char* commandValues[]={mapped->values[0],value};
    for(const char* text:commandValues)
      for(const auto* p=reinterpret_cast<const unsigned char*>(text);*p;++p)
        if(*p>=0x80){nonAscii=true;break;}
    if(commandChord&&nonAscii){
      // Native text keeps its layout. Command letters must stay addressable
      // on Cyrillic/Arabic keyboards; Latin AZERTY/QWERTZ mappings stay intact.
      if(u>=0x04&&u<=0x1d){letter[0]='a'+u-0x04;value=letter;}
      else if(u>=0x1e&&u<=0x27){letter[0]=u==0x27?'0':'1'+u-0x1e;value=letter;}
      else switch(u){
        case 0x2c:value=" ";break;
        case 0x2d:value=e.shift?"_":"-";break;
        case 0x2e:value=e.shift?"+":"=";break;
        case 0x2f:value=e.shift?"{":"[";break;
        case 0x30:value=e.shift?"}":"]";break;
        case 0x31:case 0x32:value=e.shift?"|":"\\";break;
        case 0x33:value=e.shift?":":";";break;
        case 0x34:value=e.shift?"\"":"'";break;
        case 0x35:value=e.shift?"~":"`";break;
        case 0x36:value=e.shift?"<":",";break;
        case 0x37:value=e.shift?">":".";break;
        case 0x38:value=e.shift?"?":"/";break;
        case 0x64:value=e.shift?">":"<";break;
        default:break;
      }
    }
    if((mapped->dead&(1u<<layer))&&!commandChord){char d=!strcmp(value,"¨")?'"':*value;if(!dead_){dead_=d;return e;}dead_=0;}
  }else if (altGr) {
    switch (u) {
      case 0x1f:
        value = "~";
        break;
      case 0x20:
        value = "#";
        break;
      case 0x21:
        value = "{";
        break;
      case 0x22:
        value = "[";
        break;
      case 0x23:
        value = "|";
        break;
      case 0x24:
        value = "`";
        break;
      case 0x25:
        value = "\\";
        break;
      case 0x26:
        value = "^";
        break;
      case 0x27:
        value = "@";
        break;
      case 0x2d:
        value = "]";
        break;
      case 0x2e:
        value = "}";
        break;
      case 0x08:
        value = "€";
        break;
      default:
        return e;
    }
  } else if (u >= 0x04 && u <= 0x1d) {
    const char* map = french_ ? "qbcdefghijkl,noparstuvzxyw" : "abcdefghijklmnopqrstuvwxyz";
    char c = map[u - 4];
    if (c == ',') {
      value = e.shift ? "?" : ",";
    } else {
      letter[0] = ((e.shift != caps_) && !e.ctrl && !e.meta) ? static_cast<char>(c - 32) : c;
      value = letter;
    }
  } else if (u >= 0x1e && u <= 0x27) {
    const char* fr[] = {"&", "é", "\"", "'", "(", "-", "è", "_", "ç", "à"};
    const char* frCaps[] = {"&", "É", "\"", "'", "(", "-", "È", "_", "Ç", "À"};
    const char* usShift[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"};
    const char* digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    value = (e.alt || e.ctrl || e.meta) ? digits[u - 0x1e]
            : french_ ? (e.shift ? digits[u - 0x1e] : (caps_ ? frCaps[u - 0x1e] : fr[u - 0x1e]))
                      : (e.shift ? usShift[u - 0x1e] : digits[u - 0x1e]);
  } else {
    switch (u) {
      case 0x2c:
        value = " ";
        break;
      case 0x2d:
        value = french_ ? (e.shift ? "°" : ")") : (e.shift ? "_" : "-");
        break;
      case 0x2e:
        value = e.shift ? "+" : "=";
        break;
      case 0x2f:
        if (french_) {
          char d = e.shift ? '"' : '^';
          if (!dead_) {
            dead_ = d;
            return e;
          }
          value = dead_ == '^' ? "^" : "¨";
          dead_ = 0;
        } else
          value = e.shift ? "{" : "[";
        break;
      case 0x30:
        value = french_ ? (e.shift ? "£" : "$") : (e.shift ? "}" : "]");
        break;
      case 0x31:
      case 0x32:
        value = french_ ? (e.shift ? "µ" : "*") : (e.shift ? "|" : "\\");
        break;
      case 0x33:
        value = french_ ? ((e.shift != caps_) ? "M" : "m") : (e.shift ? ":" : ";");
        break;
      case 0x34:
        value = french_ ? (e.shift ? "%" : "ù") : (e.shift ? "\"" : "'");
        break;
      case 0x35:
        value = french_ ? "²" : (e.shift ? "~" : "`");
        break;
      case 0x36:
        value = french_ ? (e.shift ? "." : ";") : (e.shift ? "<" : ",");
        break;
      case 0x37:
        value = french_ ? (e.shift ? "/" : ":") : (e.shift ? ">" : ".");
        break;
      case 0x38:
        value = french_ ? (e.shift ? "§" : "!") : (e.shift ? "?" : "/");
        break;
      case 0x64:
        value = e.shift ? ">" : "<";
        break;
      case 0x54:
        value = "/";
        break;
      case 0x55:
        value = "*";
        break;
      case 0x56:
        value = "-";
        break;
      case 0x57:
        value = "+";
        break;
      case 0x58:
        e.key = Key::Enter;
        return e;
      default:
        if (u >= 0x59 && u <= 0x61) {
          letter[0] = '1' + u - 0x59;
          value = letter;
        } else if (u == 0x62)
          value = "0";
        else if (u == 0x63)
          value = ".";
        else
          return e;
    }
  }
  e.key = Key::Text;
  if (dead_ && !e.ctrl && !e.meta) {
    const char* bases = "aeiouAEIOUyYnNcC";
    const char* circum[] = {"â", "ê", "î", "ô", "û", "Â", "Ê", "Î", "Ô", "Û", "ŷ", "Ŷ", "n", "N", "ĉ", "Ĉ"};
    const char* umlaut[] = {"ä", "ë", "ï", "ö", "ü", "Ä", "Ë", "Ï", "Ö", "Ü", "ÿ", "Ÿ", "n", "N", "c", "C"};
    const char* acute[]={"á","é","í","ó","ú","Á","É","Í","Ó","Ú","ý","Ý","ń","Ń","ć","Ć"};
    const char* grave[]={"à","è","ì","ò","ù","À","È","Ì","Ò","Ù","ỳ","Ỳ","n","N","c","C"};
    const char* tilde[]={"ã","ẽ","ĩ","õ","ũ","Ã","Ẽ","Ĩ","Õ","Ũ","ỹ","Ỹ","ñ","Ñ","c","C"};
    const char** composed=dead_=='\''?acute:dead_=='`'?grave:dead_=='~'?tilde:dead_=='^'?circum:umlaut;
    const char* found = value[1] == 0 ? strchr(bases, value[0]) : nullptr;
    if (found)
      strcpy(e.text, composed[found - bases]);
    else {
      if(dead_=='"')strcpy(e.text,"¨");else {e.text[0]=dead_;e.text[1]=0;}
      if (strcmp(value, " ")) strcat(e.text, value);
    }
    dead_ = 0;
  } else if (value)
    strcpy(e.text, value);
  return e;
}
}  // namespace writing
