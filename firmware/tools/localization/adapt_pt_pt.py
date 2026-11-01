"""Adapt the complete Brazilian Portuguese pack to European Portuguese.
Regional vocabulary, inflection and UI terminology are applied to whole words;
paths, printf arguments and executable identifiers remain unchanged.
"""
from pathlib import Path
import json,re
folder=Path(__file__).resolve().parents[2]/'Tab5OS_Arduino_v0.8.18_Ecriture/sdcard/System/languages'
source=json.loads((folder/'pt-BR.json').read_text(encoding='utf-8-sig'))
words={
'console':'consola','aba':'separador','abas':'separadores','senha':'palavra-passe','senhas':'palavras-passe','arquivo':'ficheiro','arquivos':'ficheiros','tela':'ecrã','telas':'ecrãs',
'usuário':'utilizador','usuários':'utilizadores','mouse':'rato',
'aplicativo':'aplicação','aplicativos':'aplicações','mídia':'multimédia',
'prévia':'pré-visualização','prévio':'prévio','visualização':'visualização',
'configurações':'definições','configuração':'configuração',
'conexão':'ligação','conexões':'ligações','conectar':'ligar','conecte':'ligue',
'conectado':'ligado','conectados':'ligados','conectada':'ligada',
'desconecte':'desligue','desconectar':'desligar','desconectado':'desligado',
'reconectado':'religado','reconexão':'restabelecimento da ligação',
'salvar':'guardar','salve':'guarde','salva':'guarda','salvo':'guardado',
'salvos':'guardados','salvas':'guardadas','salvamento':'gravação','salvamentos':'gravações',
'excluir':'eliminar','exclua':'elimine','exclui':'elimina','excluído':'eliminado',
'excluídos':'eliminados','excluída':'eliminada','excluídas':'eliminadas','exclusões':'exclusões',
'gerenciador':'gestor','gerenciados':'geridos','gerenciar':'gerir','gerencie':'gira','gerenciamento':'gestão',
'baixar':'transferir','baixe':'transfira','baixado':'transferido','baixados':'transferidos',
'baixada':'transferida','baixadas':'transferidas','download':'transferência','downloads':'transferências',
'acessar':'aceder','acesse':'aceda','rolagem':'deslocamento','rolar':'deslocar',
'rolam':'deslocam','rola':'desloca','seção':'secção','seções':'secções','subseções':'subsecções',
'digite':'introduza','digitar':'introduzir','digitado':'introduzido','recortar':'cortar','recortado':'cortado',
'controle':'controlo','controles':'controlos','controlar':'controlar',
'porcentagem':'percentagem','porcentagens':'percentagens','colorido':'a cores','colorida':'a cores',
'grade':'grelha','layout':'disposição','layouts':'disposições','apelido':'alcunha',
'busca':'pesquisa','buscar':'pesquisar','ônibus':'autocarro','celular':'telemóvel',
'buscando':'a pesquisar','verificando':'a verificar','abrindo':'a abrir','aguardando':'a aguardar',
'reconectando':'a restabelecer a ligação','preparando':'a preparar','conectando':'a ligar',
'rolando':'a deslocar','digitando':'a escrever','indexando':'a indexar','carregando':'a carregar',
'reproduzindo':'a reproduzir','funcionando':'a funcionar','restaurando':'a restaurar',
'derivando':'a derivar','enviando':'a enviar','consultando':'a consultar','contando':'a contar',
'criptografando':'a cifrar','descriptografando':'a decifrar','capturando':'a capturar','codificando':'a codificar',
'instalando':'a instalar','importando':'a importar','calculando':'a calcular','baixando':'a transferir',
'fechando':'a fechar','iniciando':'a iniciar','encerrando':'a terminar','lendo':'a ler',
'concluindo':'a concluir','liberando':'a libertar','decodificando':'a descodificar',
'salvando':'a guardar','copiando':'a copiar',
'criptografia':'cifragem','criptografar':'cifrar','criptografado':'cifrado',
'criptografados':'cifrados','criptografada':'cifrada','criptografadas':'cifradas',
'descriptografia':'decifragem','descriptografar':'decifrar','descriptografado':'decifrado',
'descriptografada':'decifrada','decodificar':'descodificar','decodificação':'descodificação','decodificador':'descodificador',
'detectar':'detetar','detectado':'detetado','detectada':'detetada','detectados':'detetados','detecção':'deteção',
'liberar':'libertar','liberado':'libertado','liberada':'libertada','liberados':'libertados','liberadas':'libertadas',
'ação':'ação','ações':'ações','tênue':'ténue','sutil':'subtil','cotidiano':'quotidiano',
'subpasta':'subpasta','subpastas':'subpastas','itens':'elementos','item':'elemento',
'cole':'cole','resetar':'reiniciar','compartilhamento':'partilha','compartilhar':'partilhar',
'compartilhado':'partilhado','compartilhados':'partilhados','compartilhada':'partilhada',
'backup':'cópia de segurança','backups':'cópias de segurança',
}
phrases={
'salvamento automático':'gravação automática','Salvamento automático':'Gravação automática',
'salvamento verificado':'gravação verificada','Salvamento verificado':'Gravação verificada',
'salvamento incompleto':'gravação incompleta','salvamentos incompletos':'gravações incompletas',
'salvamentos anteriores':'gravações anteriores','Salvamentos e estados':'Gravações e estados',
'salvamento em andamento':'gravação em curso','salvamento está em andamento':'gravação está em curso',
'em andamento':'em curso','está sendo':'está a ser','estão sendo':'estão a ser',
'continua sendo':'continua a ser','continua precisando':'continua a precisar',
'Você pode':'Pode','você pode':'pode','você lê':'lê','você usa':'utiliza',
'Não é possível':'Não é possível','por padrão':'por predefinição',
'disposição padrão':'disposição predefinida','layout padrão':'disposição predefinida',
'Layout padrão':'Disposição predefinida','pasta padrão':'pasta predefinida','fonte padrão':'fonte predefinida',
'arquivos padrão':'ficheiros predefinidos','configurações padrão':'definições predefinidas',
'Padrão':'Predefinido','padrão':'predefinido',
'é salva':'é guardada','foi salva':'foi guardada','salva no':'guardada no','salva na':'guardada na',
'cópia salva':'cópia guardada','cópias salvas':'cópias guardadas',
'estação de rádio':'estação de rádio','em uma':'numa','em um':'num',
'Você também pode':'Também pode','Você pode':'Pode',
}
def adapt(value):
 for a,b in sorted(phrases.items(),key=lambda pair:-len(pair[0])):value=re.sub(r'(?<!\w)'+re.escape(a)+r'(?!\w)',lambda m:b,value)
 if len(value)<300:value=re.sub(r'\bsalva\b','guardada',value)
 def replace(match):
  word=match.group(0);target=words.get(word.lower())
  if target is None:return word
  return target.upper() if word.isupper() else target[0].upper()+target[1:] if word[0].isupper() else target
 return re.sub(r'\b[^\W\d_]+\b',replace,value)
