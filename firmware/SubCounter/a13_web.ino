// ════════════════════════════════════════════════════════════════════════════
//  Web: dashboard (/), data API, settings (/settings)
// ════════════════════════════════════════════════════════════════════════════
const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter setup</title><style>
body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{max-width:440px;margin:0 auto;background:#1c1c1e;border-radius:14px;padding:22px}
h1{font-size:22px;margin:0 0 4px}p{color:#999;font-size:14px;margin:0 0 18px}
label{display:block;font-size:13px;color:#aaa;margin:14px 0 6px}
input,select,textarea{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #333;background:#000;color:#fff;font-size:16px;font-family:inherit}
textarea{min-height:150px}
button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:10px;background:#e62117;color:#fff;font-size:17px;font-weight:600}
small{display:block;color:#777;font-size:12px;margin-top:6px}a{color:#4ea1ff}
.st{font-size:13px;color:#aaa;margin-top:6px}.st b{color:#fff}
h2{font-size:17px;margin:30px 0 4px;padding-top:16px;border-top:1px solid #2a2a2e;scroll-margin-top:12px}
.jump{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 14px}.jump a{font-size:13px;background:#2a2a2e;color:#ddd;border-radius:999px;padding:5px 10px;text-decoration:none}
summary{margin-top:14px;cursor:pointer;color:#aaa;font-size:14px}
.row{display:flex;gap:8px;align-items:center}.row span{color:#aaa}
.b2{background:#2a2a2e;font-size:15px;padding:12px;margin-top:10px}
.mini{width:auto;margin:0;padding:5px 10px;font-size:13px;background:#2a2a2e}
.btnlink{display:block;text-align:center;margin-top:10px;padding:12px;border-radius:10px;background:#1db954;color:#fff;font-weight:600;text-decoration:none}
.saved{display:flex;flex-wrap:wrap;gap:6px 14px;align-items:center;padding:8px 0;border-top:1px solid #333}.saved b{flex:1 1 100%;word-break:break-word;color:#fff}
.ok{color:#30d158;margin:6px 0}
.secret{-webkit-text-security:disc;text-security:disc}
.savebar{position:sticky;bottom:0;background:#1c1c1e;padding:6px 0 10px;margin-top:22px;box-shadow:0 -10px 14px #1c1c1e}.savebar button{margin-top:6px}
</style></head><body><div class="card">)HTML";


const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter</title>
<style>
:root{--bg:#0d0d0f;--card:#18181b;--line:#2a2a2e;--text:#f2f2f3;--muted:#8d8d95;--red:#ff3b30;--gold:#ffc53d;--green:#30d158;--blue:#4da3ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,system-ui,Segoe UI,Roboto,sans-serif}
a{color:inherit;text-decoration:none}
header{display:flex;align-items:center;gap:12px;padding:16px 20px;border-bottom:1px solid var(--line);position:sticky;top:0;background:rgba(13,13,15,.92);backdrop-filter:blur(8px);z-index:2}
.logo{width:34px;height:24px;border-radius:7px;background:var(--red);display:grid;place-items:center}
.logo:after{content:"";border-left:10px solid #fff;border-top:6px solid transparent;border-bottom:6px solid transparent;margin-left:3px}
header h1{font-size:18px;margin:0;flex:1}header .meta{color:var(--muted);font-size:13px}
.btn{border:1px solid var(--line);border-radius:9px;padding:7px 12px;font-size:13px;color:var(--text);background:var(--card);cursor:pointer}
main{max-width:1500px;margin:0 auto;padding:20px}
.layout{display:grid;grid-template-columns:340px minmax(0,1fr);gap:16px;align-items:start}
.side{display:grid;gap:16px;align-content:start}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));align-items:start}
@media(max-width:1000px){.layout{grid-template-columns:1fr}.side{grid-template-columns:repeat(auto-fit,minmax(280px,1fr));align-items:start}}
@media(max-width:600px){main{padding:12px}.side,.grid{grid-template-columns:1fr}.big{font-size:38px}.vid img{width:104px}header{padding:12px}header .meta{white-space:nowrap;font-size:12px}}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:18px}
.card h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--gold);margin:0 0 12px}
.err{background:#3a1210;border-color:#e62117}
.av{width:44px;height:44px;border-radius:50%;object-fit:cover;background:#333;flex:none}
.av.lg{width:64px;height:64px}
.avw{position:relative;display:inline-flex;flex:none;border-radius:50%}
.avw.on{box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--red)}.avw.on.lg{box-shadow:0 0 0 3px var(--card),0 0 0 6px var(--red)}
.avw .lv{position:absolute;left:50%;bottom:-7px;transform:translateX(-50%);background:var(--red);color:#fff;border:2px solid var(--card);border-radius:5px;padding:0 4px;font-size:9px;line-height:13px;font-weight:800;letter-spacing:.03em}
.avw.lg .lv{font-size:11px;line-height:15px;padding:0 6px;bottom:-9px}
td .avw .lv{font-size:7px;line-height:10px;padding:0 3px;bottom:-5px;border-width:1px}td .avw.on{box-shadow:0 0 0 1px var(--card),0 0 0 3px var(--red)}
.top{display:flex;gap:12px;align-items:center}.top .nm{font-weight:700;font-size:17px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sub{color:var(--muted);font-size:13px}
.big{font-size:44px;font-weight:800;letter-spacing:-.02em;margin:10px 0 2px;font-variant-numeric:tabular-nums}
.est{font-size:12px;color:var(--muted);font-weight:500;margin-left:6px;letter-spacing:0}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}
.chip{background:#222226;border-radius:9px;padding:6px 10px;font-size:13px}.chip b{font-size:15px}
.pos{color:var(--green)}.neg{color:var(--red)}
.bar{height:10px;border-radius:6px;background:#2a2a2e;overflow:hidden;margin:8px 0 4px}.bar i{display:block;height:100%;background:var(--gold);border-radius:6px}
.vid{display:flex;gap:12px;margin-top:14px;padding-top:14px;border-top:1px solid var(--line)}
.vid img{width:128px;aspect-ratio:16/9;object-fit:cover;border-radius:8px;flex:none;background:#333}
.vid .t{font-weight:600;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.live{background:var(--red);color:#fff;border-radius:5px;padding:1px 6px;font-size:11px;font-weight:800;margin-right:6px}
svg.g{width:100%;height:120px;display:block;margin-top:12px}
.tabs{display:flex;gap:6px;margin-top:12px}.tabs button{border:0;border-radius:8px;padding:5px 12px;font-size:13px;background:#222226;color:var(--muted);cursor:pointer}.tabs button.on{background:#3a3a40;color:var(--text)}
table{width:100%;border-collapse:collapse}td{padding:8px 4px;border-top:1px solid var(--line)}td.n{text-align:right;font-variant-numeric:tabular-nums}
tr.me td{color:var(--gold)}
th{font-size:11px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted);padding:0 4px 6px}th.n{text-align:right}tr.hd+tr td{border-top:0}
.race .rs{display:flex;justify-content:space-between;align-items:center;margin:8px 0}.race .rs span{display:flex;align-items:center;gap:10px;min-width:0}
td .av{width:28px;height:28px;display:block}td{padding:6px 4px}td.nm2{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:150px}
.tug{display:flex;height:14px;border-radius:7px;overflow:hidden;margin:10px 0}.tug .a{background:var(--red)}.tug .b{background:var(--blue)}
.list div{display:flex;justify-content:space-between;padding:5px 0}
.wide{grid-column:1/-1}
.twb{background:#9146ff;color:#fff;border-radius:5px;font-size:10px;font-weight:800;padding:1px 5px;margin-left:8px;vertical-align:3px;letter-spacing:.03em}
.ctl{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:12px}
.tg{display:inline-flex;align-items:center;gap:6px;border:1px solid var(--line);border-radius:999px;padding:4px 10px;font-size:13px;background:none;color:var(--muted);cursor:pointer}
.tg.on{color:var(--text);border-color:#4a4a52;background:#222226}.tg i{width:10px;height:10px;border-radius:50%;display:inline-block}
.seg{display:inline-flex;background:#222226;border-radius:9px;padding:2px}.seg button{border:0;background:none;color:var(--muted);padding:4px 10px;border-radius:7px;font-size:13px;cursor:pointer}.seg button.on{background:#3a3a40;color:var(--text)}
.cmp{position:relative}.cmp svg{width:100%;height:260px;display:block}
.tip{position:absolute;pointer-events:none;background:#0d0d0f;border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:12px;white-space:nowrap;display:none;z-index:1}
.tip div{display:flex;gap:8px;align-items:center}.tip i{width:8px;height:8px;border-radius:50%;display:inline-block}
.small{font-size:12px;color:var(--muted);text-decoration:underline;margin-left:auto}
.cm{margin-top:14px;padding-top:12px;border-top:1px solid var(--line)}.cm h3{font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);margin:0 0 6px}
.cm .c{padding:6px 0}.cm .c b{font-size:13px}.cm .c p{margin:2px 0 0;font-size:14px;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.rvt td.nm2{max-width:200px}.rvt th:first-child{text-align:left}.rvt th a{color:inherit;text-decoration:none}details.cm summary{cursor:pointer;list-style:revert}
.pb{display:flex;flex-wrap:wrap;gap:6px 14px;font-size:13px;color:var(--muted);margin-top:10px}.pb b{color:var(--gold)}
</style></head><body>
<header><div class="logo"></div><h1>SubCounter</h1><span class="meta" id="meta"></span><a class="btn" id="upd" href="/update" style="display:none;border-color:#30d158;color:#30d158"></a><a class="btn" href="/settings">Settings</a></header>
<main id="app"><div class="card">Loading…</div></main>
<script>
const $=s=>document.querySelector(s);
function h(t,a,...k){const e=document.createElement(t);for(const x in a||{}){if(x=='class')e.className=a[x];else if(x=='text')e.textContent=a[x];else if(x.startsWith('on'))e[x]=a[x];else e.setAttribute(x,a[x])}for(const c of k.flat())if(c!=null)e.append(c.nodeType?c:document.createTextNode(c));return e}
const fmt=n=>n==null||n<0?'-':Number(n).toLocaleString('en-GB');
const cmp=n=>{if(n==null||n<0)return'-';if(n<10000)return fmt(n);const u=['K','M','B'];let i=-1;while(n>=1000&&i<2){n/=1000;i++}return(n>=100?n.toFixed(0):n>=10?n.toFixed(1):n.toFixed(2))+u[i]};
const sg=n=>(n>=0?'+':'')+fmt(n);
const cls=n=>n>0?'pos':n<0?'neg':'';
function ago(t){if(!t)return'';let d=Date.now()/1000-t;const p=(n,w)=>n+' '+w+(n==1?'':'s')+' ago';if(d<3600)return Math.max(1,d/60|0)+' min ago';if(d<86400)return(d/3600|0)+' h ago';if(d<2592000)return p(d/86400|0,'day');return p(d/2592000|0,'month')}
function dur(s){if(!s)return'';const p=x=>String(x).padStart(2,'0');return s>=3600?`${s/3600|0}:${p((s/60|0)%60)}:${p(s%60)}`:`${s/60|0}:${p(s%60)}`}
let D=null,graphs={};
function est(c){if(!D.est||!c.rate||c.rate<=0||c.step<=1||!c.stepAt)return c.subs;return c.subs+Math.min(c.step-1,Math.max(0,Math.floor(c.rate*(Date.now()/1000-c.stepAt)/86400)))}
function eta(c){const e=est(c);if(!(c.rate>0.01))return c.rate<0?'Losing subscribers':'Need more data for a date';const d=(c.next-e)/c.rate;if(d<1)return'Expected today';if(d>3650)return'10+ years away';return'Expected around '+new Date(Date.now()+d*864e5).toLocaleDateString('en-GB',{day:'numeric',month:'short',year:d>300?'numeric':undefined})}
function av(c,lg){const i=c.avatar?h('img',{class:'av'+(lg?' lg':''),src:c.avatar,alt:''}):h('div',{class:'av'+(lg?' lg':'')});const L=isLive(c);return h('span',{class:'avw'+(L?' on':'')+(lg?' lg':''),title:L?name(c)+' is live now':''},i,L?h('span',{class:'lv',text:'LIVE'}):null)}
function isLive(c){return !!(c.vid&&c.vid.live)}
function name(c){return c.title||c.handle}
async function graph(c,days,box){box.textContent='';const r=await fetch('/api/history?i='+c.i+'&days='+days);const pts=await r.json();if(pts.length<2){box.append(h('div',{class:'sub',text:'Collecting data – recorded every hour'}));return}
pts.push([Date.now()/1000|0,c.subs]);const W=600,H=120,t0=pts[0][0],t1=pts[pts.length-1][0];let mn=Math.min(...pts.map(p=>p[1])),mx=Math.max(...pts.map(p=>p[1]));if(mx==mn){mx++;mn--}
const X=t=>(t-t0)/(t1-t0||1)*(W-4)+2,Y=v=>H-6-(v-mn)/(mx-mn)*(H-24);const d=pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg');svg.setAttribute('viewBox',`0 0 ${W} ${H}`);svg.setAttribute('class','g');svg.setAttribute('preserveAspectRatio','none');
const area=document.createElementNS(ns,'path');area.setAttribute('d',d+` L ${X(t1)} ${H} L ${X(t0)} ${H} Z`);area.setAttribute('fill','rgba(48,209,88,.12)');svg.append(area);
const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#30d158');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');svg.append(ln);
box.append(svg,h('div',{class:'sub',text:`${cmp(mn)} – ${cmp(mx)}  ·  ${new Date(t0*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'})} to now`}))}
function channelCard(c){const card=h('div',{class:'card'+(c.err&&c.subs<0?' err':'')});
const link=c.tw?'https://www.twitch.tv/'+c.tw:isLive(c)?'https://youtu.be/'+c.vid.id:c.id?'https://www.youtube.com/channel/'+c.id:'#';
card.append(h('a',{class:'top',href:link,target:'_blank'},av(c,true),h('div',{style:'min-width:0'},h('div',{class:'nm'},name(c),c.tw?h('span',{class:'twb',text:'Twitch'}):null),h('div',{class:'sub',text:[c.tw?'twitch.tv/'+c.tw:c.handle,c.country,c.joined?'since '+new Date(c.joined*1000).getFullYear():''].filter(Boolean).join(' · ')}))));
if(c.err&&c.subs<0){card.append(h('p',{text:c.err}));return card}
const e=est(c);card.append(h('div',{class:'big'},h('span',{'data-est':c.i,text:fmt(e)}),(D.est&&c.step>1&&c.rate>0)?h('span',{class:'est',text:'est.'}):null));
card.append(h('div',{class:'sub',text:c.tw?(isLive(c)?'followers · live now':'followers · offline'):`${cmp(c.views)} views · ${fmt(c.videos)} videos · ${c.videos>0?cmp(Math.round(c.views/c.videos)):'-'} avg`}));
if(c.statsOk)card.append(h('div',{class:'chips'},[['Today',c.today],['Last 24 h',c.d1],['7 days',c.d7],['30 days',c.d30]].map(([k,v])=>h('div',{class:'chip'},k+' ',h('b',{class:cls(v),text:sg(v)}))),c.rate?h('div',{class:'chip'},'≈ ',h('b',{text:sg(Math.round(c.rate))}),'/day'):null));
else card.append(h('div',{class:'chips'},h('div',{class:'chip',text:'Growth: collecting data'})));
const f=Math.max(0,Math.min(1,(e-c.prev)/(c.next-c.prev||1)));
card.append(h('div',{style:'margin-top:6px;display:flex;justify-content:space-between'},h('span',{class:'sub',text:'Next milestone '}),h('b',{text:fmt(c.next)})),h('div',{class:'bar'},h('i',{style:`width:${(f*100).toFixed(1)}%`})),(c.next-e>0&&c.next-e<=Math.max(10,(c.next-c.prev)/10))?h('div',{style:'color:var(--gold);font-weight:700',text:`Almost there: ${fmt(c.next-e)} to go! · ${eta(c)}`}):h('div',{class:'sub',text:`${fmt(c.next-e)} to go · ${eta(c)}`}));
const tabs=h('div',{class:'tabs'}),gbox=h('div');let cur=graphs[c.i]||7;
for(const dd of [7,30]){const b=h('button',{text:dd+' days',class:dd==cur?'on':'',onclick:()=>{graphs[c.i]=dd;[...tabs.children].forEach(x=>x.className='');b.className='on';graph(c,dd,gbox)}});tabs.append(b)}
tabs.append(h('a',{class:'small',href:'/api/csv?i='+c.i,text:'Download CSV'}));card.append(tabs,gbox);graph(c,cur,gbox);
if(c.vt){const t=c.vt,ag=t.ageMin<60?t.ageMin+' min':(t.ageMin/60|0)+'h '+(t.ageMin%60)+'m';const box=h('div',{style:'margin-top:14px;padding:12px;border-radius:12px;background:#1f2a1f'},h('div',{class:'sub',style:'color:var(--gold)',text:'NEW VIDEO TRACKER'}),h('div',{style:'font-size:26px;font-weight:800',text:fmt(t.views)+' views'}),h('div',{class:'sub',text:`in ${ag}`+(t.rate>=0?` · ${cmp(t.rate)}/hour now`:'')}),t.cmp?h('div',{style:'margin-top:4px',class:t.cmp.includes('+')?'pos':t.cmp.includes('-')?'neg':'',text:t.cmp}):null);
if(t.pts&&t.pts.length>1){const W=600,H=70,p=t.pts,t0=p[0][0],t1=p[p.length-1][0],mn=p[0][1],mx=Math.max(p[p.length-1][1],mn+1);const d=p.map((q,k)=>(k?'L':'M')+((q[0]-t0)/(t1-t0||1)*(W-4)+2).toFixed(1)+' '+(H-4-(q[1]-mn)/(mx-mn)*(H-10)).toFixed(1)).join(' ');const ns='http://www.w3.org/2000/svg',sv=document.createElementNS(ns,'svg');sv.setAttribute('viewBox',`0 0 ${W} ${H}`);sv.setAttribute('preserveAspectRatio','none');sv.setAttribute('class','g');sv.style.height='70px';const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#ffc53d');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');sv.append(ln);box.append(sv)}
card.append(box)}
if(c.vid){const v=c.vid;card.append(h('a',{class:'vid',href:c.tw?'https://www.twitch.tv/'+c.tw:'https://youtu.be/'+v.id,target:'_blank'},h('img',{src:v.thumb||`https://i.ytimg.com/vi/${v.id}/mqdefault.jpg`,alt:''}),h('div',{style:'min-width:0'},h('div',{class:'t'},v.live?h('span',{class:'live',text:'LIVE'}):null,v.title),h('div',{class:'sub',text:v.live?`${fmt(v.viewers)} watching now`:`${ago(v.pub)} · ${dur(v.dur)}`}),c.tw?h('div',{class:'sub',text:v.game||''}):h('div',{class:'sub',text:`${cmp(v.views)} views · ${v.likes>=0?cmp(v.likes)+' likes':'likes hidden'} · ${v.comments>=0?cmp(v.comments)+' comments':'comments off'}`}))))}
if(c.pb){const b=c.pb,dd=t=>t?new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'}):'';const it=[];
if(b.day)it.push(h('span',{},'Best day ',h('b',{text:sg(b.day)}),' '+dd(b.dayT)));if(b.week)it.push(h('span',{},'Best week ',h('b',{text:sg(b.week)}),' '+dd(b.weekT)));if(b.vid)it.push(h('span',{title:b.vidTitle||''},'Best video, 1st day ',h('b',{text:cmp(b.vid)+' views'})));
if(it.length)card.append(h('div',{class:'pb'},h('span',{text:'Records:'}),it))}
if(c.rv&&c.rv.length){const box=h('details',{class:'cm'}),tb=h('table',{class:'rvt'});let sk=RVS[c.i]||'pd';
const rows=c.rv.map(r=>{const d=Math.max(1,(Date.now()/1000-r[2])/86400);return{id:r[0],t:r[1],pub:r[2],v:r[3],pd:r[3]/d,lk:r[3]>0&&r[4]>=0?r[4]/r[3]*100:-1}});
const keys=[['v','Views'],['pd','Per day'],['lk','Likes %']];
const draw=()=>{tb.textContent='';tb.append(h('tr',{class:'hd'},h('th',{text:'Video'}),keys.map(([k,t])=>h('th',{class:'n'},h('a',{href:'#',text:t+(sk==k?' ▾':''),onclick:e=>{e.preventDefault();sk=k;RVS[c.i]=k;draw()}})))));
[...rows].sort((a,b)=>b[sk]-a[sk]).forEach(r=>tb.append(h('tr',{},h('td',{class:'nm2'},h('a',{href:'https://youtu.be/'+r.id,target:'_blank',text:r.t,title:r.t})),h('td',{class:'n',text:cmp(r.v)}),h('td',{class:'n',text:cmp(Math.round(r.pd))}),h('td',{class:'n',text:r.lk>=0?r.lk.toFixed(1)+'%':'–'}))))};
draw();box.append(h('summary',{},h('h3',{style:'display:inline',text:'Recent uploads ('+rows.length+')'})),tb);card.append(box)}
if(c.vid&&(c.cm||c.cmOff)){const box=h('div',{class:'cm'},h('h3',{text:'Latest comments'}));
if(c.cmOff)box.append(h('div',{class:'sub',text:'Comments are off on this video'}));
else c.cm.forEach(m=>box.append(h('div',{class:'c'},h('b',{text:m.a}),h('span',{class:'sub',text:'  '+ago(m.ts)+(m.l>0?' · '+cmp(m.l)+' likes':'')}),h('p',{text:m.t}))));card.append(box)}
return card}
const COLS=['#3987e5','#d95926','#199e70','#c98500','#d55181','#008300','#9085e9','#e66767'];
const colFor=i=>i<COLS.length?COLS[i]:'#8d8d95';
let CMP={sel:null,days:7,mode:'gain'},HC={},RVS={};
async function hist(i,days){const k=i+':'+days;if(!HC[k]||Date.now()-HC[k].at>120000){const r=await fetch('/api/history?i='+i+'&days='+days);HC[k]={at:Date.now(),p:await r.json()}}return HC[k].p}
function compareCard(){const C=D.channels.filter(c=>c.subs>=0);
if(!CMP.sel){const o=[...C].filter(c=>c.i!=0).sort((a,b)=>(b.d7||0)-(a.d7||0));CMP.sel=[0,...o.slice(0,2).map(c=>c.i)].filter(i=>C.some(c=>c.i==i))}
const card=h('div',{class:'card wide'}),box=h('div',{class:'cmp'}),ctl=h('div',{class:'ctl'});
card.append(h('h2',{text:'Compare'}),h('div',{class:'sub',style:'margin:-8px 0 12px',text:'Subscribers gained over the period, so big and small channels fit on one chart. Pick up to 4.'}),ctl,box,h('div',{style:'display:flex;margin-top:8px'},h('a',{class:'small',href:'/api/csv',text:'Download all history (CSV)'})));
const draw=()=>{ctl.textContent='';
C.forEach(c=>{const on=CMP.sel.includes(c.i);ctl.append(h('button',{class:'tg'+(on?' on':''),onclick:()=>{if(on)CMP.sel=CMP.sel.filter(x=>x!=c.i);else if(CMP.sel.length<4)CMP.sel.push(c.i);draw()}},h('i',{style:'background:'+(on?colFor(c.i):'transparent')+';border:1px solid '+colFor(c.i)}),name(c)))});
const seg=(opts,key)=>h('div',{class:'seg'},opts.map(([v,t])=>h('button',{class:CMP[key]==v?'on':'',text:t,onclick:()=>{CMP[key]=v;draw()}})));
ctl.append(h('span',{style:'flex:1'}),seg([[7,'7 days'],[30,'30 days']],'days'),seg([['gain','Gained'],['pct','% growth']],'mode'));
plot(box,C)};draw();return card}
async function plot(box,C){const sel=CMP.sel.slice(),days=CMP.days,mode=CMP.mode;
const ser=[];for(const i of sel){const c=C.find(x=>x.i==i);if(!c)continue;const p=(await hist(i,days)).slice();p.push([Date.now()/1000|0,c.subs]);if(p.length<2)continue;const b=p[0][1];
ser.push({c,pts:p.map(([t,v])=>[t,mode=='pct'?(b>0?(v-b)/b*100:0):v-b])})}
const BW=box.clientWidth||900;box.textContent='';if(!ser.length){box.append(h('div',{class:'sub',style:'padding:40px 0;text-align:center',text:sel.length?'Not enough history yet':'Pick a channel above'}));return}
const W=Math.max(280,BW),narrow=W<600,H=260,L=narrow?46:56,R=narrow?10:150,T=12,B=28,ns='http://www.w3.org/2000/svg',mk=(t,a)=>{const e=document.createElementNS(ns,t);for(const k in a)e.setAttribute(k,a[k]);return e};
const now=Date.now()/1000,t0=now-days*86400;let mn=0,mx=0;ser.forEach(s=>s.pts.forEach(([,v])=>{mn=Math.min(mn,v);mx=Math.max(mx,v)}));if(mx==mn)mx=mn+1;
const st=(()=>{const r=(mx-mn)/4,m=Math.pow(10,Math.floor(Math.log10(r))),f=r/m;return (f<=1?1:f<=2?2:f<=2.5?2.5:f<=5?5:10)*m})();mn=Math.floor(mn/st)*st;mx=Math.ceil(mx/st)*st;const nT=Math.round((mx-mn)/st);
const X=t=>L+(Math.max(t,t0)-t0)/(now-t0)*(W-L-R),Y=v=>T+(mx-v)/(mx-mn)*(H-T-B);
const fv=v=>mode=='pct'?(v>=0?'+':'')+v.toFixed(Math.abs(mx)<1?2:1)+'%':(v>=0?'+':'')+cmp(Math.round(v));
const svg=mk('svg',{viewBox:`0 0 ${W} ${H}`});
for(let k=0;k<=nT;k++){const v=mn+st*k,y=Y(v);svg.append(mk('line',{x1:L,x2:W-R,y1:y,y2:y,stroke:'#2a2a2e','stroke-width':1}));const tx=mk('text',{x:L-8,y:y+4,'text-anchor':'end','font-size':11,fill:'#8d8d95'});tx.textContent=fv(v);svg.append(tx)}
[0,.5,1].forEach(f=>{const t=t0+(now-t0)*f,tx=mk('text',{x:L+(W-L-R)*f,y:H-8,'text-anchor':f==0?'start':f==1?'end':'middle','font-size':11,fill:'#8d8d95'});tx.textContent=f==1?'now':new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'});svg.append(tx)});
const ends=[];ser.forEach(s=>{const d=s.pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
svg.append(mk('path',{d,fill:'none',stroke:colFor(s.c.i),'stroke-width':2,'stroke-linejoin':'round','vector-effect':'non-scaling-stroke','stroke-dasharray':s.c.i<COLS.length?'':'5 4'}));
const last=s.pts[s.pts.length-1];ends.push({s,y:Y(last[1]),v:last[1]})});
ends.sort((a,b)=>a.y-b.y);for(let k=1;k<ends.length;k++)if(ends[k].y-ends[k-1].y<16)ends[k].y=ends[k-1].y+16;
ends.forEach(e=>{svg.append(mk('circle',{cx:W-R,cy:Y(e.s.pts[e.s.pts.length-1][1]),r:4,fill:colFor(e.s.c.i),stroke:'#18181b','stroke-width':2}));if(narrow)return;const g=mk('text',{x:W-R+8,y:e.y+4,'font-size':12,fill:'#f2f2f3'});g.textContent=name(e.s.c).slice(0,14)+' '+fv(e.v);svg.append(g)});
const cross=mk('line',{y1:T,y2:H-B,stroke:'#8d8d95','stroke-width':1,visibility:'hidden'});svg.append(cross);
const tip=h('div',{class:'tip'});box.append(svg,tip);
svg.onmousemove=ev=>{const r=svg.getBoundingClientRect(),sx=(ev.clientX-r.left)/r.width*W;if(sx<L||sx>W-R){svg.onmouseleave();return}
const t=t0+(sx-L)/(W-L-R)*(now-t0);cross.setAttribute('x1',sx);cross.setAttribute('x2',sx);cross.setAttribute('visibility','visible');
tip.textContent='';tip.append(h('div',{class:'sub',text:new Date(t*1000).toLocaleString('en-GB',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'})}));
ser.forEach(s=>{let v=s.pts[0][1];for(const p of s.pts){if(p[0]>t)break;v=p[1]}tip.append(h('div',{},h('i',{style:'background:'+colFor(s.c.i)}),name(s.c)+' ',h('b',{text:fv(v)})))});
tip.style.display='block';const px=ev.clientX-r.left;tip.style.left=Math.min(px+12,r.width-tip.offsetWidth-4)+'px';tip.style.top='10px'};
svg.onmouseleave=()=>{cross.setAttribute('visibility','hidden');tip.style.display='none'}}
function render(){const app=$('#app');app.textContent='';const C=D.channels;if(D.accent)document.documentElement.style.setProperty('--gold',D.accent);
$('#meta').textContent=(D.err?D.err:(D.updatedAgo>=0?'Updated '+(D.updatedAgo<60?'just now':(D.updatedAgo/60|0)+' min ago'):''))+(D.ver?'  ·  v'+D.ver:'');{const u=$('#upd');if(D.upd){u.textContent='Update v'+D.upd;u.style.display=''}else u.style.display='none'}
const row=h('aside',{class:'side'});
// summary
const ok=C.filter(c=>c.statsOk).sort((a,b)=>b.d1-a.d1);const nv=C.filter(c=>c.vid&&!c.tw&&Date.now()/1000-c.vid.pub<86400);
row.append(h('div',{class:'card'},h('h2',{text:'Last 24 hours'}),h('div',{class:'sub',style:'margin:-8px 0 8px',text:'Rolling: gains since this time yesterday'}),h('div',{class:'list'},ok.length?ok.slice(0,5).map(c=>h('div',{},h('span',{text:name(c)}),h('b',{class:cls(c.d1),text:sg(c.d1)}))):h('div',{class:'sub',text:'Collecting data – check back in a few hours'})),h('div',{class:'sub',style:'margin-top:8px',text:nv.length?`${nv.length} new video${nv.length>1?'s':''} today`:'No new videos in the last day'})));
// weather
if(D.wx){const w=D.wx;row.append(h('div',{class:'card'},h('h2',{text:'Weather · '+w.place}),h('div',{class:'big',text:w.temp+'°'}),h('div',{text:w.text+' · feels '+w.feels+'°'}),h('div',{class:'sub',text:`High ${w.hi}° · Low ${w.lo}° · Wind ${w.wind} mph`}),w.rainHour>=0?h('div',{style:'margin-top:8px;color:var(--blue)',text:`Rain likely around ${String(w.rainHour).padStart(2,'0')}:00 (${w.rainPct}%)`}):null))}
// race
if(D.race){const A=C[D.race[0]],B=C[D.race[1]],ea=est(A),eb=est(B),fa=ea+eb?ea/(ea+eb):.5;const lead=ea>=eb?A:B,ch=lead===A?B:A,closing=(ch.rate||0)-(lead.rate||0),gap=Math.abs(ea-eb);
row.append(h('div',{class:'card race'},h('h2',{text:'Race'}),h('div',{class:'rs'},h('span',{},av(A),h('b',{style:'color:var(--red)',text:name(A)})),h('b',{text:fmt(ea)})),h('div',{class:'rs'},h('span',{},av(B),h('b',{style:'color:var(--blue)',text:name(B)})),h('b',{text:fmt(eb)})),h('div',{class:'tug'},h('div',{class:'a',style:`width:${fa*100}%`}),h('div',{class:'b',style:`width:${(1-fa)*100}%`})),h('div',{text:`Gap ${fmt(gap)}`}),h('div',{class:'sub',text:closing>0.01?`${name(ch)} is catching up by ${fmt(Math.round(closing))}/day – could pass in about ${Math.max(1,Math.round(gap/closing))} days`:(lead.rate||ch.rate)?`${name(lead)} is pulling away`:'Trend: need more data'})))}
// leaderboard
const lb=[...C].filter(c=>c.subs>=0).sort((a,b)=>b.subs-a.subs);
row.append(h('div',{class:'card'},h('h2',{text:'Leaderboard'}),h('table',{},h('tr',{class:'hd'},h('th',{colspan:'3'}),h('th',{class:'n',text:C.some(c=>c.tw)?'Total':'Subs'}),h('th',{class:'n',text:'Today',title:'Since midnight'})),lb.map((c,k)=>h('tr',{class:c.i==0?'me':''},h('td',{text:k+1+'.'}),h('td',{},av(c)),h('td',{class:'nm2',text:name(c)}),h('td',{class:'n',text:cmp(c.subs)}),h('td',{class:'n '+cls(c.today),text:c.statsOk?sg(c.today):''}))))));
{const mon=t=>new Date(t*1000).toLocaleDateString('en-GB',{month:'short'});const ms=[...C].filter(c=>c.subs>=0).sort((a,b)=>(b.mo||0)-(a.mo||0));
if(ms.length&&C.some(c=>c.statsOk))row.append(h('div',{class:'card'},h('h2',{text:'Monthly'}),h('table',{},h('tr',{class:'hd'},h('th',{}),h('th',{class:'n',text:mon(D.m1)}),h('th',{class:'n',text:mon(D.m0)+' so far'})),ms.map(c=>h('tr',{class:c.i==0?'me':''},h('td',{class:'nm2',text:name(c)}),h('td',{class:'n '+(c.lm!=null?cls(c.lm):''),text:c.lm!=null?sg(c.lm):'–'}),h('td',{class:'n '+cls(c.mo),text:c.statsOk?sg(c.mo):'–'}))))))}
const grid=h('section',{class:'grid'});C.forEach(c=>grid.append(channelCard(c)));if(C.filter(c=>c.subs>=0).length>1)grid.append(compareCard());app.append(h('div',{class:'layout'},row,grid))}
async function load(){try{const r=await fetch('/api/data');D=await r.json();render()}catch(e){$('#meta').textContent='Board not reachable'}}
setInterval(()=>{if(!D)return;document.querySelectorAll('[data-est]').forEach(el=>{const c=D.channels[el.dataset.est];el.textContent=fmt(est(c))})},1000);
load();setInterval(load,60000);
</script></body></html>)HTML";

void handleDashboard() {
  if (portalMode) { handleSettings(); return; }
  server.send_P(200, "text/html", DASH_HTML);
}

void handleApiData() {
  JsonDocument doc;
  doc["now"] = (long)nowT();
  doc["updatedAgo"] = lastFetchOk ? (long)((millis() - lastFetchOk) / 1000) : -1;
  doc["err"] = netError;
  doc["est"] = cfgEst;
  doc["ip"] = WiFi.localIP().toString();
  doc["ver"] = FW_VERSION;
  doc["accent"] = THEMES[cfgTheme].css;
  if (updAvail) doc["upd"] = updVer;
  doc["m0"] = (long)monthStart(0); doc["m1"] = (long)monthStart(1);
  if (raceSet()) { JsonArray r = doc["race"].to<JsonArray>(); r.add(cfgRaceA); r.add(cfgRaceB); }
  else doc["race"] = nullptr;
  if (wx.ok) {
    JsonObject w = doc["wx"].to<JsonObject>();
    w["place"] = cfgWxName; w["temp"] = lroundf(wx.temp); w["feels"] = lroundf(wx.feels); w["text"] = wxText(wx.code);
    w["hi"] = wx.hi; w["lo"] = wx.lo; w["rainHour"] = wx.rainHour; w["rainPct"] = wx.rainPct; w["wind"] = lroundf(wx.wind);
  }
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    JsonObject o = arr.add<JsonObject>();
    o["i"] = i; o["handle"] = c.handle; o["id"] = c.id; o["title"] = c.title;
    o["subs"] = c.subs; o["step"] = (c.subs >= 0 && !c.tw) ? stepFor(c.subs) : 1;
    o["rate"] = c.ratePerDay; o["stepAt"] = (long)c.stepChangedAt;
    o["views"] = c.views; o["videos"] = c.videos; o["joined"] = (long)c.joined;
    o["country"] = c.country; o["avatar"] = c.avatarUrl; o["err"] = c.err;
    o["statsOk"] = c.statsOk; o["today"] = c.gainToday; o["d1"] = c.gain24;
    o["d7"] = c.gain7; o["d30"] = c.gain30; o["histStart"] = (long)c.histStart;
    long e = c.subs >= 0 ? estimateFor(i) : 0;
    long nx = nextMilestone(max(0L, e));
    o["next"] = nx; o["prev"] = prevMilestone(nx);
    if (i == 0 && vt.active && vt.n) {
      JsonObject t = o["vt"].to<JsonObject>();
      t["views"] = vtViews(); t["ageMin"] = vtAgeMin(); t["rate"] = vtRate(); t["typical"] = vt.typical;
      t["at1h"] = vt.at1h; t["base1h"] = vt.base1h; t["at24h"] = vt.at24h; t["base24h"] = vt.base24h; t["cmp"] = vtCompare();
      JsonArray pts = t["pts"].to<JsonArray>();
      for (int k = 0; k < vt.n; k++) { JsonArray pp = pts.add<JsonArray>(); pp.add(vt.t[k]); pp.add(vt.v[k]); }
    }
    if (c.tw) {
      o["tw"] = c.twLogin;
      if (c.live) { JsonObject v = o["vid"].to<JsonObject>(); v["id"] = ""; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished;
                    v["live"] = true; v["viewers"] = c.liveViewers; v["thumb"] = c.twThumb; v["game"] = c.twGame; }
    }
    o["mo"] = c.gainMonth;
    if (c.lastMonthOk) o["lm"] = c.gainLastMonth;
    if (!c.pbLoaded) pbLoad(i);
    if (c.pbDay || c.pbWeek || c.pbVid) {
      JsonObject b = o["pb"].to<JsonObject>();
      b["day"] = c.pbDay; b["dayT"] = c.pbDayT; b["week"] = c.pbWeek; b["weekT"] = c.pbWeekT;
      if (c.pbVid) { b["vid"] = c.pbVid; b["vidT"] = c.pbVidT; b["vidTitle"] = c.pbVidTitle; }
    }
    if (c.cmOff) o["cmOff"] = true;
    if (c.rvN) {
      JsonArray rv = o["rv"].to<JsonArray>();
      for (int k = 0; k < c.rvN; k++) {
        JsonArray x = rv.add<JsonArray>();
        x.add(c.rv[k].id); x.add(c.rv[k].title); x.add((long)c.rv[k].pub); x.add(c.rv[k].views); x.add(c.rv[k].likes); x.add(c.rv[k].comments);
      }
    }
    if (c.cmN) {
      JsonArray cm = o["cm"].to<JsonArray>();
      for (int k = 0; k < c.cmN; k++) {
        JsonObject x = cm.add<JsonObject>();
        x["a"] = c.cmAuthor[k]; x["t"] = c.cmText[k]; x["ts"] = (long)c.cmT[k]; x["l"] = c.cmLikes[k];
      }
    }
    if (!c.tw && c.vidId.length() && c.vidTitle.length()) {
      JsonObject v = o["vid"].to<JsonObject>();
      v["id"] = c.vidId; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished; v["dur"] = c.vidDuration;
      v["views"] = c.vidViews; v["likes"] = c.vidLikes; v["comments"] = c.vidComments;
      v["live"] = c.live; v["viewers"] = c.liveViewers;
    }
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// [[time, subs], ...] for one channel, streamed in chunks
void handleApiHistory() {
  int i = server.arg("i").toInt();
  int days = constrain(server.arg("days").toInt(), 1, 31);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  if (i >= 0 && i < numCh && fsOk && ch[i].id.length()) {
    File f = LittleFS.open(histPath(i), "r");
    if (f) {
      time_t from = nowT() - (time_t)days * 86400;
      String chunk; bool first = true; Sample s;
      while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
        if ((time_t)s.t < from) continue;
        chunk += (first ? "[" : ",[") + String(s.t) + "," + String(s.s) + "]";
        first = false;
        if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
      }
      f.close();
      if (chunk.length()) server.sendContent(chunk);
    }
  }
  server.sendContent("]");
  server.sendContent("");
}

// History as CSV: /api/csv?i=2 for one channel, /api/csv for all of them
void handleApiCsv() {
  int only = server.hasArg("i") ? server.arg("i").toInt() : -1;
  String fname = only >= 0 && only < numCh ? nameOf(only) : String("all-channels");
  String safe; for (char ch2 : fname) safe += isalnum((unsigned char)ch2) ? ch2 : '-';
  server.sendHeader("Content-Disposition", "attachment; filename=\"subcounter-" + safe + ".csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent("channel,time_utc,subscribers\n");
  for (int i = 0; i < numCh; i++) {
    if (only >= 0 && i != only) continue;
    if (!fsOk || !ch[i].id.length()) continue;
    File f = LittleFS.open(histPath(i), "r");
    if (!f) continue;
    String nm = nameOf(i); nm.replace("\"", "'");
    String chunk; Sample sm;
    while (f.read((uint8_t *)&sm, sizeof(sm)) == sizeof(sm)) {
      char ts[24]; time_t t = sm.t; struct tm g; gmtime_r(&t, &g);
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &g);
      chunk += "\"" + nm + "\"," + ts + "," + String(sm.s) + "\n";
      if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
    }
    f.close();
    if (chunk.length()) server.sendContent(chunk);
  }
  server.sendContent("");
}

// ── Settings backup / restore ───────────────────────────────────────────────
// Every saved setting (NVS namespace "subcounter") as JSON. Passwords, keys and
// sign-ins are left out unless asked for. History and records aren't included.
bool secretKey(const char *k) {
  String s = k;
  return s == "apikey" || s == "pin" || s == "spsec" || s == "spref" || s == "twsec" || s.endsWith("pass");
}
void handleNtfyTest() {
  if (!authed()) return;
  String t = server.arg("ntfy"); t.trim(); t.replace(" ", "-");
  if (!t.length()) { sendMessage(400, "No topic", "Type an ntfy topic first."); return; }
  cfgNtfy = t;                          // use what's typed (saved properly when you press Save)
  queueNote("SubCounter test", "Notifications are working! v" FW_VERSION, "white_check_mark", "http://" + WiFi.localIP().toString() + "/");
  sendMessage(200, "Test sent", "Check your phone in a few seconds. If nothing arrives, make sure the ntfy app is subscribed to <b>" + htmlEscape(t) + "</b>. Don't forget to press Save.");
}
void handleBackup() {
  if (!authed()) return;
  bool secrets = server.arg("secrets") == "1";
  JsonDocument doc;
  doc["subcounter_backup"] = 1; doc["firmware"] = FW_VERSION; doc["with_secrets"] = secrets;
  JsonObject keys = doc["keys"].to<JsonObject>();
  prefs.begin("subcounter", true);
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find("nvs", "subcounter", NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info_t info; nvs_entry_info(it, &info);
    if (secrets || !secretKey(info.key)) {
      JsonObject o = keys[info.key].to<JsonObject>();
      switch (info.type) {
        case NVS_TYPE_U8:  o["t"] = "u8";  o["v"] = prefs.getUChar(info.key); break;
        case NVS_TYPE_I8:  o["t"] = "i8";  o["v"] = prefs.getChar(info.key); break;
        case NVS_TYPE_U16: o["t"] = "u16"; o["v"] = prefs.getUShort(info.key); break;
        case NVS_TYPE_I16: o["t"] = "i16"; o["v"] = prefs.getShort(info.key); break;
        case NVS_TYPE_U32: o["t"] = "u32"; o["v"] = prefs.getUInt(info.key); break;
        case NVS_TYPE_I32: o["t"] = "i32"; o["v"] = prefs.getInt(info.key); break;
        case NVS_TYPE_STR: o["t"] = "str"; o["v"] = prefs.getString(info.key); break;
        case NVS_TYPE_BLOB: {
          size_t n = prefs.getBytesLength(info.key); uint8_t b[64];
          if (n <= sizeof(b)) { prefs.getBytes(info.key, b, n); String hx; char t[3];
            for (size_t k = 0; k < n; k++) { snprintf(t, 3, "%02x", b[k]); hx += t; }
            o["t"] = "blob"; o["v"] = hx; }
          break; }
        default: keys.remove(info.key); break;
      }
    }
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  prefs.end();
  String out; serializeJsonPretty(doc, out);
  server.sendHeader("Content-Disposition", String("attachment; filename=\"subcounter-settings") + (secrets ? "-with-keys" : "") + ".json\"");
  server.send(200, "application/json", out);
}
void handleRestore() {
  if (!authed()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !(doc["subcounter_backup"] | 0)) {
    server.send(400, "text/plain", "That isn't a SubCounter settings file."); return;
  }
  int n = 0;
  prefs.begin("subcounter", false);
  for (JsonPair kv : doc["keys"].as<JsonObject>()) {
    const char *k = kv.key().c_str(); String t = kv.value()["t"] | "";
    JsonVariant v = kv.value()["v"];
    if (strlen(k) > 15) continue;
    if (t == "u8") prefs.putUChar(k, v.as<uint8_t>());
    else if (t == "i8") prefs.putChar(k, v.as<int8_t>());
    else if (t == "u16") prefs.putUShort(k, v.as<uint16_t>());
    else if (t == "i16") prefs.putShort(k, v.as<int16_t>());
    else if (t == "u32") prefs.putUInt(k, v.as<uint32_t>());
    else if (t == "i32") prefs.putInt(k, v.as<int32_t>());
    else if (t == "str") prefs.putString(k, v.as<String>());
    else if (t == "blob") { String hx = v.as<String>(); uint8_t b[64]; size_t m = min((size_t)64, hx.length() / 2);
      for (size_t i = 0; i < m; i++) b[i] = strtoul(hx.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
      prefs.putBytes(k, b, m); }
    else continue;
    n++;
  }
  prefs.end();
  server.send(200, "text/plain", "Restored " + String(n) + " settings. Restarting...");
  delay(800);
  ESP.restart();
}

const char SCAN_JS[] PROGMEM = R"JS(<style>
.net{display:flex;align-items:center;gap:10px;width:100%;margin:0;padding:11px 12px;border:1px solid #333;border-radius:10px;background:#111;color:#fff;font-size:15px;font-weight:400;text-align:left;cursor:pointer;margin-top:6px}
.net:hover{border-color:#e62117}.net b{flex:1;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.net small{margin:0;color:#888}.bars{display:inline-flex;gap:2px;align-items:flex-end;height:14px}.bars i{width:4px;background:#555;border-radius:1px}.bars i.on{background:#30d158}
</style><script>
async function scan(){
  const box=document.getElementById('nets'),btn=document.getElementById('scanBtn');
  btn.disabled=true;btn.textContent='Scanning… (a few seconds)';box.textContent='';
  try{
    const r=await fetch('/api/scan');const list=await r.json();
    if(!list.length){box.innerHTML='<small>No networks found. Is the hotspot on? (iPhone: keep the Personal Hotspot screen open and turn on Maximise Compatibility)</small>'}
    list.forEach(n=>{
      const b=document.createElement('button');b.type='button';b.className='net';
      const bars=document.createElement('span');bars.className='bars';
      const lvl=n.rssi>-55?4:n.rssi>-67?3:n.rssi>-78?2:1;
      for(let k=1;k<=4;k++){const i=document.createElement('i');i.style.height=(k*3+2)+'px';if(k<=lvl)i.className='on';bars.append(i)}
      const nm=document.createElement('b');nm.textContent=n.ssid;
      const info=document.createElement('small');
      info.textContent=(n.saved?'saved · ':'')+(n.ent?'needs username':(n.open?'open':'🔒'));
      b.append(bars,nm,info);
      b.onclick=()=>{document.getElementById('s').value=n.ssid;document.querySelectorAll('.net').forEach(x=>x.style.borderColor='');b.style.borderColor='#30d158';
        const pw=document.getElementById('pw');pw.value='';pw.focus();pw.scrollIntoView({block:'center',behavior:'smooth'})};
      box.append(b)});
  }catch(e){box.innerHTML='<small>Scan failed – try again.</small>'}
  btn.disabled=false;btn.textContent='\u{1F4F6} Scan again';
}
</script>)JS";

// Nearby networks for the settings page: [{ssid, rssi, open, ent, saved}, ...] strongest first
void handleApiScan() {
  if (!authed()) return;
  int n = WiFi.scanNetworks();
  struct Found { String ssid; int rssi; wifi_auth_mode_t auth; } f[25]; int cnt = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;                                  // hidden networks
    int dup = -1;
    for (int k = 0; k < cnt; k++) if (f[k].ssid == s) dup = k;
    if (dup >= 0) { if (WiFi.RSSI(i) > f[dup].rssi) f[dup].rssi = WiFi.RSSI(i); continue; }
    if (cnt < 25) { f[cnt].ssid = s; f[cnt].rssi = WiFi.RSSI(i); f[cnt].auth = WiFi.encryptionType(i); cnt++; }
  }
  WiFi.scanDelete();
  for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++)      // strongest first
    if (f[b].rssi > f[a].rssi) { Found t = f[a]; f[a] = f[b]; f[b] = t; }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int k = 0; k < cnt; k++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = f[k].ssid;
    o["rssi"] = f[k].rssi;
    o["open"] = (f[k].auth == WIFI_AUTH_OPEN);
    o["ent"] = (f[k].auth == WIFI_AUTH_WPA2_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA3_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA2_WPA3_ENTERPRISE);
    bool saved = false;
    for (int j = 0; j < numNets; j++) if (nets[j].ssid == f[k].ssid) saved = true;
    o["saved"] = saved;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

String checkbox(const char *name, bool on, const char *label) {
  return String("<label><input type='checkbox' name='") + name + "' value='1' style='width:auto'" + (on ? " checked" : "") + "> " + label + "</label>";
}

String hourSelect(const char *name, int val) {
  String s = String("<select name='") + name + "'><option value='-1'" + (val < 0 ? " selected" : "") + ">Off</option>";
  for (int h = 0; h < 24; h++) {
    char b[8]; snprintf(b, sizeof(b), "%02d:00", h);
    s += "<option value='" + String(h) + "'" + (val == h ? " selected" : "") + ">" + b + "</option>";
  }
  return s + "</select>";
}

String channelSelect(const char *name, int val) {
  String s = String("<select name='") + name + "'><option value='-1'>None</option>";
  for (int i = 0; i < numCh; i++)
    s += "<option value='" + String(i) + "'" + (val == i ? " selected" : "") + ">" + htmlEscape(ch[i].title.length() ? ch[i].title : ch[i].handle) + "</option>";
  return s + "</select>";
}

// Section heading with an anchor for the jump links at the top
String sec(const char *id, const char *title) {
  return String("<h2 id='") + id + "'>" + title + "</h2>";
}

void handleSettings() {
  if (!authed()) return;
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter settings</h1><p>Saved on the board only. v" FW_VERSION;
  if (!portalMode) h += " &middot; <a href='/'>&larr; Dashboard</a>";
  h += "</p><nav class='jump'><a href='#channels'>YouTube</a><a href='#twitch'>Twitch</a><a href='#display'>Display</a><a href='#alerts'>Alerts</a>"
       "<a href='#phone'>Phone</a><a href='#weather'>Weather</a><a href='#spotify'>Spotify</a><a href='#motion'>Motion</a><a href='#wifi'>Wi-Fi</a>"
       "<a href='#updates'>Updates</a><a href='#security'>Security</a>" + String(portalMode ? "" : "<a href='#firmware'>Firmware</a>") + "</nav>";
  if (wifiFailReason.length()) {
    h += "<div style='background:#3a1210;border:1px solid #e62117;border-radius:10px;padding:12px;margin-bottom:8px;font-size:14px'>"
         "<b>Couldn't connect to Wi-Fi:</b><br>" + htmlEscape(wifiFailReason) + "</div>";
  }
  // Everything is inside ONE form so a single Save sends it all. Extra actions
  // (Connect now, Spotify, calibrate) are buttons with their own formaction.
  h += "<form method='POST' action='/save'>";

  // ── YouTube ──
  h += sec("channels", "YouTube");
  h += "<label>YouTube channels (one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@yourchannel&#10;@mkbhd&#10;@veritasium'>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>@handles, UC… channel IDs or channel links. Put your own channel first – it's highlighted and shown on the night clock.</small>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' autocapitalize='off' autocomplete='off' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'><small>Only needed if you follow YouTube channels.</small>";

  // ── Twitch ──
  h += sec("twitch", "Twitch");
  h += "<label>Twitch channels (just the name, one per line)</label>";
  h += "<textarea name='twch' autocapitalize='off' autocorrect='off' spellcheck='false' style='min-height:90px' placeholder='shroud&#10;pokimane'>" +
       htmlEscape(cfgTwitch) + "</textarea>";
  h += "<small>Type the name as it appears in their Twitch address (twitch.tv/<b>name</b>). Pasting the full link works too. "
       "YouTube and Twitch together: up to 10 channels.</small>";
  if (twError.length() && !portalMode) h += "<small style='color:#e62117'>" + htmlEscape(twError) + "</small>";
  h += "<details" + String(cfgTwId.length() && cfgTwSecret.length() ? "" : " open") + "><summary>Twitch app details" +
       String(cfgTwId.length() && cfgTwSecret.length() ? " (saved &#10003;)" : " (needed for Twitch channels)") + "</summary>";
  h += "<small>Create a free app at <a href='https://dev.twitch.tv/console/apps' target='_blank'>dev.twitch.tv/console</a>: "
       "Register Your Application &rarr; any unique name &rarr; OAuth Redirect URL <code>https://localhost</code> (any https address works, it isn't used) "
       "&rarr; Category: Other &rarr; Client type: Confidential &rarr; Create &rarr; Manage &rarr; copy the Client ID and a <b>New Secret</b>. "
       "Your Twitch account needs two-factor sign-in turned on.</small>";
  h += "<label>Client ID</label><input name='twid' autocapitalize='off' autocomplete='off' value='" + htmlEscape(cfgTwId) + "'>";
  h += "<label>Client Secret</label><input name='twsec' type='text' class='secret' autocapitalize='off' autocomplete='off' spellcheck='false' placeholder='";
  h += cfgTwSecret.length() ? "(saved — leave blank to keep)" : "";
  h += "'>";
  h += "</details>";

  // ── Display ──
  h += sec("display", "Display");
  h += "<label>Colour theme</label><select name='theme'>";
  for (int t = 0; t < NUM_THEMES; t++) h += "<option value='" + String(t) + "'" + (cfgTheme == t ? " selected" : "") + ">" + THEMES[t].name + "</option>";
  h += "</select><small>Accent colour on the board and the dashboard.</small>";
  h += "<label>Screen brightness: <b id='bv'>" + String(cfgBright) + "%</b></label>"
       "<input type='range' name='bright' min='10' max='100' step='5' value='" + String(cfgBright) + "' oninput=\"bv.textContent=this.value+'%'\" style='padding:0'>";
  h += "<label>Night clock brightness: <b id='nv'>" + String(cfgNightBright) + "%</b></label>"
       "<input type='range' name='nbright' min='1' max='40' value='" + String(cfgNightBright) + "' oninput=\"nv.textContent=this.value+'%'\" style='padding:0'>";
  h += checkbox("auto", cfgAuto, "Switch channels automatically every 10 seconds");
  h += checkbox("est", cfgEst, "Estimated live counts between YouTube's rounded steps (“est.”)");
  h += "<label>Show the big clock after this long without use</label><select name='idleclk'>";
  { const int opts[] = { 0, 1, 2, 3, 5, 10 };
    for (int o : opts) h += "<option value='" + String(o) + "'" + (cfgIdleClock == o ? " selected" : "") + ">" +
                            (o == 0 ? String("Never") : String(o) + (o == 1 ? " minute" : " minutes")) + "</option>"; }
  h += "</select><small>Touch, press BOOT or pick the board up to go back.</small>";
  h += "<label>Night clock (dims and shows the time)</label><div class='row'>" +
       hourSelect("nightStart", cfgNightStart) + "<span>to</span>" + hourSelect("nightEnd", cfgNightEnd) + "</div>";
  if (numCh >= 2) {
    h += "<label>Subscriber race</label><div class='row'>" + channelSelect("raceA", cfgRaceA) +
         "<span>vs</span>" + channelSelect("raceB", cfgRaceB) + "</div>";
    h += "<small>Swipe down twice from the main count to see it.</small>";
  }

  // ── Alerts ──
  h += sec("alerts", "Alerts");
  h += checkbox("celebrate", cfgCelebrate, "Confetti for milestones (bigger milestones, bigger party)");
  h += checkbox("livealert", cfgLiveAlert, "Alert when a channel goes live (and switch to it)");
  h += checkbox("summary", cfgSummary, "Daily summary at 9 am (a monthly recap on the 1st)");
  h += "<small>New-subscriber, overtake and record alerts are always on. Alerts are skipped at night, "
       "when the board is face-down, and while Spotify is playing.</small>";

  // ── Phone notifications ──
  h += sec("phone", "Phone notifications");
  h += "<small>Get alerts on your phone with the free <b>ntfy</b> app (iPhone and Android, no account needed). "
       "Install it, tap <b>+</b>, subscribe to the topic below, then save.</small>";
  { String sug = "subcounter-"; for (int k = 0; k < 6; k++) sug += (char)('a' + esp_random() % 26);
    h += "<label>ntfy topic (blank = off)</label><input name='ntfy' autocapitalize='off' autocomplete='off' spellcheck='false' value='" +
         htmlEscape(cfgNtfy) + "' placeholder='e.g. " + sug + "'>";
    h += "<small>Anyone who knows the topic name can read it, so make it hard to guess.</small>"; }
  const char *nk[] = { "Milestones", "Channel goes live", "New records (your channel)", "Overtakes in the race", "Your new video's view milestones", "Every new subscriber (your channel)" };
  for (int k = 0; k < 6; k++) {
    String nmk = "nt" + String(k);
    h += "<label><input type='checkbox' name='" + nmk + "' value='1' style='width:auto'" + String((cfgNtfyMask >> k) & 1 ? " checked" : "") + "> " + nk[k] + "</label>";
  }
  h += "<details><summary>Own ntfy server (advanced)</summary><label>Server address</label><input name='ntfysrv' autocapitalize='off' value='" + htmlEscape(cfgNtfyServer) + "'></details>";
  if (!portalMode) h += "<button type='submit' class='b2' formaction='/ntfy/test' formnovalidate>Send a test notification</button>";

  // ── Weather ──
  h += sec("weather", "Weather");
  h += "<label>Town or city</label><input name='wxtown' value='" + htmlEscape(cfgWxName) + "' placeholder='e.g. Glasgow'>";
  h += "<small>Long-press the board's screen and pick Weather. Forecasts from Open-Meteo.</small>";

  // ── Spotify ──
  h += sec("spotify", "Spotify");
  if (cfgSpRefresh.length()) h += "<p class='ok'>Connected &#10003;</p>";
  h += "<details" + String(cfgSpId.length() ? "" : " open") + "><summary>Spotify app details (one-time setup, needs Premium)</summary>";
  h += "<small>Put the relay page on GitHub Pages (or any https address you own), then at "
       "<b>developer.spotify.com/dashboard</b> &rarr; Create app &rarr; add that https address as the Redirect URI &rarr; tick <b>Web API</b> &rarr; Save. "
       "Copy the Client ID, Client secret and the same Redirect URI here, and save.</small>";
  h += "<label>Redirect URI (exactly as entered at Spotify)</label><input name='spredir' value='" + htmlEscape(cfgSpRedirect) +
       "' autocapitalize='off' placeholder='https://yourname.github.io/spotify-callback/'>";
  h += "<label>Client ID</label><input name='spid' value='" + htmlEscape(cfgSpId) + "' autocapitalize='off' autocomplete='off'>";
  h += "<label>Client secret</label><input name='spsecret' type='text' class='secret' autocapitalize='off' autocomplete='off' spellcheck='false' placeholder='" +
       String(cfgSpSecret.length() ? "(saved — leave blank to keep)" : "") + "'>";
  h += "</details>";
  if (!portalMode && cfgSpId.length() && cfgSpSecret.length() && cfgSpRedirect.length()) {
    if (!cfgSpRefresh.length()) {
      h += "<a class='btnlink' href='" + htmlEscape(spAuthUrl()) + "'>Connect Spotify</a>"
           "<small>Press Agree on Spotify's page and you'll be brought straight back here.</small>"
           "<details><summary>The relay page showed a code instead?</summary>"
           "<label>Paste the code or that page's full address</label><input name='url' placeholder='code or full address' autocapitalize='off'>"
           "<button type='submit' class='b2' formaction='/spotify/code' formnovalidate>Use this code</button></details>";
    } else {
      h += "<button type='submit' class='b2' formaction='/spotify/disconnect' formnovalidate>Disconnect Spotify</button>";
    }
  } else if (!portalMode) {
    h += "<small>Save the app details first; a <b>Connect Spotify</b> button then appears here.</small>";
  }

  // ── Motion ──
  h += sec("motion", "Motion");
  if (!imuOk && !portalMode) h += "<small style='color:#e62117'>Motion sensor not detected.</small>";
  h += checkbox("shake", cfgShake, "Shake to refresh");
  h += checkbox("facedown", cfgFaceDown, "Face-down turns the screen off");
  h += checkbox("portrait", cfgPortrait, "Stand it on its side for a tall leaderboard");
  h += checkbox("flip", cfgFlipPortrait, "Tall leaderboard is upside down? Tick to flip it");
  h += checkbox("tap", cfgTap, "Double-tap the desk for the next channel");
  h += "<label>Desk tap sensitivity</label><select name='tapsens'>";
  const char *sens[] = { "", "Low (firm knocks)", "Medium", "High (light taps)" };
  for (int k = 1; k <= 3; k++) h += "<option value='" + String(k) + "'" + (cfgTapSens == k ? " selected" : "") + ">" + sens[k] + "</option>";
  h += "</select>";
  if (!portalMode && imuOk) {
    h += "<label>Calibration</label><small>Put the board in the position you normally use it (on its stand or flat), then press:</small>"
         "<button type='submit' class='b2' formaction='/calibrate' formnovalidate>Set this as the normal position</button>";
  }

  // ── Wi-Fi: saved networks + add/edit one ──
  int edit = server.hasArg("edit") ? server.arg("edit").toInt() : -1;
  if (edit >= numNets) edit = -1;
  Net blank; Net &e = edit >= 0 ? nets[edit] : blank;
  h += sec("wifi", "Wi-Fi networks");
  if (numNets) {
    h += "<small>The board joins your <b>Preferred</b> network when it's in range, otherwise the strongest saved one. "
         "<b>Connect now</b> switches straight away.</small><div class='st' style='margin:10px 0'>";
    for (int k = 0; k < numNets; k++) {
      h += "<div class='saved'><b>" + htmlEscape(nets[k].ssid) + "</b>";
      if (k == curNet && !portalMode) h += "<span style='color:#30d158'>connected</span>";
      else if (!portalMode) h += "<button type='submit' formaction='/wifi/connect?n=" + String(k) + "' formnovalidate class='mini'>Connect now</button>";
      if (nets[k].ip.length()) h += "<span>fixed IP</span>";
      h += "<label style='margin:0'><input type='radio' name='pref' value='" + String(k) + "' style='width:auto'" +
           String(cfgPreferred == k ? " checked" : "") + "> Preferred</label>";
      h += "<a href='/settings?edit=" + String(k) + "#wifi'>Edit</a>";
      h += "<label style='margin:0'><input type='checkbox' name='rm" + String(k) + "' value='1' style='width:auto'> Remove</label></div>";
    }
    h += "<label style='margin:4px 0 0'><input type='radio' name='pref' value='-1' style='width:auto'" +
         String(cfgPreferred < 0 ? " checked" : "") + "> No preference (strongest signal wins)</label>";
    h += "</div>";
  }
  h += "<label>" + String(edit >= 0 ? "Editing: " + htmlEscape(e.ssid) : (numNets ? String("Add another network (e.g. home)") : String("Wi-Fi network"))) + "</label>";
  h += "<button type='button' id='scanBtn' onclick='scan()' class='b2' style='margin-top:6px'>&#128246; Scan for networks</button>";
  h += "<div id='nets' style='margin:8px 0'></div>";
  h += "<input id='s' name='ssid' value='" + htmlEscape(e.ssid) + "' placeholder='Network name (tap one above, or type it)'" + String(numNets ? "" : " required") + ">";
  if (numNets && edit < 0) h += "<small>Leave blank if you're not adding a network.</small>";
  h += "<label>Wi-Fi password</label>";
  h += "<input id='pw' name='pass' type='password' autocomplete='off' autocapitalize='off' placeholder='";
  h += e.pass.length() ? "(saved — leave blank to keep)" : "Password";
  h += "'><small><label style='display:inline;margin:0'><input type='checkbox' style='width:auto' "
       "onclick=\"document.getElementById('pw').type=this.checked?'text':'password'\"> Show password</label></small>";
  h += "<label>Username (only for work Wi-Fi that asks for one)</label>";
  h += "<input name='user' value='" + htmlEscape(e.user) + "' autocapitalize='off' placeholder='Leave blank for normal Wi-Fi'>";
  h += "<details" + String(e.ip.length() || e.compat ? " open" : "") + "><summary>Advanced settings for this network</summary>";
  h += checkbox("compat", e.compat, "Compatibility mode (Wi-Fi 4 instead of Wi-Fi 6)");
  h += "<label>Fixed IP address (blank = automatic)</label>";
  h += "<input name='ip' value='" + htmlEscape(e.ip) + "' placeholder='e.g. 192.168.1.250' inputmode='decimal'>";
  h += "<label>Gateway (router) address</label>";
  h += "<input name='gw' value='" + htmlEscape(e.gw) + "' placeholder='e.g. 192.168.1.1' inputmode='decimal'>";
  h += "<label>Subnet mask</label>";
  h += "<input name='mask' value='" + htmlEscape(e.mask) + "' placeholder='255.255.255.0' inputmode='decimal'>";
  h += "<label>DNS server</label>";
  h += "<input name='dns' value='" + htmlEscape(e.dns) + "' placeholder='e.g. 8.8.8.8' inputmode='decimal'>";
  h += "<small>If this one doesn't answer, the board also tries 8.8.8.8.</small>";
  h += "</details>";

  // ── Security ──
  h += sec("updates", "Updates");
  h += checkbox("autoupd", cfgAutoUpd, "Install new versions from GitHub automatically (at 3 am)");
  h += "<small>Either way the board checks once a day and shows when a new version is out; "
       "install it from the <a href='/update'>Update page</a>.</small>";
  h += "<details><summary>Update source (advanced)</summary><label>Address of the builds folder</label>"
       "<input name='updurl' autocapitalize='off' value='" + htmlEscape(cfgUpdUrl) + "'>"
       "<small>Only change this if you build your own copy (a fork) on GitHub.</small></details>";

  h += sec("security", "Security");
  if (cfgPin.length()) h += "<p class='ok'>PIN is on &#10003;</p>";
  else h += "<small>No PIN set: anyone on your Wi-Fi can open this page and change things.</small>";
  h += "<label>" + String(cfgPin.length() ? "New PIN" : "Choose a PIN (4–12 characters)") + "</label>"
       "<input name='pin' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12' placeholder='" +
       String(cfgPin.length() ? "Leave blank to keep the current PIN" : "e.g. 4 digits") + "'>";
  h += "<label>Type it again</label><input name='pin2' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12'>";
  if (cfgPin.length()) h += checkbox("nopin", false, "Remove the PIN");
  h += "<small>After you save, your browser asks for it whenever you open Settings or Update: "
       "user name <b>admin</b>, password = your PIN. The dashboard stays open to view. "
       "Forgotten it? Hold BOOT for 3 seconds: setup mode doesn't ask for it, so you can change it there.</small>";

  // ── Save ──
  h += "<div class='savebar'><button type='submit'>Save &amp; restart</button></div>";
  h += "</form>";

  // ── Firmware (links only) ──
  if (!portalMode) {
    h += sec("firmware", "Firmware");
    h += "<p style='margin:0'>Running <b>v" FW_VERSION "</b> &middot; <a href='/update'>&#11014; Update</a>" +
         String(updAvail ? " &middot; <b style='color:#30d158'>v" + htmlEscape(updVer) + " available</b>" : "") + "</p>";
    h += sec("backup", "Backup &amp; restore");
    h += "<small>Save all your settings to a file, e.g. before a full re-flash or to set up a second board. "
         "History and records stay on the board.</small>"
         "<a class='btnlink' style='background:#2a2a2e' href='/backup'>&#11015; Download settings</a>"
         "<small><a href='/backup?secrets=1'>Download including passwords, keys and sign-ins</a> – keep that file private.</small>"
         "<label>Restore from a file</label><input type='file' id='rf' accept='.json'>"
         "<button type='button' class='b2' onclick='restore()'>Restore &amp; restart</button><small id='rm'></small>"
         "<script>function restore(){const f=document.getElementById('rf').files[0],m=document.getElementById('rm');"
         "if(!f){m.textContent='Choose the .json file first.';return}f.text().then(t=>fetch('/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:t}))"
         ".then(r=>r.text()).then(t=>{m.textContent=t;setTimeout(()=>location.href='/',12000)}).catch(()=>m.textContent='Restore failed.')}</script>";
  }
  h += "<small style='margin-top:16px'>Board Wi-Fi MAC address: " + boardMac() + "</small></div>";
  h += FPSTR(SCAN_JS);
  if (portalMode) h += "<script>scan()</script>";
  h += "</body></html>";
  server.send(200, "text/html", h);
}

void sendMessage(int code, const String &title, const String &body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>" + title + "</h1><p>" + body +
              "</p><a href='/settings'>Go back</a></div></body></html>");
}

void handleCalibrate() {
  if (!authed()) return;
  calibrateMotion();
  sendMessage(200, "Calibrated &#10003;", "This is now the board's normal position.");
}

void handleSave() {
  if (!authed()) return;
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (pin.length() && server.arg("nopin") != "1") {
      if (pin != pin2) { sendMessage(400, "PINs don't match", "Type the same PIN in both boxes. Nothing was saved."); return; }
      if (pin.length() < 4) { sendMessage(400, "PIN too short", "Use at least 4 characters. Nothing was saved."); return; }
    } }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String user = server.arg("user");     user.trim();
  String chs  = server.arg("channels"); chs.trim();
  String key  = server.arg("apikey");   key.trim();
  while (pass.length() && (pass.endsWith("\n") || pass.endsWith("\r") || pass.endsWith(" ") || pass.endsWith("\t")))
    pass.remove(pass.length() - 1);
  while (pass.length() && (pass[0] == ' ' || pass[0] == '\n' || pass[0] == '\r' || pass[0] == '\t'))
    pass.remove(0, 1);
  String twList;
  { String yt, tw; splitLists(chs, yt, tw); yt.trim(); chs = yt;        // a Twitch link pasted in the YouTube box moves across
    twList = normTwitchList(server.arg("twch") + "\n" + tw); }
  bool ytListed = chs.length() > 0;
  if ((!ytListed && !twList.length()) || (ytListed && key.length() == 0 && cfgApiKey.length() == 0)) {
    sendMessage(400, "Missing details", ytListed ? "YouTube channels need the YouTube Data API key." : "Add at least one YouTube or Twitch channel.");
    return;
  }
  String ip = server.arg("ip"), gw = server.arg("gw"), mask = server.arg("mask"), dnsS = server.arg("dns");
  ip.trim(); gw.trim(); mask.trim(); dnsS.trim();
  IPAddress t;
  if ((ip.length() && (!t.fromString(ip) || !t.fromString(gw) || (mask.length() && !t.fromString(mask)))) ||
      (dnsS.length() && !t.fromString(dnsS))) {
    sendMessage(400, "Check the network settings", "A fixed IP needs a valid IP and gateway, e.g. 192.168.1.250 and 192.168.1.1.");
    return;
  }
  // Wi-Fi: remove ticked networks, then add / update the one in the form
  Net keep[MAX_NETS]; int nk = 0;
  int prefOld = server.hasArg("pref") ? server.arg("pref").toInt() : cfgPreferred;
  int prefNew = -1;
  for (int k = 0; k < numNets; k++) if (server.arg("rm" + String(k)) != "1") { if (k == prefOld) prefNew = nk; keep[nk++] = nets[k]; }
  if (ssid.length()) {
    int found = -1;
    for (int k = 0; k < nk; k++) if (keep[k].ssid == ssid) found = k;
    if (found < 0) {
      if (nk >= MAX_NETS) { sendMessage(400, "Too many networks", "You can save up to 5. Remove one first."); return; }
      found = nk++;
      keep[found] = Net();
      keep[found].ssid = ssid;
    }
    Net &n = keep[found];
    if (pass.length()) n.pass = pass;
    n.user = user;
    n.ip = ip; n.gw = ip.length() ? gw : ""; n.mask = ip.length() ? mask : ""; n.dns = dnsS;
    n.compat = server.arg("compat") == "1";
  }
  if (nk == 0) { sendMessage(400, "No Wi-Fi network", "Add at least one Wi-Fi network."); return; }
  for (int k = 0; k < nk; k++) nets[k] = keep[k];
  numNets = nk;
  cfgPreferred = prefNew;
  cfgChannels = chs;
  cfgTwitch = twList;
  if (key.length()) cfgApiKey = key;
  cfgAuto = server.arg("auto") == "1";
  cfgEst = server.arg("est") == "1";
  cfgCelebrate = server.arg("celebrate") == "1";
  cfgLiveAlert = server.arg("livealert") == "1";
  cfgAutoUpd = server.arg("autoupd") == "1";
  { String t = server.arg("ntfy"); t.trim(); t.replace(" ", "-"); cfgNtfy = t;
    String sv = server.arg("ntfysrv"); sv.trim(); if (sv.endsWith("/")) sv.remove(sv.length() - 1);
    if (sv.startsWith("http")) cfgNtfyServer = sv;
    cfgNtfyMask = 0; for (int k = 0; k < 6; k++) if (server.arg("nt" + String(k)) == "1") cfgNtfyMask |= 1 << k; }
  if (server.hasArg("theme")) cfgTheme = server.arg("theme").toInt();
  if (server.hasArg("bright")) cfgBright = server.arg("bright").toInt();
  if (server.hasArg("nbright")) cfgNightBright = server.arg("nbright").toInt();
  applyTheme();
  { String u = server.arg("updurl"); u.trim(); if (u.startsWith("https://")) { if (!u.endsWith("/")) u += "/"; cfgUpdUrl = u; } }
  { String v = server.arg("twid"); v.trim(); if (server.hasArg("twid")) cfgTwId = v;
    v = server.arg("twsec"); v.trim(); if (v.length()) { cfgTwSecret = v; } }
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (server.arg("nopin") == "1") cfgPin = "";
    else if (pin.length()) cfgPin = pin; }
  cfgSummary = server.arg("summary") == "1";
  cfgShake = server.arg("shake") == "1";
  cfgFaceDown = server.arg("facedown") == "1";
  cfgPortrait = server.arg("portrait") == "1";
  cfgFlipPortrait = server.arg("flip") == "1";
  cfgTap = server.arg("tap") == "1";
  if (server.hasArg("idleclk")) cfgIdleClock = constrain(server.arg("idleclk").toInt(), 0, 60);
  String town = server.arg("wxtown"); town.trim();
  if (town.length() && town != cfgWxName && !portalMode) {
    float la, lo; String label;
    if (geocode(town, la, lo, label)) { cfgWxLat = la; cfgWxLon = lo; cfgWxName = label; }
    else { sendMessage(400, "Town not found", "Couldn't find \"" + htmlEscape(town) + "\". Try a nearby town or add the country, e.g. \"Paisley, UK\"."); return; }
  } else if (town.length() && portalMode && town != cfgWxName) { cfgWxName = town; cfgWxLat = cfgWxLon = 0; }   // looked up once online
  String spid = server.arg("spid"); spid.trim();
  String spsec = server.arg("spsecret"); spsec.trim();
  if (spid != cfgSpId) { cfgSpId = spid; cfgSpRefresh = ""; }
  if (spsec.length()) cfgSpSecret = spsec;
  String spredir = server.arg("spredir"); spredir.trim();
  if (spredir != cfgSpRedirect) { cfgSpRedirect = spredir; cfgSpRefresh = ""; }
  if (server.hasArg("tapsens")) cfgTapSens = constrain(server.arg("tapsens").toInt(), 1, 3);
  if (server.hasArg("raceA")) { cfgRaceA = server.arg("raceA").toInt(); cfgRaceB = server.arg("raceB").toInt(); }
  if (server.hasArg("nightStart")) { cfgNightStart = server.arg("nightStart").toInt(); cfgNightEnd = server.arg("nightEnd").toInt(); }
  saveSettings();
  sendMessage(200, "Saved &#10003;", "The board is restarting and will join whichever saved network is in range.");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

// "Connect now": answer the browser first (we're about to leave this network), then switch in loop()
void handleWifiConnect() {
  if (!authed()) return;
  int k = server.arg("n").toInt();
  if (k < 0 || k >= numNets) { sendMessage(400, "Unknown network", "That network isn't saved."); return; }
  sendMessage(200, "Switching to " + htmlEscape(nets[k].ssid) + "&hellip;",
              "The board is leaving this network now, so this page will stop updating. "
              "Join <b>" + htmlEscape(nets[k].ssid) + "</b> yourself and use the new address shown on the board's leaderboard. "
              "If it can't connect, it goes back to the best network it can find.");
  pendingSwitch = k;
}

void handleSpotifyCode() {
  if (!authed()) return;
  String err;
  if (spConnectWithCode(server.arg("url"), err)) {
    saveSettings();
    sendMessage(200, "Spotify connected &#10003;", "Long-press the board's screen and choose Spotify.");
    if (app == APP_SP && !menuOpen) { lastSpPoll = 0; }
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Codes only work once and expire after a few minutes &ndash; open the Spotify link again and paste the new address.");
}

// The relay page sends the browser here with ?code=... after you press Agree on Spotify
void handleSpotifyCallback() {
  if (!authed()) return;
  if (server.hasArg("error")) { sendMessage(400, "Spotify not connected", "Spotify said: " + htmlEscape(server.arg("error"))); return; }
  String err;
  if (spConnectWithCode(server.arg("code"), err)) {
    saveSettings();
    if (app == APP_SP && !menuOpen) lastSpPoll = 0;
    server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>Spotify connected &#10003;</h1><p>Long-press the board's screen and choose Spotify.</p>"
                "<a href='/'>Go to the dashboard</a></div></body></html>");
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Press Connect Spotify on the settings page to try again.");
}

void handleSpotifyDisconnect() {
  if (!authed()) return;
  cfgSpRefresh = ""; sp = SpotifyState();
  saveSettings();
  sendMessage(200, "Spotify disconnected", "You can connect it again any time.");
}

void handleNotFound() {
  if (!portalMode) { server.send(404, "text/plain", "Not found"); return; }
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void registerRoutes() {
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/csv", HTTP_GET, handleApiCsv);
  server.on("/backup", HTTP_GET, handleBackup);
  server.on("/ntfy/test", HTTP_POST, handleNtfyTest);
  server.on("/restore", HTTP_POST, handleRestore);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/update/github", HTTP_POST, handleUpdateGithub);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/spotify/code", HTTP_POST, handleSpotifyCode);
  server.on("/spotify/callback", HTTP_GET, handleSpotifyCallback);
  server.on("/spotify/disconnect", HTTP_POST, handleSpotifyDisconnect);
  server.onNotFound(handleNotFound);
}
