// Tab5OS MicroPython adapter, MIT. All VM entry points execute on one worker.
#include "py/runtime.h"
#include "../../Tab5SystemPath.h"
#include "py/builtin.h"
#include "py/objstr.h"
#include "py/lexer.h"
#include "py/compile.h"
#include "py/stackctrl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/stream.h"
#include "shared/runtime/gchelper.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
extern int tab5_mp_cancelled(void);
extern void tab5_mp_yield(void);
extern void tab5_mp_output(const char*, size_t);
extern void tab5_mp_phase(const char*, const char*);
extern int tab5_mp_io(int op, int a, int b, int c, const void* in, size_t len, void* out, size_t capacity);
static int fileDescriptors[16];
int tab5_mp_open_files(void){int n=0;for(unsigned i=0;i<16;++i)if(fileDescriptors[i]>=0)++n;return n;}
static bool interruptRaised=false;
void tab5_mp_hook(void) {
  tab5_mp_yield();
  if(tab5_mp_cancelled()&&!interruptRaised){interruptRaised=true;mp_raise_type(&mp_type_KeyboardInterrupt);}
}
void tab5_mp_compile_poll(void) {
  // compile_node recursively descends imported syntax trees too. VM-only
  // stack checks do not protect this path, before the first bytecode runs.
  mp_stack_check();
  tab5_mp_hook();
}
mp_uint_t mp_hal_stdout_tx_strn(const char* s,size_t n){tab5_mp_output(s,n);return n;}
void mp_hal_stdout_tx_strn_cooked(const char* s,size_t n){tab5_mp_output(s,n);}
mp_import_stat_t mp_import_stat(const char* p){char full[320];struct stat s;if(!tab5SystemPosixPath(full,sizeof(full),p)||stat(full,&s))return MP_IMPORT_STAT_NO_EXIST;return S_ISDIR(s.st_mode)?MP_IMPORT_STAT_DIR:MP_IMPORT_STAT_FILE;}
int tab5_mp_open_import(const char* p){char full[320];if(!tab5SystemPosixPath(full,sizeof(full),p)){errno=EINVAL;return -1;}return open(full,O_RDONLY,0644);}
void gc_collect(void){
 tab5_mp_phase("GC",NULL);gc_collect_start();gc_helper_collect_regs_and_stack();gc_collect_end();
}
void nlr_jump_fail(void* val){(void)val;abort();}
// POSIX SD file objects; the VM closes every owned descriptor when a script ends.
typedef struct {mp_obj_base_t base;int fd;size_t pos,length;uint8_t buffer[1024];} TabFile;