translations={key:adapt(value) for key,value in source['translations'].items()}
# Avoid grammatical mismatches from nouns that change gender between locales.
overrides={
'Média':'Multimédia','Fichiers':'Ficheiros','Réglages':'Definições','Affichage':'Ecrã',
'Autosauvegarde':'Gravação automática','Délai autosauvegarde':'Intervalo de gravação automática',
'Presse-papiers texte vide':'Área de transferência vazia',
'Explorateur de fichiers':'Explorador de ficheiros','Enregistrer sous':'Guardar como',
'Sauvegarde OK':'Guardado','Sauvegarde vérifiée':'Gravação verificada',
'Application Python':'Aplicação Python','Application (.app.elf)':'Aplicação (.app.elf)',
'Lecture en cours':'Em reprodução','Applications':'Aplicações','Aucun fichier':'Nenhum ficheiro',
'Ordinateur':'Computador','Disposition du clavier appliquée':'Disposição do teclado aplicada',
'Disposition du clavier invalide':'Disposição do teclado inválida',
'Application native terminée':'Aplicação nativa terminada','Application native arrêtée':'Aplicação nativa parada',
'Application native en cours':'Aplicação nativa em execução',
'Application ELF':'Aplicação ELF','Mode par défaut':'Disposição predefinida',
'Mode personnel':'Disposição personalizada',
'Copie terminée':'Cópia concluída','Déplacement terminé':'Movimento concluído',
'Fenêtre active':'Janela ativa','Écran':'Ecrã','Liste':'Lista','Libellés':'Legendas',
'Afficher les libellés':'Mostrar legendas','Présentation':'Apresentação',
'Texte · Entrée : éditer · Échap : commandes':'Texto · Enter: editar · Escape: controlos',
}
translations.update({k:v for k,v in overrides.items() if k in translations})
# Apply only grammatical phrases, after vocabulary adaptation.
fixes={
'uma aplicação oculto':'uma aplicação oculta','uma aplicação ativo':'uma aplicação ativa',
'um aplicação':'uma aplicação','o aplicação':'a aplicação','os aplicações':'as aplicações',
'uma aplicação está ativo':'uma aplicação está ativa','outro aplicação':'outra aplicação',
'outros aplicações':'outras aplicações','aplicação nativo':'aplicação nativa',
'aplicações nativos':'aplicações nativas','aplicação parado':'aplicação parada',
'aplicação concluído':'aplicação concluída','aplicação encerrado':'aplicação encerrada',
'aplicações instalados':'aplicações instaladas','Aplicações instalados':'Aplicações instaladas',
'aplicações integrados':'aplicações integradas','Aplicações integrados':'Aplicações integradas',
'aplicações visíveis':'aplicações visíveis','nenhum aplicação':'nenhuma aplicação',
'Nenhum aplicação':'Nenhuma aplicação','Aplicação nativo':'Aplicação nativa',
'Aplicação parado':'Aplicação parada','Aplicação concluído':'Aplicação concluída',
'aplicação selecionado':'aplicação selecionada','aplicações executados':'aplicações executadas',
'da ecrã':'do ecrã','a ecrã':'o ecrã','na ecrã':'no ecrã','uma ecrã':'um ecrã',
'ecrã apagada':'ecrã apagado','ecrã estática':'ecrã estático','toda a ecrã':'todo o ecrã',
'ecrã inteira':'ecrã inteiro','ecrã desligada':'ecrã desligado','ecrã cheia':'ecrã inteiro',
'disposição aplicado':'disposição aplicada','Disposição aplicado':'Disposição aplicada',
'disposição inválido':'disposição inválida','Disposição inválido':'Disposição inválida',
'Definições continua':'Definições continua','gravação verificado':'gravação verificada',
'Gravação verificado':'Gravação verificada','gravação concluído':'gravação concluída',
'Gravação concluído':'Gravação concluída','gravação incompleto':'gravação incompleta',
'gravações incompletos':'gravações incompletas','um cópia de segurança':'uma cópia de segurança',
'o cópia de segurança':'a cópia de segurança','cópia de segurança inválido':'cópia de segurança inválida',
'Cópia de segurança inválido':'Cópia de segurança inválida','cópia de segurança criado':'cópia de segurança criada',
'copias de segurança':'cópias de segurança','de cópia de segurança':'de cópia de segurança',
'busca bem-sucedida':'pesquisa bem-sucedida','a gravação automático':'a gravação automática',
'Gravação automático':'Gravação automática','gravação automático':'gravação automática',
}
for key,value in translations.items():
 for a,b in sorted(fixes.items(),key=lambda pair:-len(pair[0])):value=re.sub(r'(?<!\w)'+re.escape(a)+r'(?!\w)',lambda m:b,value)
 # Apply noun gender agreement at whole-word boundaries only.
 masculine_to_feminine={'o':'a','os':'as','um':'uma','uns':'umas','ao':'à','aos':'às','do':'da','dos':'das','no':'na','nos':'nas','outro':'outra','outros':'outras','esse':'essa','esses':'essas','este':'esta','estes':'estas','nenhum':'nenhuma','seu':'sua','seus':'suas'}
 feminine_to_masculine={'a':'o','as':'os','uma':'um','à':'ao','da':'do','das':'dos','na':'no','nas':'nos','sua':'seu','suas':'seus','esta':'este','desta':'deste','essa':'esse','nessa':'nesse'}
 def gender(match,mapping):
  word=match.group(1);new=mapping[word.lower()]
  if word[0].isupper():new=new[0].upper()+new[1:]
  return new+match.group(2)
 value=re.sub(r'\b('+'|'.join(masculine_to_feminine)+r')(\s+(?:aplicações|aplicação|gravações|gravação|partilha|transferências|transferência|disposição|consola|cópias de segurança|cópia de segurança))\b',lambda m:gender(m,masculine_to_feminine),value,flags=re.I)
 value=re.sub(r'\b('+'|'.join(feminine_to_masculine)+r')(\s+ecrã)\b',lambda m:gender(m,feminine_to_masculine),value,flags=re.I)
 value=re.sub(r'\b((?:aplicação|gravação|partilha|transferência|disposição|cópia de segurança) (?:nativa )?)(associado|instalado|oculto|ativo|usado|concluído|parado|interrompido|limitado|encerrado|iniciado|criado|liberado|solicitado)\b',lambda m:m.group(1)+m.group(2)[:-1]+'a',value,flags=re.I)
 value=re.sub(r'\b(aplicações )(usados|executados|ordenados|instalados|concluídos)\b',lambda m:m.group(1)+m.group(2)[:-2]+'as',value,flags=re.I)
 value=value.replace('continuam precisando ser','continuam a precisar de ser').replace('enquanto estiver ativo','enquanto estiver ativa')
 value=value.replace('Fazendo cópia de segurança','A criar cópia de segurança')
 translations[key]=value
output={**source,'id':'pt-PT','name':'Português (Portugal)','coverage':'Interface, mensagens e ajuda completas em português europeu','translations':translations}
path=folder/'pt-PT.json';path.write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('European Portuguese:',len(translations),'entries,',path.stat().st_size,'bytes')
