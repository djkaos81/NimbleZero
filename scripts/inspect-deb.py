#!/usr/bin/env python3
"""Inspect Debian packages: control fields, maintainer scripts, layout and binary requirements.
usage: inspect-deb.py [--json] a.deb b.deb ...   (needs dpkg-deb, readelf)"""
import sys,subprocess,tempfile,os,re,json
def run(c): return subprocess.run(c,capture_output=True,text=True,errors='replace').stdout
res=[]
for deb in [a for a in sys.argv[1:] if not a.startswith('--')]:
    t=tempfile.mkdtemp(); subprocess.run(['dpkg-deb','-R',deb,t],check=True)
    ctl={}
    for l in open(os.path.join(t,'DEBIAN/control'),errors='replace'):
        if ':' in l and not l.startswith(' '): k,v=l.split(':',1); ctl[k]=v.strip()
    scripts={s:open(os.path.join(t,'DEBIAN',s),errors='replace').read() for s in ('preinst','postinst','prerm','postrm','conffiles','triggers') if os.path.exists(os.path.join(t,'DEBIAN',s))}
    needed=set(); glibc=set(); glibcxx=set(); elfs=0; interp=set(); tops={}; units=[]; desktops=[]
    for root,_,fs in os.walk(t):
        if root.startswith(os.path.join(t,'DEBIAN')): continue
        for f in fs:
            p=os.path.join(root,f); rel=p[len(t):]
            top='/'.join(rel.split('/')[:4]); tops[top]=tops.get(top,0)+ (os.path.getsize(p) if not os.path.islink(p) else 0)
            if rel.endswith(('.service','.timer','.socket')): units.append(rel)
            if rel.endswith('.desktop'): desktops.append(rel)
            if os.path.islink(p): continue
            try:
                if open(p,'rb').read(4)!=b'\x7fELF': continue
            except Exception: continue
            elfs+=1
            d=run(['readelf','-dlW',p])
            needed|=set(re.findall(r'NEEDED\)\s+Shared library: \[(.*?)\]',d))
            interp|=set(re.findall(r'interpreter: (.*?)\]',d))
            v=run(['readelf','-VW',p])
            glibc|=set(re.findall(r'GLIBC_([0-9.]+)',v)); glibcxx|=set(re.findall(r'GLIBCXX_([0-9.]+)',v))
    key=lambda s: tuple(int(x) for x in s.split('.') if x.isdigit())
    res.append(dict(file=os.path.basename(deb),package=ctl.get('Package'),version=ctl.get('Version'),arch=ctl.get('Architecture'),
        depends=ctl.get('Depends',''),predepends=ctl.get('Pre-Depends',''),recommends=ctl.get('Recommends',''),installed_size_kb=ctl.get('Installed-Size'),
        scripts={k:len(v.splitlines()) for k,v in scripts.items()},script_cmds=sorted(set(re.findall(r'\b(systemctl|loginctl|update-rc\.d|deb-systemd-helper|deb-systemd-invoke|adduser|useradd|usermod|groupadd|udevadm|ldconfig|setcap|apt-get|apt|dpkg|pip3?|update-desktop-database|gtk-update-icon-cache|chown|runuser|su|dbus-send|busctl|journalctl|pkill|killall)\b',' '.join(scripts.values())))),
        elf_count=elfs,interp=sorted(interp),needed=sorted(needed),max_glibc=max(glibc,key=key) if glibc else None,max_glibcxx=max(glibcxx,key=key) if glibcxx else None,
        units=units,desktops=desktops,top_dirs={k:v//1024 for k,v in sorted(tops.items(),key=lambda x:-x[1])[:8]}))
    subprocess.run(['rm','-rf',t])
if '--json' in sys.argv: print(json.dumps(res,indent=1))
else:
    for r in res:
        print(f"## {r['file']}\n {r['package']} {r['version']} {r['arch']} inst={r['installed_size_kb']}kB elf={r['elf_count']} glibc<={r['max_glibc']} glibcxx<={r['max_glibcxx']}\n depends: {r['depends']}\n pre: {r['predepends']} rec: {r['recommends']}\n scripts: {r['scripts']} cmds: {r['script_cmds']}\n needed: {' '.join(r['needed'])}\n units: {r['units']}\n desktops: {r['desktops']}\n dirs(kB): {r['top_dirs']}")
