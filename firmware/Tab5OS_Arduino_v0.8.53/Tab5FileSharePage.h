#pragma once
#include <stddef.h>
struct Tab5ShareText {const char* id;const char* source;};
static const Tab5ShareText FileShareTexts[]={
 {"title","Carte SD"},
 {"sub","Tab5 · Partage local de fichiers"},
 {"code","Code affiché sur la tablette"},
 {"connect","Se connecter"},
 {"parent","Dossier parent"},
 {"refresh","Actualiser"},
 {"folder","Nouveau dossier"},
 {"upload","Envoyer des fichiers"},
 {"cancel","Annuler le transfert"},
 {"next","Afficher la suite"},
 {"name","Nom"},
 {"size","Taille"},
 {"actions","Actions"},
 {"download","Télécharger"},
 {"rename","Renommer"},
 {"delete","Supprimer"},
 {"empty","Ce dossier est vide."},
 {"foldername","Nom du dossier"},
 {"newname","Nouveau nom"},
 {"deleteconfirm","Mettre ce fichier à la corbeille ou supprimer ce dossier vide ?\n"},
 {"permanent","Supprimer définitivement"},
 {"permanentconfirm","Supprimer définitivement cet élément de la corbeille ?\n"},
 {"replace","Remplacer ce fichier ?\n"},
 {"done","Terminé"},
 {"busy","La carte SD est utilisée. Fermez le document ouvert ou arrêtez l’enregistrement, puis réessayez."},
 {"choose","Choisir les fichiers"},
 {"limit","Les fichiers doivent faire moins de 2 Gio."},
 {"hint","Les anciennes versions et fichiers supprimés sont conservés dans /.Tab5Trash. Vous pouvez les y télécharger ou les supprimer définitivement. Les dossiers doivent être vides avant suppression."},
 {"auth","Code incorrect ou session terminée."},
 {"request","Requête invalide."},
 {"missing","Ce fichier n’existe plus."},
 {"exists","La destination existe ou le dossier n’est pas vide."},
 {"full","La carte SD est pleine."},
 {"storage","Carte SD indisponible. Réessayez."},
 {"changed","Le fichier a changé. Actualisez avant de réessayer."},
 {"path","Ce nom ou chemin n’est pas autorisé."},
 {"large","Les fichiers doivent faire moins de 2 Gio."},
 {"transfer","Transfert interrompu. Le fichier original a été préservé."},
 {"network","Connexion perdue. Vérifiez le partage sur la tablette."},
 {"badname","Saisissez un nom sans / ni \\."},
 {"cancelled","Transfert annulé."},
 {"bytes","octets"},
};
static const char FileSharePageBefore[] = R"T5PAGE(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Tab5</title><style>
:root{color-scheme:light dark;--bg:#111923;--panel:#1d2936;--line:#3c4c5c;--ink:#eff5fa;--dim:#b6c5d3;--accent:#9ddcd1}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px system-ui,sans-serif}main{max-width:1050px;margin:0 auto;padding:30px 20px}h1{font-size:32px;margin:8px 0 24px}.eyebrow{color:var(--accent);font-size:14px}button,.button,input{font:inherit;border:1px solid var(--line);border-radius:8px;padding:10px 13px;background:var(--panel);color:var(--ink)}button,.button{cursor:pointer;text-decoration:none;display:inline-block}button:hover,.button:hover{border-color:var(--accent)}button:disabled{opacity:.45;cursor:wait}input{max-width:100%}#pin{direction:ltr;width:170px;letter-spacing:3px}#login{padding:24px;border:1px solid var(--line);border-radius:14px}label{display:block;margin-bottom:10px}.toolbar{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0}.path{direction:ltr;text-align:left;overflow-wrap:anywhere;font-weight:600;background:var(--panel);padding:14px;border-radius:8px}#status{min-height:30px;margin:16px 0;white-space:pre-wrap}#status.error{color:#ffb6ae}.hint{color:var(--dim);font-size:14px;line-height:1.5}table{width:100%;border-collapse:collapse}th{text-align:start;color:var(--dim);font-weight:500}th,td{padding:14px 8px;border-bottom:1px solid var(--line)}td:first-child{overflow-wrap:anywhere;max-width:400px}.ops{display:flex;gap:6px;flex-wrap:wrap}.ops button,.ops a{font-size:13px;padding:8px}.name{background:transparent;border:0;text-align:start;padding:0;font-weight:600;color:var(--accent)}.size{white-space:nowrap;font-size:14px;color:var(--dim)}progress{width:100%;height:18px;accent-color:var(--accent)}[hidden]{display:none!important}@media(max-width:600px){main{padding:20px 12px}th:nth-child(2),td:nth-child(2){display:none}td:first-child{max-width:45vw}.ops{max-width:180px}h1{font-size:28px}}</style><main><div class="eyebrow" id="sub"></div><h1 id="title"></h1><form id="login"><label for="pin" id="code"></label><input id="pin" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="one-time-code" required><button id="connect" type="submit"></button></form><section id="manager" hidden><div class="path" id="path"></div><div class="toolbar"><button id="parent"></button><button id="refresh"></button><button id="folder"></button><button id="upload"></button><input type="file" id="files" multiple hidden><button id="cancel" hidden></button></div><progress id="progress" max="100" value="0" hidden></progress><table><thead><tr><th id="name"></th><th id="size"></th><th id="actions"></th></tr></thead><tbody id="rows"></tbody></table><button id="next" hidden></button><p class="hint" id="hint"></p></section><div id="status" role="status" aria-live="polite"></div></main><script>
'use strict';const T=)T5PAGE";
static const char FileSharePageAfter[] = R"T5PAGE(,$=id=>document.getElementById(id);document.documentElement.lang=T.language;document.documentElement.dir=T.direction;document.title=T.title+' · Tab5';for(const id of ['title','sub','code','connect','parent','refresh','folder','upload','cancel','next','name','size','actions','hint'])$(id).textContent=T[id];let path='/',offset=0,busy=false,xhr=null;function message(s,bad=false){$('status').textContent=s;$('status').className=bad?'error':''}function fail(e){message((T[e.key]||e.message||T.network)+(e.backup?'\n'+e.backup:''),true)}function join(n){return path==='/'?'/'+n:path+'/'+n}function valid(n){return n&&!/[\\/]/.test(n)&&n!=='.'&&n!=='..'}function lock(b){busy=b;for(const id of ['parent','refresh','folder','upload','next'])$(id).disabled=b;for(const b of $('rows').querySelectorAll('button'))b.disabled=busy}async function api(action,params={},method='GET',body=null){const q=new URLSearchParams(params),r=await fetch(action+(q.size?'?'+q:''),{method,headers:method==='GET'?{}:{'X-Tab5-Request':'1'},body,credentials:'same-origin'});let data;try{data=await r.json()}catch(e){throw {key:'network'}}if(!r.ok)throw {key:data.error||'network',status:r.status,backup:data.backup};return data}function bytes(n){if(n<1024)return n+' '+T.bytes;if(n<1048576)return (n/1024).toFixed(1)+' KiB';return(n/1048576).toFixed(1)+' MiB'}function button(text,fn){const b=document.createElement('button');b.textContent=text;b.onclick=fn;return b}async function change(fn){if(busy)return;lock(true);try{await fn();await list(false);message(T.done)}catch(e){fail(e)}finally{lock(false)}}function row(e){const tr=document.createElement('tr'),n=document.createElement('td'),s=document.createElement('td'),a=document.createElement('td'),ops=document.createElement('div');ops.className='ops';const target=join(e.name);if(e.dir){const b=button('▸ '+e.name,()=>{if(!busy){path=target;list(false).catch(fail)}});b.className='name';n.append(b)}else n.textContent=e.name;s.className='size';s.textContent=e.dir?'—':bytes(e.size);if(!e.dir){const d=document.createElement('a');d.className='button';d.textContent=T.download;d.href='/api/download?'+new URLSearchParams({path:target});d.download=e.name;ops.append(d)}ops.append(button(T.rename,()=>{const name=prompt(T.newname,e.name);if(name===null||name===e.name)return;if(!valid(name))return message(T.badname,true);change(()=>api('/api/rename',{path:target,to:join(name),id:e.id},'POST'))}),button(path.startsWith('/.Tab5Trash/')?T.permanent:T.delete,()=>{const permanent=path.startsWith('/.Tab5Trash/');if(confirm((permanent?T.permanentconfirm:T.deleteconfirm)+e.name))change(()=>api('/api/delete',{path:target,id:e.id,...(permanent?{permanent:1}:{})},'POST'))}));a.append(ops);tr.append(n,s,a);$('rows').append(tr)}async function list(append){const data=await api('/api/list',{path,offset:append?offset:0});if(!append)$('rows').replaceChildren();data.entries.forEach(row);offset=data.next;$('next').hidden=!data.more;$('path').textContent=path;if(!offset)message(T.empty);$('login').hidden=true;$('manager').hidden=false}function transfer(file,target,id){return new Promise((resolve,reject)=>{xhr=new XMLHttpRequest();xhr.open('PUT','/api/upload?'+new URLSearchParams({path:target,...(id?{id}:{} )}));xhr.setRequestHeader('X-Tab5-Request','1');xhr.upload.onprogress=e=>{if(e.lengthComputable){$('progress').value=e.loaded*100/e.total;message(file.name+' · '+Math.round(e.loaded*100/e.total)+' %')}};xhr.onload=()=>{let d={};try{d=JSON.parse(xhr.responseText)}catch{}if(xhr.status===200)resolve();else reject({key:d.error||'network',backup:d.backup})};xhr.onerror=()=>reject({key:'network'});xhr.onabort=()=>reject({key:'cancelled'});xhr.send(file)})}$('login').onsubmit=async e=>{e.preventDefault();try{await api('/login',{},'POST',$('pin').value);$('pin').value='';await list(false);message('')}catch(e){fail(e)}};$('parent').onclick=()=>{path=path.slice(0,path.lastIndexOf('/'))||'/';list(false).catch(fail)};$('refresh').onclick=()=>list(false).catch(fail);$('next').onclick=()=>list(true).catch(fail);$('folder').onclick=()=>{const n=prompt(T.foldername);if(n===null)return;if(!valid(n))return message(T.badname,true);change(()=>api('/api/mkdir',{path:join(n)},'POST'))};$('upload').onclick=()=>$('files').click();$('cancel').onclick=()=>{if(xhr)xhr.abort()};$('files').onchange=async()=>{const files=[...$('files').files];if(!files.length)return;lock(true);$('cancel').hidden=false;$('progress').hidden=false;try{for(const f of files){if(f.size>2147483647)throw {key:'large'};if(!valid(f.name))throw {key:'path'};const target=join(f.name);let id=0;try{const old=await api('/api/stat',{path:target});if(old.dir)throw {key:'exists'};if(!confirm(T.replace+f.name))continue;id=old.id}catch(e){if(e.status!==404)throw e}await transfer(f,target,id)}message(T.done);await list(false)}catch(e){fail(e)}finally{xhr=null;lock(false);$('cancel').hidden=true;$('progress').hidden=true;$('files').value=''}};list(false).catch(e=>{if(e.key!=='auth')fail(e)});
</script></html>)T5PAGE";
