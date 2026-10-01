#!/usr/bin/env python3
"""Compile the verbatim KentOS system library into PiriCAD's shipped catalogue.

No source style is replaced by a scanned screenshot. Lines/hatches remain native
symbol layers; compound marks and marker tiles are self-contained SVG geometry.
Source identities, category paths, tags, references and the complete kstil remain.
Run from any directory; --check verifies the generated files without writing.
"""
from pathlib import Path
import argparse, collections, hashlib, html, json, math, re, xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'data/styles/assets'
SOURCE = ASSETS / 'system-library.kstil'
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
j = json.loads(SOURCE.read_text())
assets = {x['id']: x for x in j['items'] if x['kind'] == 'asset'}
outputs, pictures, counts = {}, [], collections.Counter()

def num(v): return format(float(v), '.9g')
def esc(v): return html.escape(str(v), quote=True)
def col(v):
    if v is None: return 'none'
    return {'ink': 'currentColor', 'paper': '#FFFFFF', 'fg': 'currentColor', 'fg-dim': '#666666'}.get(v, v)
def rgba(v):
    if v is None: return '#00000000'
    v = {'ink':'#000000','paper':'#FFFFFF','fg':'#000000','fg-dim':'#666666'}.get(v,v)
    if len(v)==7: return '#FF'+v[1:]
    assert len(v)==9, v
    return '#'+v[7:9]+v[1:7]
def tag(name, **attrs):
    return '<'+name+' '+ ' '.join(k.replace('_','-')+'="'+esc(v)+'"' for k,v in attrs.items())+'/>'
def svg(body,w,h, **attrs):
    return ('<svg xmlns="'+NS+'" width="'+num(w)+'" height="'+num(h)+'" viewBox="'+
            ' '.join(map(num,(-w/2,-h/2,w,h)))+'" '+
            ' '.join(k.replace('_','-')+'="'+esc(v)+'"' for k,v in attrs.items())+'>'+body+'</svg>')
def polygon(pts):
    return 'M'+' L'.join(num(x)+','+num(y) for x,y in pts)+' Z'
def path_shape(m):
    r=m['size']/2;hw=r;hh=m.get('height',m['size'])/2;shape=m['shape']
    p=''
    if shape in ('circle','ring'): p=f'M{num(r)},0 A{num(r)},{num(r)} 0 1 0 {num(-r)},0 A{num(r)},{num(r)} 0 1 0 {num(r)},0 Z'
    elif shape in ('square','rectangle'):
        if shape=='square': hh=min(hw,hh);hw=hh
        p=polygon([(-hw,-hh),(hw,-hh),(hw,hh),(-hw,hh)])
    elif shape=='diamond':p=polygon([(0,hh),(hw,0),(0,-hh),(-hw,0)])
    elif shape=='triangle':
        h=r*math.sqrt(3);p=polygon([(0,2*h/3),(r,-h/3),(-r,-h/3)])
    elif shape in ('pentagon','hexagon','octagon','star'):
        n={'pentagon':5,'hexagon':6,'octagon':8,'star':10}[shape]
        base=r if shape=='star' else r/math.cos(math.pi/n)
        rot=math.pi/8 if shape=='octagon' else math.pi/2
        p=polygon([(math.cos(rot+i*2*math.pi/n)*base*(.4 if shape=='star' and i%2 else 1),
                    math.sin(rot+i*2*math.pi/n)*base*(.4 if shape=='star' and i%2 else 1)) for i in range(n)])
    elif shape in ('semicircle','quartercircle','arc'):
        if shape=='arc':
            sweep=math.radians(m.get('sweep',180));start=math.pi/2-sweep/2
        else:start=0;sweep=math.pi if shape=='semicircle' else math.pi/2
        pts=[(r*math.cos(start+i*sweep/64),r*math.sin(start+i*sweep/64)) for i in range(65)]
        if shape=='quartercircle':pts.insert(0,(0,0))
        p=polygon(pts) if shape!='arc' else polygon(pts)[:-2]
    elif shape=='arrowhead': p=polygon([(hw,0),(-hw,hh*.8),(-hw,-hh*.8)])
    elif shape=='chevron':p=f'M{num(-hw)},{num(hh)} L{num(hw)},0 L{num(-hw)},{num(-hh)}'
    elif shape=='line':p=f'M{num(-hw)},0 L{num(hw)},0'
    elif shape=='cross':p=f'M{num(-r)},0 L{num(r)},0 M0,{num(-r)} L0,{num(r)}'
    elif shape=='x':
        d=r/math.sqrt(2);p=f'M{num(-d)},{num(-d)} L{num(d)},{num(d)} M{num(-d)},{num(d)} L{num(d)},{num(-d)}'
    elif shape=='arrow':p=f'M{num(-hw)},0 L{num(hw)},0 M{num(hw*.5)},{num(hh*.4)} L{num(hw)},0 L{num(hw*.5)},{num(-hh*.4)}'
    elif shape=='gear':
        n=max(3,round(m.get('teeth',12))); rb=r*(1-m.get('teethDepth',.2));a=2*math.pi/n
        hw=.25*a*(rb+(r-rb)/2);foot=math.sqrt(max(rb*rb-hw*hw,0));side=math.atan2(hw,foot);pts=[]
        for k in range(n):
            for x,y in ((foot,-hw),(r,-hw),(r,hw),(foot,hw)):
                pts.append((x*math.cos(k*a)-y*math.sin(k*a),x*math.sin(k*a)+y*math.cos(k*a)))
            for i in range(1,9):
                t=k*a+side+i*(a-2*side)/8;pts.append((rb*math.cos(t),rb*math.sin(t)))
        p=polygon(pts)
    else: raise ValueError('Unknown shape '+shape)
    if m.get('hole',0)>0:
        hr=r*m['hole'];p+=f' M{num(hr)},0 A{num(hr)},{num(hr)} 0 1 0 {num(-hr)},0 A{num(hr)},{num(hr)} 0 1 0 {num(hr)},0 Z'
    return p

