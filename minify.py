import subprocess,os
import gzip,shutil


def minify(t):
  def removeComments(s):
    nest = 0
    print(s)
    c= 0;
    while c <len(s):
      if s[c]=='"':
        if nest == 0 or (c>0 and s[c-1]=="\\"):
          nest+=1
          print ("entering nest",nest,s[0:c])
        else:
          nest-=1
          print ("leaving nest",nest,s[0:c])
      elif c>0 and s[c-1:c+1] == '//' and nest ==0:
        if c==1:
          return ""
        else:
          print ("stripped",s[0:c-1])
          return s[0:c-1]
      elif (s[c-1:c+1] in [ '\\r','\\n','\\d','\\.','\\+']):
          print('nested special',s)
          s = s[:c]+"\\"+s[c:]
          c+=1
      
      c+=1


    return s

  res = []

  for a in t:
    a= a.strip();
    a = removeComments(a);
    d = ""
    for c in range(len(a)):
      if a[c] == "\"" and (c==0 or a[c-1]!="\\"):
        d+="\\\""
      else:
        d+=a[c]

    if(d):
        res+=[str(d).strip()]



  return res

def minifyDict(ls):
  depth = 0;
  res = []
  tmp =""
  for l in ls:
    for c in l:
      if c == '{':
        depth+=1
      elif c=='}':
        depth-=1
    if depth==0:
      tmp+=l
      res+=[tmp]
      tmp = ""
    else:
      tmp+=l
  return res


def getDeps(f):
  from html.parser import HTMLParser

  class MyHTMLParser(HTMLParser):
    def __init__(self):
      HTMLParser.__init__(self)
      self.localSources = []
      self.localSourceStart = None
    def handle_starttag(self, tag, attrs):
      if(tag=="script"):
        for attr in attrs:
          if attr[0]=='src' and not attr[1].startswith("http"):
            self.localSources+=[attr[1]]
            self.localSourceStart = True
    def handle_endtag(self,tag):
      
      if(tag=="script") and (self.localSourceStart==True):
        self.localSourceStart = False

  parser = MyHTMLParser()
  depLines = {}
  num = 0;
  with open(f,'r') as fp:
    for l in fp.readlines():
      parser.feed(l)
      st = parser.localSourceStart
      if st!=None :
        if not parser.localSources[-1] in depLines:
          depLines[parser.localSources[-1]] ={'lines':[num,-1]}
        depLines[parser.localSources[-1]]['lines'][1] = num
        if(st==False):parser.localSourceStart = None
      num+=1
  return depLines;

def addNPMPath():
  if not 'npm-packages' in os.environ['PATH']:
    os.environ['PATH']+=':'+os.path.expanduser('~/.npm-packages/bin')


def uglify(s):
  addNPMPath();
  sex = os.path.splitext(s);
  sout = sex[0]+'.min'+sex[1]
  out = subprocess.check_output(['uglifyjs','-cm','-o',sout,'--warn','--',s])
  with open(sout,'r') as fp:
    data = ''.join(fp.readlines());
  print(out)
  return {'name':sout,'data':data}

def htmlmin(s,sout):
  addNPMPath();

  ls = ['html-minifier','--minify-css','true','--minify-js','true','--remove-comments','--html5','--remove-tag-whitespace ','--collapse-whitespace ','--remove-redundant-attributes','-o',sout,'--',s]
  out = subprocess.check_output(ls)
  print(out)
  return sout

def replace_deps(inHtml,deps,sout):
  
    with open(inHtml,'r') as fp:
      l = fp.readlines();
      for k,v in deps.items():
        start = v['lines'][0]
        end = max(start,v['lines'][1])
        for k in range(start,end+1):
          del l[k]

        l = l[:start] +[str('<script>'+ v['min']['data'] + '</script>')]+l[start:]
    with open(sout,'w') as outf:
      outf.writelines(l)

def removeEmptyLines(inf,outf):
  lines = []
  with open(inf,'r') as fp:
    for l in fp.readlines():
      n = l.strip()
      if n :
        lines+=[n]
      else:
        print('removing')
  with open(outf,'w') as fp:
    fp.write('\n'.join(lines))




if __name__ == "__main__":
  pack = True
  file = "index.html"
  deps = getDeps(file)
  
  
  for d in deps.keys():
    deps[d]['min']=uglify(d)
  
  sex = os.path.splitext(file);
  htmlMin = sex[0]+'.min'+sex[1]
  if pack:
    shutil.copyfile(file,htmlMin)
    replace_deps(htmlMin,deps,htmlMin)
    htmlmin(htmlMin,htmlMin)
  else:
    htmlmin(file,htmlMin)

  removeEmptyLines(htmlMin,htmlMin)

  
  filesToCompress = [htmlMin]
  if not pack:
    filesToCompress+=[v['min']['name'] for v in deps.values()]

  for f in filesToCompress:
    localgz = 'FS/'+f+'.gz'
    
    print (f,localgz)
    with open(f, 'rb') as f_in, gzip.open(localgz, 'wb',compresslevel=9) as f_out:
      shutil.copyfileobj(f_in, f_out)

  # exit()
  
#   oneline = True
#   with open(file,'r') as fp:
#     lines = fp.readlines();
#     minl = minify(lines)

#   minl = minifyDict(minl)
#   # for l in minl:
#     # print(l)

#   stringT = "".join(minl)
#   # print( eval("\"%s\"%(stringT)"))


#   import os,shutil
#   sex = os.path.splitext(file)
#   minPath = sex[0]+".min"+sex[1]
#   with open(minPath,'w') as fp:
#     minlH = []
#     for i in range(len(minl)):
#       x = minl[i]
#       comp = x.replace("\\\"","\"")
#       comp=comp.replace("\\\\r","\\r")
#       comp=comp.replace("\\\\n","\\n")
#       comp=comp.replace("\\\\d","\\d")
#       comp=comp.replace("\\\\.","\\.")
#       comp=comp.replace("\\\\+","\\+")
#       if not oneline :
#         comp+='\n'
#       else:
#         l = comp[-1]
#         n = ''
#         if i < len(minl)-1:
#           n = minl[i+1][0]
#         if not l in ['{','}','>',','] and (l != ';') and n not in ['{','}']:
#           comp+=';'
#       minlH+=[comp]
#     fp.writelines(minlH)

#   localgz = 'FS/'+minPath+'.gz'
#   import gzip
#   print (minPath,localgz)
#   with open(minPath, 'rb') as f_in, gzip.open(localgz, 'wb',compresslevel=9) as f_out:
#     shutil.copyfileobj(f_in, f_out)



  from ftplib import FTP
  import socket
  print('getting ip');
  ip = socket.gethostbyname('smartpotier.local')
  print(ip)
  print('logging in');
  ftp = FTP(ip,user='esp32',passwd='esp32')
  print('logged in');
  def advance(v):
    print("block done : ",len(v))


  with open(localgz,'rb') as fp:
    ftp.storbinary("STOR "+os.path.basename(localgz),fp,callback=advance)
  # with open('httpoterie.h','w') as fp:
  #   fp.writelines(['const char * http_page = '])
  #   minlH = ['"'+x+'\\n"\n' for x in minl]
  #   fp.writelines(minlH+[";"])