// json.load()/readline() request ONE byte at a time. A FAT/VFS read for each
// character used to run for a complete language pack without yielding at all.
// Keep this buffer in the GC heap (PSRAM), never on the task's internal stack.
static mp_uint_t file_read(mp_obj_t self,void* buffer,mp_uint_t n,int* error){
 TabFile* f=MP_OBJ_TO_PTR(self);
 if(f->fd<0){*error=EBADF;return MP_STREAM_ERROR;}if(!n)return 0;
 if(f->pos<f->length){size_t count=f->length-f->pos;if(count>n)count=n;memcpy(buffer,f->buffer+f->pos,count);f->pos+=count;return count;}
 tab5_mp_hook();
 if(n>=sizeof(f->buffer)){if(n>4096)n=4096;int r=read(f->fd,buffer,n);if(r<0){*error=errno;return MP_STREAM_ERROR;}return r;}
 int r=read(f->fd,f->buffer,sizeof(f->buffer));
 if(r<0){*error=errno;return MP_STREAM_ERROR;}
 f->length=r;f->pos=0;if(n>f->length)n=f->length;memcpy(buffer,f->buffer,n);f->pos=n;return n;
}
static mp_uint_t file_write(mp_obj_t self,const void* buffer,mp_uint_t n,int* error){
 TabFile* f=MP_OBJ_TO_PTR(self);if(f->fd<0){*error=EBADF;return MP_STREAM_ERROR;}
 // Switching r+ from reading to writing must use the logical Python cursor.
 if(f->length>f->pos&&lseek(f->fd,-(off_t)(f->length-f->pos),SEEK_CUR)<0){*error=errno;return MP_STREAM_ERROR;}
 f->pos=f->length=0;tab5_mp_hook();if(n>4096)n=4096;
 int r=write(f->fd,buffer,n);if(r<0){*error=errno;return MP_STREAM_ERROR;}return r;
}
static mp_uint_t file_ioctl(mp_obj_t self,mp_uint_t request,uintptr_t arg,int* error){TabFile* f=MP_OBJ_TO_PTR(self);
 if(request==MP_STREAM_CLOSE){if(f->fd>=0){for(unsigned i=0;i<16;++i)if(fileDescriptors[i]==f->fd)fileDescriptors[i]=-1;close(f->fd);f->fd=-1;}return 0;}
 if(f->fd<0){*error=EBADF;return MP_STREAM_ERROR;}
 if(request==MP_STREAM_FLUSH){if(fsync(f->fd)){*error=errno;return MP_STREAM_ERROR;}return 0;}
 if(request==MP_STREAM_SEEK){struct mp_stream_seek_t* s=(struct mp_stream_seek_t*)arg;off_t offset=s->offset;if(s->whence==SEEK_CUR)offset-=(off_t)(f->length-f->pos);off_t r=lseek(f->fd,offset,s->whence);if(r<0){*error=errno;return MP_STREAM_ERROR;}f->pos=f->length=0;s->offset=r;return 0;}
 *error=EINVAL;return MP_STREAM_ERROR;
}
static mp_obj_t file_enter(mp_obj_t self){return self;}
static MP_DEFINE_CONST_FUN_OBJ_1(file_enter_obj,file_enter);
static mp_obj_t file_exit(size_t n,const mp_obj_t* args){(void)n;int err=0;file_ioctl(args[0],MP_STREAM_CLOSE,0,&err);return mp_const_none;}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(file_exit_obj,4,4,file_exit);
static const mp_rom_map_elem_t file_locals[]={
 {MP_ROM_QSTR(MP_QSTR_read),MP_ROM_PTR(&mp_stream_read_obj)},
 {MP_ROM_QSTR(MP_QSTR_readinto),MP_ROM_PTR(&mp_stream_readinto_obj)},
 {MP_ROM_QSTR(MP_QSTR_readline),MP_ROM_PTR(&mp_stream_unbuffered_readline_obj)},
 {MP_ROM_QSTR(MP_QSTR_write),MP_ROM_PTR(&mp_stream_write_obj)},
 {MP_ROM_QSTR(MP_QSTR_seek),MP_ROM_PTR(&mp_stream_seek_obj)},
 {MP_ROM_QSTR(MP_QSTR_tell),MP_ROM_PTR(&mp_stream_tell_obj)},
 {MP_ROM_QSTR(MP_QSTR_flush),MP_ROM_PTR(&mp_stream_flush_obj)},
 {MP_ROM_QSTR(MP_QSTR_close),MP_ROM_PTR(&mp_stream_close_obj)},
 {MP_ROM_QSTR(MP_QSTR___enter__),MP_ROM_PTR(&file_enter_obj)},
 {MP_ROM_QSTR(MP_QSTR___exit__),MP_ROM_PTR(&file_exit_obj)},
};
static MP_DEFINE_CONST_DICT(file_locals_dict,file_locals);
static const mp_stream_p_t file_protocol={.read=file_read,.write=file_write,.ioctl=file_ioctl,.is_text=false};
static const mp_stream_p_t text_protocol={.read=file_read,.write=file_write,.ioctl=file_ioctl,.is_text=true};
MP_DEFINE_CONST_OBJ_TYPE(tab_file_type,MP_QSTR_File,MP_TYPE_FLAG_ITER_IS_STREAM,protocol,&file_protocol,locals_dict,&file_locals_dict);
MP_DEFINE_CONST_OBJ_TYPE(tab_text_type,MP_QSTR_TextFile,MP_TYPE_FLAG_ITER_IS_STREAM,protocol,&text_protocol,locals_dict,&file_locals_dict);
mp_obj_t mp_builtin_open(size_t n,const mp_obj_t* args,mp_map_t* kw){
 if(n<1||n>2)mp_raise_TypeError(MP_ERROR_TEXT("open: path and mode only"));
 if(kw&&kw->used)mp_raise_TypeError(MP_ERROR_TEXT("open: positional arguments only"));
 const char* path=mp_obj_str_get_str(args[0]);const char* mode=n>1?mp_obj_str_get_str(args[1]):"r";
 int flags;switch(mode[0]){case 'r':flags=O_RDONLY;break;case 'w':flags=O_WRONLY|O_CREAT|O_TRUNC;break;case 'a':flags=O_WRONLY|O_CREAT|O_APPEND;break;case 'x':flags=O_WRONLY|O_CREAT|O_EXCL;break;default:mp_raise_ValueError(MP_ERROR_TEXT("mode"));}
 if(strchr(mode,'+'))flags=(flags&~O_ACCMODE)|O_RDWR;
 unsigned slot=0;while(slot<16&&fileDescriptors[slot]>=0)++slot;if(slot==16)mp_raise_OSError(EMFILE);
 char full[320];if(!tab5SystemPythonPath(full,sizeof(full),path))mp_raise_ValueError(MP_ERROR_TEXT("invalid SD path or path too long"));
 TabFile* f=mp_obj_malloc(TabFile,strchr(mode,'b')?&tab_file_type:&tab_text_type);f->fd=-1;f->pos=f->length=0;
 tab5_mp_phase("ouvrir",full);
 int fd=open(full,flags,0666);if(fd<0)mp_raise_OSError(errno);fileDescriptors[slot]=fd;f->fd=fd;return MP_OBJ_FROM_PTR(f);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj,1,mp_builtin_open);