OPEN={'cross','x','line','arrow','chevron','arc'}
FONTS={'sans':'Arial, Liberation Sans, sans-serif','serif':'Times New Roman, Liberation Serif, serif','mono':'IBM Plex Mono, monospace','narrow':'Arial Narrow, Liberation Sans Narrow, sans-serif','ui':'Barlow, sans-serif'}
class Expression:
    def __init__(self, source):
        self.tokens = re.findall(r"\[[^\]]+\]|'(?:''|[^'])*'|\$[^\s()*/,]+|[\wçğıöşüÇĞİÖŞÜ]+(?:\.\d+)?|\|\||[(),+*/-]", source)
        self.at = 0
    def take(self, token=None):
        v = self.tokens[self.at] if self.at < len(self.tokens) else ''
        if token is not None and v != token: raise ValueError('Expression expected '+token+', got '+v)
        self.at += 1
        return v
    def value(self, precedence=0):
        token=self.take()
        if token=='(':
            left=self.value();self.take(')')
        elif token=='-':left=-float(self.value(4))
        elif token.startswith("'"):left=token[1:-1].replace("''", "'")
        elif token.startswith('['):left=None
        elif token=='$ölçek':left=1000
        elif re.fullmatch(r'\d+(?:\.\d+)?',token):left=float(token)
        else:
            self.take('(');values=[]
            if self.tokens[self.at]!=')':
                while True:
                    values.append(self.value())
                    if self.tokens[self.at]!=',':break
                    self.take(',')
            self.take(')')
            if token=='varsayılan':left=next((v for v in values if v is not None and v!=''),None)
            elif token=='boş':left=values[0] in (None,'')
            elif token=='eğer':left=values[1] if values[0] else values[2]
            elif token=='büyük':left=str(values[0] or '').replace('i','İ').replace('ı','I').upper()
            else:raise ValueError('Unknown source function '+token)
        operators={'||':1,'ve':1,'+':2,'-':2,'*':3,'/':3}
        while self.at<len(self.tokens):
            op=self.tokens[self.at];level=operators.get(op,0)
            if level<=precedence:break
            self.take();right=self.value(level)
            if op=='||':left=str(left or '')+str(right or '')
            elif op=='ve':left=bool(left) and bool(right)
            elif op=='+':left=float(left or 0)+float(right or 0)
            elif op=='-':left=float(left or 0)-float(right or 0)
            elif op=='*':left=float(left or 0)*float(right or 0)
            elif op=='/':left=float(left or 0)/float(right)
        return left

def default(v):
    if isinstance(v,dict):
        expression=Expression(v['expr']);result=expression.value()
        assert expression.at==len(expression.tokens), v['expr']
        return v.get('fallback') if result is None else result
    return v

def marker(m):
    """SVG geometry in paper mm; nested offsets rotate with their own marker."""
    t=m['type']; counts[t]+=1
    m={k:default(v) if isinstance(v,dict) and 'expr' in v and k!='text' else v for k,v in m.items()}
    size=default(m['size']);off=m.get('offset',[0,0]); rotation=default(m.get('rotation',0))
    if t=='shape':
        open_=m['shape'] in OPEN;stroke=m.get('stroke')
        if open_ and not stroke:stroke=m.get('fill','ink')
        body=tag('path', d=path_shape(m), fill='none' if open_ or m['shape']=='ring' else col(m.get('fill')), stroke=col(stroke), stroke_width=m.get('strokeWidth',.2 if stroke else 0), fill_rule='evenodd', stroke_linejoin='round', stroke_linecap='round')
        body='<g transform="scale(1,-1)">'+body+'</g>'
        h=m.get('height',size);reach=max(size,h)/2+m.get('strokeWidth',0)
    elif t=='svg':
        asset=assets[m['asset']];root=ET.fromstring(asset['data']);vb=list(map(float,root.attrib.get('viewBox','0 0 100 100').split()));h=size*asset['height']/asset['width']
        attrs={k:v for k,v in root.attrib.items() if k in ('fill','stroke','stroke-width','stroke-linejoin','stroke-linecap','fill-rule','opacity')}
        transform='translate('+num(-size/2)+','+num(-h/2)+') scale('+num(size/vb[2])+','+num(h/vb[3])+') translate('+num(-vb[0])+','+num(-vb[1])+')'
        body='<g transform="'+transform+'" '+ ' '.join(k+'="'+esc(v)+'"' for k,v in attrs.items())+'>'+''.join(ET.tostring(child,encoding='unicode') for child in root)+'</g>'
        body=body.replace('param(fill)',col(m.get('fill','ink'))).replace('param(stroke)',col(m.get('stroke','ink')))
        body=body.replace('currentColor',col(m.get('fill','ink')))
        reach=max(size,h)/2
    elif t=='text':
        expression=m['text'].get('expr') if isinstance(m['text'],dict) else None
        text=default(m['text']) or ''
        anchor=m.get('anchor','center');align='start' if anchor=='left' else 'end' if anchor=='right' else 'middle'
        color=col(m.get('color','ink'));halo=m.get('halo');attrs=dict(x=0,y=0,font_size=size,font_family=FONTS[m.get('font','ui')],font_weight=m.get('weight',400),text_anchor=align,dominant_baseline='central')
        base='<text '+('data-kentos-expr=\"'+expression.encode().hex()+'\" ' if expression else '')+ ' '.join(k.replace('_','-')+'="'+esc(v)+'"' for k,v in attrs.items())
        body=''
        if halo:body+=base+' fill="none" stroke="'+col(halo['color'])+'" stroke-width="'+num(halo['width']*2)+'" stroke-linejoin="round">'+esc(text)+'</text>'
        body+=base+' fill="'+color+'">'+esc(text)+'</text>'
        # Symmetric canvas keeps off-centre text attached to the geometry centre.
        reach=max(size*len(text)*.6,size)+ (halo['width'] if halo else 0)
    else:raise ValueError('Unknown marker '+t)
    reach+=max(abs(off[0]),abs(off[1]))
    return ('<g transform="rotate('+num(-rotation)+') translate('+num(off[0])+','+num(-off[1])+')">'+body+'</g>',max(reach,.01)*2)
def marker_symbol(sym):
    assert sym['type']=='marker'
    parts=[marker(m) for m in sym['layers']]
    return ''.join(p[0] for p in parts),max(p[1] for p in parts)