static mp_obj_t call(size_t n,const mp_obj_t* args){
 mp_stack_check();
 if(n<1)mp_raise_TypeError(MP_ERROR_TEXT("operation required"));int op=mp_obj_get_int(args[0]);
 int a=n>1?mp_obj_get_int(args[1]):0,b=n>2?mp_obj_get_int(args[2]):0,c=n>3?mp_obj_get_int(args[3]):0;
 mp_buffer_info_t input={0};if(n>4&&args[4]!=mp_const_none)mp_get_buffer_raise(args[4],&input,MP_BUFFER_READ);
 int cap=n>5?mp_obj_get_int(args[5]):0;if(cap<0||cap>4096)mp_raise_ValueError(MP_ERROR_TEXT("transfer limit: 4096 bytes"));
 vstr_t out;vstr_init_len(&out,cap);int result=tab5_mp_io(op,a,b,c,input.buf,input.len,out.buf,cap);
 if(tab5_mp_cancelled()&&!interruptRaised){vstr_clear(&out);tab5_mp_hook();}
 if(result<0){vstr_clear(&out);mp_raise_OSError(-result);}
 if(cap){if((size_t)result>(size_t)cap){vstr_clear(&out);mp_raise_OSError(EOVERFLOW);}out.len=result;return mp_obj_new_bytes_from_vstr(&out);}vstr_clear(&out);return mp_obj_new_int(result);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(call_obj,1,6,call);
static const mp_rom_map_elem_t globals[]={ {MP_ROM_QSTR(MP_QSTR___name__),MP_ROM_QSTR(MP_QSTR__tab5)}, {MP_ROM_QSTR(MP_QSTR_call),MP_ROM_PTR(&call_obj)} };
static MP_DEFINE_CONST_DICT(globals_dict,globals);
const mp_obj_module_t tab5_module={.base={&mp_type_module},.globals=(mp_obj_dict_t*)&globals_dict};
MP_REGISTER_MODULE(MP_QSTR__tab5,tab5_module);
// Only a canonical volume-absolute script path can add local import roots.
// Normalize repeated separators; virtual editor names and traversal never
// become filesystem roots. The Python VFS uses /sd for SD and /sd/USB for USB.
static bool script_import_directory(const char* filename,char* out,size_t capacity){
 if(!filename||filename[0]!='/'||capacity<8)return false;
 size_t length=strnlen(filename,261);
 if(length<2||length>260||filename[length-1]=='/')return false;
 size_t used=3,lastSlash=3;memcpy(out,"/sd",3);
 const char* p=filename;
 while(*p){
  while(*p=='/')++p;
  const char* first=p;
  while(*p&&*p!='/'){
   if((unsigned char)*p<32||(unsigned char)*p==127||*p=='\\'||*p==':')return false;
   ++p;
  }
  const size_t count=(size_t)(p-first);if(!count)break;
  if((count==1&&first[0]=='.')||(count==2&&first[0]=='.'&&first[1]=='.'))return false;
  if(used>=capacity-1||count>capacity-used-2)return false;
  lastSlash=used;out[used++]='/';memcpy(out+used,first,count);used+=count;
 }
 out[lastSlash]=0;return used>3&&tab5SystemPath(out+3,capacity-3,out+3,strlen(out+3));
}
int tab5_mp_run_with_argument(const char* script,const char* filename,const char* argument,void* heap,size_t heapSize){
 interruptRaised=false;
 // Native services (SD/TLS/graphics) execute below the Python frames. Keep
 // 12 KiB on the 32 KiB worker for their calls and exception unwinding.
 int stack_top;mp_stack_set_top(&stack_top);mp_stack_set_limit(20*1024);
 for(unsigned i=0;i<16;++i)fileDescriptors[i]=-1;
 gc_init(heap,(uint8_t*)heap+heapSize);
 // mp_init also allocates. It must be INSIDE the exception boundary, otherwise
 // allocation failure calls nlr_jump_fail()/abort instead of reporting an error.
 MP_STATE_THREAD(nlr_top)=NULL;
 nlr_buf_t nlr;int result=0;
 if(nlr_push(&nlr)==0){
 tab5_mp_phase("initialisation",filename);mp_init();
 // Values are created directly: quotation marks and Unicode in filenames
 // cannot become executable Python code. Ordinary launches receive argv[0].
 mp_obj_list_append(mp_sys_argv,mp_obj_new_str(filename,strlen(filename)));
 if(argument&&*argument)mp_obj_list_append(mp_sys_argv,mp_obj_new_str(argument,strlen(argument)));
 // OS modules keep priority. Add only this script's directory and lib folder;
 // a project can use attached dependencies without installing them globally.
 // The library catalogue is deliberately not an import root.
 mp_obj_list_append(mp_sys_path,mp_obj_new_str("/sd/MicroPython/lib",19));
 char dir[320];
 if(script_import_directory(filename,dir,sizeof(dir))){
  size_t length=strlen(dir);
  mp_obj_list_append(mp_sys_path,mp_obj_new_str(dir,length));
  if(length+4<sizeof(dir)){
   memcpy(dir+length,"/lib",5);
   mp_obj_list_append(mp_sys_path,mp_obj_new_str(dir,length+4));
  }
 }
 tab5_mp_phase("analyse",filename);
 mp_lexer_t* lex=mp_lexer_new_from_str_len(qstr_from_str(filename),script,strlen(script),0);qstr name=lex->source_name;
   mp_parse_tree_t tree=mp_parse(lex,MP_PARSE_FILE_INPUT);tab5_mp_phase("compilation",filename);mp_obj_t function=mp_compile(&tree,name,false);tab5_mp_phase("execution",filename);mp_call_function_0(function);nlr_pop();
 }else{
  result=1;tab5_mp_phase("exception",filename);
  // Printing a traceback can allocate, too. A second MemoryError must not
  // escape the worker's exception boundary and reset the complete OS.
  mp_obj_t exception=MP_OBJ_FROM_PTR(nlr.ret_val);
  nlr_buf_t report;
  if(nlr_push(&report)==0){mp_obj_print_exception(&mp_plat_print,exception);nlr_pop();}
  else{const char* fallback="MemoryError while reporting Python exception\n";tab5_mp_output(fallback,strlen(fallback));}
 }
 for(unsigned i=0;i<16;++i)if(fileDescriptors[i]>=0)close(fileDescriptors[i]);mp_deinit();return result;
}

// Retain the host/embedding entry point without a document argument.
int tab5_mp_run(const char* script,const char* filename,void* heap,size_t heapSize){
 return tab5_mp_run_with_argument(script,filename,NULL,heap,heapSize);
}