def picture(id_,body,w,h,**attrs):
    raw=svg(body,w,h,data_piricad='kentos',**attrs)
    path='generated/'+id_+'.svg';outputs[ASSETS/path]=raw+'\n'
    key='svg-'+hashlib.sha256(raw.encode()).hexdigest()[:20]
    pictures.append(dict(id=key,dosya=path,sha256=hashlib.sha256((raw+'\n').encode()).hexdigest(),bayt=len((raw+'\n').encode()),tur='svg'))
    return key

def measure(v,unit='mm'):return round(v*(1 if unit=='px' else 1000))
def base_layer(t):return dict(tip=t,birim='kagit')
def compile_layer(layer,id_):
    t=layer['type'];counts[t]+=1;unit=layer.get('unit','mm');out=base_layer('cizgi')
    original=layer
    layer={k:default(v) if isinstance(v,dict) and 'expr' in v else v for k,v in layer.items()}
    out['birim']={'mm':'kagit','px':'piksel','m':'zemin'}[unit]
    if t=='simpleFill':out.update(tip='dolgu',dolgu_renk=rgba(layer['color']))
    elif t=='simpleLine' and 'wave' not in layer:
        out.update(renk=rgba(layer['color']),kalinlik=measure(layer['width']),kaydirma=measure(layer.get('offset',0)),uc={'butt':'duz','round':'yuvarlak','square':'kare'}[layer.get('cap','butt')],birlesim={'miter':'kose','round':'yuvarlak','bevel':'pah'}[layer.get('join','miter')])
        if layer.get('dash'):out['desen']=[v/max(layer['width'],.001) for v in layer['dash']]
    elif t=='hatchFill':
        out.update(tip='cizgi-desen-dolgu',renk=rgba(layer['color']),kalinlik=measure(layer['width']),aci=round(layer['angle']*1e6),aralik=measure(layer['spacing']),kaydirma=measure(layer.get('offset',0)))
        if layer.get('dash'):out['desen']=[v/max(layer['width'],.001) for v in layer['dash']]
    elif t=='markerLine':
        body,size=marker_symbol(layer['marker']);group=layer.get('group',{'count':1,'spacing':0})
        if group['count']>1:
            body=''.join('<g transform="translate('+num(k*group['spacing'])+',0)">'+body+'</g>' for k in range(group['count']))
            size+=2*(group['count']-1)*group['spacing']
        key=picture(id_,body,size,size,data_fixed_pitch='1',data_rotate='1' if layer.get('rotate',False) else '0',data_phase=layer.get('offsetAlong',0),data_placement=layer['placement'])
        out.update(tip='gorsel-cizgi',gorsel=key,boyut=measure(size),aralik=measure(layer.get('interval',0)),faz=measure(layer.get('offsetAlong',0)),kaydirma=measure(layer.get('offset',0)),yerlesim={'interval':'aralik','vertex':'tepe','first':'ilk','last':'son','center':'orta','innerVertex':'tepe','segmentCenter':'orta'}[layer['placement']])
    elif t=='centroidMarker' or t in ('shape','svg','text'):
        body,size=marker_symbol(layer['marker']) if t=='centroidMarker' else marker(layer)
        key=picture(id_,body,size,size)
        out.update(tip='gorsel-isaretci',gorsel=key,boyut=measure(size,unit))
    elif t=='patternFill':
        body,size=marker_symbol(layer['marker']);sx=layer['spacingX'];sy=layer['spacingY'];stagger=layer.get('stagger',False);scatter=layer.get('jitter',0)
        cols=16 if scatter else 1;rows=16 if scatter else 2 if stagger else 1;w=sx*cols;h=sy*rows;parts=[];off=layer.get('offset',[0,0]);seed=layer.get('seed',0)
        for y in range(-1,rows+1):
            for x in range(-1,cols+1):
                # The same per-cell hash as KentOS's preview and pattern shader.
                q0=((x%cols+seed*17.31)*.1031)%1
                q1=((y%rows+seed*7.73)*.103)%1
                q2=((x%cols+seed*17.31)*.0973)%1
                d=q0*(q1+33.33)+q1*(q0+33.33)+q2*(q2+33.33)
                q0+=d;q1+=d;q2+=d
                hs=(((q0+q1)*q2)%1,((q0+q2)*q1)%1,((q1+q2)*q0)%1)
                if hs[2]>layer.get('coverage',1):continue
                jx=(hs[0]-.5)*max(0,sx-size)*scatter
                jy=(hs[1]-.5)*max(0,sy-size)*scatter
                px=(x+.5)*sx-w/2+(sx/2 if stagger and y%2 else 0)+off[0]+jx
                py=(y+.5)*sy-h/2-off[1]+jy
                parts.append('<g transform="translate('+num(px)+','+num(py)+')">'+body+'</g>')
        key=picture(id_,''.join(parts),w,h)
        out.update(tip='gorsel-dolgu',gorsel=key,boyut=measure(h),aci=round(layer.get('angle',0)*1e6))
    elif t=='simpleLine' and 'wave' in layer:
        wave=layer['wave'];length=wave['length'];pitch=wave.get('spacing',length);amp=wave['amplitude'];width=layer['width'];height=amp*2+width*2;pts=[]
        for k in range(65):
            x=k/64*length
            if wave['shape']=='sine':y=amp*math.sin(2*math.pi*k/64)
            elif wave['shape']=='zigzag':y=amp*(1-abs((k/64*4+1)%4-2))
            elif wave['shape']=='square':y=amp*(1 if k<32 else -1)
            else:raise ValueError('Unknown wave '+wave['shape'])
            pts.append((x-pitch/2,-y))
        if wave.get('connect',True):pts.append((pitch/2,0))
        d=polygon(pts)[:-2];body=tag('path',d=d,fill='none',stroke=col(layer['color']),stroke_width=width)
        key=picture(id_,body,pitch,height,data_fixed_pitch='1',data_rotate='1',data_phase=wave.get('offsetAlong',0)+pitch/2,data_placement='interval')
        out.update(tip='gorsel-cizgi',gorsel=key,boyut=measure(height),aralik=measure(pitch),faz=measure(wave.get('offsetAlong',0)+pitch/2),yerlesim='aralik',kaydirma=measure(layer.get('offset',0)))
    else:raise ValueError('Unknown symbol layer '+t)
    expressions={}
    for name, native in (('width','kalinlik'),('offset','kaydirma'),('size','boyut'),('interval','aralik')):
        if isinstance(original.get(name),dict) and 'expr' in original[name]:
            expressions[native]={'ifade':original[name]['expr'],'katsayi':1000 if unit!='px' else 1}
    if expressions:out['ifadeler']=expressions
    return out

styles=[]
for item in j['items']:
    id_=item['id']
    if item['kind']=='asset':
        path='/'.join(id_.split('.'))+'.svg';outputs[ASSETS/path]=item['data']+'\n'
        symbol={'type':'marker','layers':[{'id':'asset','type':'svg','asset':id_,'size':14,'fill':'ink'}]}
    else:symbol=item['symbol']
    layers=[compile_layer(l,id_.replace('.','/')+'/'+str(i)) for i,l in enumerate(symbol['layers'])]
    styles.append(dict(id=id_,ad=item['name'],bolum=item.get('path',[]),etiketler=item.get('tags',[]),kaynak=item.get('reference',item.get('description','KentOS sistem kütüphanesi')),geometri={'fill':'alan','line':'cizgi','marker':'nokta'}[symbol['type']],katmanlar=layers))
package=dict(schema_version=1,package_version='1.0.0',id='kentos-system',source='KentOS CAD sistem stil kütüphanesi — özgün sembol tanımları',published='2026-10-01',licence='kaynak-proje',kaynak_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),gorseller=list({p["id"]:p for p in reversed(pictures)}.values())[::-1],stiller=styles,kurallar=[])
outputs[ASSETS/'system-library.json']=json.dumps(package,ensure_ascii=False,indent=2)+'\n'
for path,data in outputs.items():
    if args.check:
        if not path.exists() or path.read_text()!=data:raise SystemExit('Generated style differs: '+str(path))
    else:path.parent.mkdir(parents=True,exist_ok=True);path.write_text(data)
print(f'{len(styles)} entries, {len(pictures)} vector motifs; source {package["kaynak_sha256"]}')
