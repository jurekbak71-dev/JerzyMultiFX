'use strict';
const $=id=>document.getElementById(id);
let parameters=window.MF_META.parameters,values=parameters.map(p=>p.default??0),connected=false;
let tab='Efekty',selected=0,section=0,mxSlot=0,advanced=false,order=[0,1,2,3,4],snapshot={},renderSerial=0,lastSignature='',hardwareSnapshot={},selectedPan='',connectionFailed=false;
let detailsExpanded=false,liveRestore=null;
let controllerLayout={},controllerProfile='hotone',selectedControl='F1',midiCapture=null,midiMapOpen=false,lastMidiMapSignature='';
const mdChoices=['Digital','Volante / MultiHead Reel','El Capistan / Tape Echo','Olivera / Oil Can','EC-1 / Tube Echo','Brig / BBD','Deco / Doubletrack','DIG / Dual Digital','Space Echo 202','DM-101','Memory Delay','Reel Echo','Oil Drum Echo'];
const blockNames=['ANALOG','CHORUS / ATTACK / ATOMIC','MD MULTIDELAY','REVERB','IR LOADER'];
// Four Rack Hybrid performance strips use the same live normalized parameters as the detailed pages.
const rackQuickKeys=[['cpd.gain','drive.gain','comp.input'],['clone.rate','clone.depth','clone.mix'],['delayA.time','delayA.feedback','delayA.mix'],['reverb.decay','reverb.size','reverb.mix'],[]];
const sections=[['PREAMP','TAPE DRIVE','DRIVE','COMPRESSOR'],['CHORUS','ATTACK DECAY','ATOMIC CLUSTER'],['DELAY A','DELAY B'],[],[]];
function el(tag,text,cls){let x=document.createElement(tag);if(text!==undefined)x.textContent=text;if(cls)x.className=cls;return x}
function error(e){$('error').textContent=e.message||String(e)}
function button(text,fn,cls){const b=el('button',text,cls);b.onclick=()=>Promise.resolve().then(fn).catch(error);return b}
function find(key){return parameters.find(p=>p.key===key)?.id}
function permutation(n){const a=[0,1,2,3,4];for(let i=0;i<n;i++){let k=3;while(a[k]>=a[k+1])k--;let l=4;while(a[l]<=a[k])l--;[a[k],a[l]]=[a[l],a[k]];a.push(...a.splice(k+1).reverse())}return a}
async function api(path,body,raw=false){let r=await fetch('/api/'+path,{method:body===undefined?'GET':'POST',headers:{...(raw?{'X-IR-Name':encodeURIComponent(body.name||'IR.wav')}:{'Content-Type':'application/json'})},body:body===undefined?undefined:raw?body:JSON.stringify(body)});let data=await r.json();if(!r.ok||data.ok===false){const e=Error(data.error||'Nie udało się wykonać operacji.');e.status=r.status;throw e;}return data}
const queues=new Map(),pendingEdits=new Map();let sending=false,editRevision=0,polling=false,drainPromise=null;
async function set(id,value){
 if(id===undefined)throw Error('Brak parametru w tej wersji programu.');
 value=Math.min(1,Math.max(0,Number(value)));
 if(!pendingEdits.has(id))pendingEdits.set(id,values[id]);
 queues.set(id,value);values[id]=value;editRevision++;updateControls();
 if(sending)return drainPromise;
 sending=true;
 drainPromise=(async()=>{try{while(queues.size){const [key,v]=queues.entries().next().value;queues.delete(key);await api('set',{id:key,value:v});if(queues.has(key))pendingEdits.set(key,v);else pendingEdits.delete(key)}}
 catch(e){for(const [key,previous] of pendingEdits)values[key]=previous;pendingEdits.clear();queues.clear();editRevision++;throw e}
 finally{sending=false;updateControls()}})();
 return drainPromise;
}
const switchNames={'master.bypass':'CAŁY TOR','block.0.bypass':'ANALOG','block.1.bypass':'CHORUS / ATTACK / ATOMIC','block.2.bypass':'MD MULTIDELAY','block.3.bypass':'REVERB','block.4.bypass':'IR LOADER','cpd.bypass':'PREAMP','td.bypass':'TAPE DRIVE','td.optoBypass':'KOMPRESOR TAŚMY','attack.returnBypass':'TOR RETURN','drive.bypass':'DRIVE','comp.bypass':'COMPRESSOR','clone.bypass':'CHORUS','attack.bypass':'ATTACK DECAY','atomic.bypass':'ATOMIC CLUSTER','delayA.bypass':'DELAY A','delayB.bypass':'DELAY B'};
function name(p){
 if(switchNames[p.key])return switchNames[p.key];
 const bank=p.key.startsWith('delayA.')?'DELAY A':p.key.startsWith('delayB.')?'DELAY B':'';
 if(bank&&isBypass(p)){const kind=p.key.includes('.oil.')?'OIL':p.key.includes('.mx.')?'MX':'MEMORY';return bank+' / '+kind+' / '+(p.key.endsWith('inbypass')?'PREAMP':p.key.endsWith('outbypass')?'WYJŚCIE':'EFEKT')}
 const mx=p.key.match(/^delay([AB])\.mx\.slot([01])_/);
 if(mx){const label=(p.label||p.name.split(' / ').pop()).replace(/^(?:Silnik [AB]: |DELAY [AB] \/ MX [12]: )/,'');return 'DELAY '+mx[1]+' / MX '+(Number(mx[2])+1)+': '+label}
 return p.label||p.name.split(' / ').pop();
}
function formatted(p,v){if(p.key==='td.optoMakeup')return (24*v-12>=0?'+':'')+(24*v-12).toFixed(1)+' dB';if(p.key==='attack.makeup'||p.key==='comp.makeup'||p.key.endsWith('.oil.icmakeup')||p.key.endsWith('.oil.ocmakeup')){const db=p.min+(p.max-p.min)*v;return (db>=0?'+':'')+db.toFixed(1)+' dB'}if(p.key==='clone.rate')return (.05*Math.pow(160,v)).toFixed(2)+' Hz';if(p.key==='clone.level')return (v*24-12).toFixed(1)+' dB';if(p.key==='atomic.volume')return (20*Math.log10(Math.max(1e-6,2*v))).toFixed(1)+' dB';if(p.key==='attack.volume')return (39*v-26).toFixed(1)+' dB';if(p.key==='attack.hvol')return (36*v-24).toFixed(1)+' dB';if(p.key==='attack.attack'||p.key==='attack.decay'){if(p.key==='attack.attack'&&v<=.00001||p.key==='attack.decay'&&v>=.99999)return 'OFF';return (4*Math.pow(values[find('attack.poly')]>=.5?1000:2000,v)).toFixed(1)+' ms'}if(p.key.includes('.mx.')&&p.key.endsWith('_time'))return Math.pow(2500,v).toFixed(1)+' ms';if(p.key.includes('.mx.')&&p.key.endsWith('.inputTrim'))return (36*v-18).toFixed(1)+' dB';if(p.key.includes('.mx.')&&p.key.endsWith('.outputTrim'))return (30*v-18).toFixed(1)+' dB';if(p.key.includes('.memory.')&&p.key.endsWith('.delay'))return (20+530*v).toFixed(1)+' ms';if(p.key==='atomic.atoms')return Math.round(Math.pow(64,v))+' atoms';if(p.key==='atomic.speed')return (2000*Math.pow(1/750,v)).toFixed(1)+' ms';if(p.min===0&&p.max===1)return (v*100).toFixed(1)+' %';return (p.min+(p.max-p.min)*v).toFixed(p.steps?0:1)+' '+(p.unit||'')}
function signature(){return [tab,selected,section,mxSlot,advanced,...parameters.filter(p=>p.key.endsWith('.type')||p.key.endsWith('_algorithm')||p.key.endsWith('.sync')||p.key.endsWith('_sync')||p.key.endsWith('.tempoSync')).map(p=>values[p.id])].join(':')}
function learnButton(id){return button('MIDI Learn · ON/OFF',()=>set(find('midi.learn'),(id+1)/window.MF_META.controlCount),'learn')}
function isBypass(p){return p.bypass===true||/bypass$/i.test(p.key)}
function toggleControl(p){let b=button('',async()=>{await set(p.id,values[p.id]>=.5?0:1);if(p.key.endsWith('.sync')||p.key.endsWith('.tempoSync'))await renderPanel();else updateControls()});b.dataset.parameter=p.id;b.dataset.kind='toggle';b.setAttribute('aria-label',name(p));paintSwitch(b,p);return b}
function control(p,compact=false){
 const family=p.key==='delayA.type'||p.key==='delayB.type',bank=p.key.startsWith('delayB.')?'delayB':'delayA';const choices=family?mdChoices:p.choices?p.choices.split('|'):p.steps>1&&p.steps<=64?Array.from({length:p.steps+1},(_,i)=>String(p.min+(p.max-p.min)*i/p.steps)):[],v=values[p.id]??0;
 if(p.control==='switch'||(!p.choices&&p.steps===1)||p.key==='atomic.smooth'||p.key==='atomic.flat'){let label=el('label',name(p));label.append(toggleControl(p),learnButton(p.id));return label}
 if(choices.length>1){const label=el('label',name(p));const select=el('select');select.dataset.parameter=p.id;select.dataset.kind='choice';select.setAttribute('aria-label',name(p));choices.forEach((text,i)=>{let o=el('option',text);o.value=i/(choices.length-1);select.append(o)});let chosen=Math.round(v*(choices.length-1));if(family){const type=Math.round(v*4);chosen=type===0?0:type===1?1+Math.round(values[find(bank+'.mx.slot0_algorithm')]*8):type+8;}select.value=String(chosen/(choices.length-1));select.onchange=async()=>{try{if(family){const index=Math.round(Number(select.value)*12),type=index===0?0:index<=9?1:index-8;await set(p.id,type/4);if(index>0)await set(find(bank+'.mix'),1);if(type===1){await set(find(bank+'.mx.slot0_algorithm'),(index-1)/8);await set(find(bank+'.mx.slot1_enable'),0)}}else await set(p.id,select.value);await renderPanel()}catch(e){error(e)}};label.append(select);return label}

 if(p.control==='number'||p.steps>64&&!p.choices){const label=el('label',name(p)),input=el('input');input.type='number';input.min=p.min;input.max=p.max;input.step=(p.max-p.min)/p.steps;input.value=p.min+(p.max-p.min)*v;input.dataset.parameter=p.id;input.dataset.kind='number';input.onchange=()=>set(p.id,(Number(input.value)-p.min)/(p.max-p.min)).catch(error);label.append(input);return label}
 if(compact){const label=el('label',name(p)),input=el('input'),out=el('output',formatted(p,v),'value');input.type='range';input.min=0;input.max=1;input.step=.001;input.value=v;input.dataset.parameter=p.id;input.dataset.kind='mini';input.setAttribute('aria-label',name(p));let pending=null;input.oninput=()=>{out.textContent=formatted(p,Number(input.value));if(pending===null)pending=setTimeout(()=>{pending=null;set(p.id,input.value).catch(error)},20)};input.onchange=()=>{clearTimeout(pending);pending=null;set(p.id,input.value).catch(error)};label.append(input,out);return label}
 const c=el('label',undefined,'control'),knob=el('div',undefined,'rotary'),needle=el('span',undefined,'needle'),input=el('input'),value=el('output',formatted(p,v),'value');
 input.type='range';input.min=0;input.max=1;input.step=.001;input.value=v;input.dataset.parameter=p.id;input.dataset.kind='knob';if(p.key==='clone.color'&&values[find('clone.model')]*13<2.5){input.disabled=true;c.title='COLOR nie jest używany w tym modelu, zgodnie z oryginałem.';c.style.opacity='.4'}input.setAttribute('aria-label',name(p));input.setAttribute('aria-valuetext',formatted(p,v));needle.style.transform=`rotate(${-135+270*v}deg)`;
 const show=()=>{value.textContent=formatted(p,Number(input.value));needle.style.transform=`rotate(${-135+270*input.value}deg)`;input.setAttribute('aria-valuetext',value.textContent)};
 let pending=null;input.oninput=()=>{show();if(pending===null)pending=setTimeout(()=>{pending=null;set(p.id,input.value).catch(error)},20)};input.onchange=()=>{clearTimeout(pending);pending=null;show();set(p.id,input.value).catch(error)};
 let start=null;input.onpointerdown=e=>{e.preventDefault();input.focus();input.setPointerCapture(e.pointerId);start={y:e.clientY,v:Number(input.value)}};input.onpointermove=e=>{if(!start)return;input.value=Math.min(1,Math.max(0,start.v+(start.y-e.clientY)*(e.shiftKey?.0005:.005)));input.oninput()};input.onpointerup=e=>{if(!start)return;start=null;input.releasePointerCapture(e.pointerId);input.onchange()};input.onpointercancel=()=>{start=null;input.onchange()};input.ondblclick=()=>{input.value=p.default;input.onchange()};
 knob.append(needle,input);c.append(knob,el('span',name(p),'label'),value,button('MIDI Learn',()=>set(find('midi.learn'),(p.id+1)/window.MF_META.controlCount),'learn'));return c;
}
function paintSwitch(x,p){
 const on=isBypass(p)?values[p.id]<.5:values[p.id]>=.5;
 x.textContent=on?'WŁĄCZONY':isBypass(p)?'WYŁĄCZONY / BYPASS':'WYŁĄCZONY';
 x.className='toggle '+(on?'on':'off');
 x.setAttribute('aria-pressed',String(on));x.setAttribute('aria-label',name(p)+': '+x.textContent);
 x.setAttribute('aria-busy',String(pendingEdits.has(p.id)));
 x.title=pendingEdits.has(p.id)?'Wysyłanie zmiany…':name(p)+' — kliknij, aby '+(on?'wyłączyć':'włączyć');
}
function updateControls(){
 const master=parameters.find(p=>p.key==='master.bypass');if(master){$('master').dataset.parameter=master.id;$('master').dataset.kind='toggle';paintSwitch($('master'),master)}
 document.querySelectorAll('[data-parameter]').forEach(x=>{
  const id=Number(x.dataset.parameter),p=parameters.find(p=>p.id===id),v=values[id]??0;if(!p)return;
  if(x.dataset.kind==='toggle'){paintSwitch(x,p);return}
  if(document.activeElement===x||pendingEdits.has(id))return;
  if(x.dataset.kind==='choice'){const n=x.options.length-1;if(p.key==='delayA.type'||p.key==='delayB.type'){const bank=p.key.startsWith('delayB.')?'delayB':'delayA',type=Math.round(v*4),index=type===0?0:type===1?1+Math.round(values[find(bank+'.mx.slot0_algorithm')]*8):type+8;x.value=String(index/12)}else x.value=String(Math.round(v*n)/n)}
  else if(x.dataset.kind==='number'){x.value=p.min+(p.max-p.min)*v}
  else if(x.dataset.kind==='mini'){x.value=v;x.parentElement.querySelector('output').textContent=formatted(p,v)}
  else{x.value=v;x.parentElement.querySelector('.needle').style.transform=`rotate(${-135+270*v}deg)`;x.closest('.control').querySelector('output').textContent=formatted(p,v)}
 });
 const notice=$('signal-status');if(notice){const masterOff=values[find('master.bypass')]>=.5,blockOff=values[find('block.'+selected+'.bypass')]>=.5;notice.hidden=!masterOff&&!blockOff;notice.textContent=masterOff?'CAŁY TOR WYŁĄCZONY / BYPASS — ustawienia efektu są zapamiętane, ale nie przetwarzają sygnału.':blockOff?blockNames[selected]+' WYŁĄCZONY / BYPASS — włącz blok, aby usłyszeć jego efekty.':''}
 document.querySelectorAll('[data-block]').forEach(card=>{const n=Number(card.dataset.block),off=values[find('block.'+n+'.bypass')]>=.5;card.className='block'+(n===selected?' selected':'')+(off?' off':'')});
}
function renderShell(){let tabs=$('tabs');tabs.replaceChildren(...['Efekty','Ustawienia','MIDI i Bluetooth','Presety'].map(t=>button(t,()=>{tab=t;if(t!=='Ustawienia'){midiMapOpen=false;midiCapture=null}lastSignature='';render()},t===tab?'active':'')));}
async function reorder(from,to){const a=[...order],x=a.indexOf(from),y=a.indexOf(to);[a[x],a[y]]=[a[y],a[x]];for(let i=0;i<120;i++)if(permutation(i).every((v,n)=>a[n]===v)){await set(find('chain.order'),i/119);break}await render()}
function activeDSP(key){const id=find(key);return id!==undefined&&(values[id]??0)<.5}
function dspLatencyDetails(){
 const rate=snapshot.sampleRate||48000;
 const ms=samples=>(samples*1000/rate).toFixed(2)+' ms';
 const enabled=activeDSP('master.bypass');
 const analog=enabled&&activeDSP('block.0.bypass');
 const mod=enabled&&activeDSP('block.1.bypass');
 const ir=enabled&&activeDSP('block.4.bypass')&&(values[find('ir.mix')]||0)>.00001;
 const drive=analog&&activeDSP('drive.bypass')?16:0;
 const attack=mod&&activeDSP('attack.bypass')?((values[find('attack.poly')]||0)>=.5?2064:16):0;
 const atomic=mod&&activeDSP('atomic.bypass')?2048:0;
 const irDelay=ir?128:0;
 return {rate,ms,drive,attack,atomic,irDelay,reported:snapshot.latencySamples||0};
}
async function enableLiveMode(){
 if(liveRestore)return;
 const keys=['atomic.bypass','attack.poly'];
 liveRestore=keys.map(key=>({key,id:find(key),value:values[find(key)]}));
 try{for(const {key,id} of liveRestore)if(id!==undefined)await set(id,key==='atomic.bypass'?1:0);
 await poll();await render();
 }catch(e){const old=liveRestore;liveRestore=null;for(const x of old)if(x.id!==undefined)await set(x.id,x.value).catch(()=>{});throw e}
}
async function restoreLiveMode(){
 if(!liveRestore)return;
 const saved=liveRestore;for(const x of saved)if(x.id!==undefined)await set(x.id,x.value);
 liveRestore=null;await poll();await render();
}
async function render(){renderShell();let main=$('main');main.replaceChildren();if(!connected){main.append(el('p','Łączenie z MultiFX…','empty'));return}if(tab==='Efekty'){
 order=permutation(Math.round((values[find('chain.order')]||0)*119));let chain=el('nav',undefined,'chain');chain.id='blocks';
 order.filter(n=>n!==4).concat(order.includes(4)?[4]:[]).forEach(n=>{const i=order.indexOf(n),off=values[find('block.'+n+'.bypass')]>=.5,card=el('article',undefined,'block'+(n===selected?' selected':'')+(off?' off':''));card.dataset.block=n;card.draggable=true;card.ondragstart=e=>e.dataTransfer.setData('text/plain',String(n));card.ondragover=e=>e.preventDefault();card.ondrop=e=>{e.preventDefault();reorder(Number(e.dataTransfer.getData('text/plain')),n).catch(error)};card.append(el('small',String(i+1).padStart(2,'0')),button(blockNames[n],()=>{selected=n;section=0;advanced=false;detailsExpanded=true;lastSignature='';render()},'choose'),toggleControl(parameters.find(p=>p.key==='block.'+n+'.bypass')),learnButton(find('block.'+n+'.bypass')));if(n<4){const quick=el('div',undefined,'rack-quick');for(const key of rackQuickKeys[n]){const p=parameters.find(p=>p.key===key);if(p)quick.append(control(p))}card.append(quick)}chain.append(card)});main.append(chain);
 let editBar=el('div',undefined,'rack-editor-bar');
 editBar.append(button(liveRestore?'PRZYWRÓĆ BRZMIENIE':'LIVE · MNIEJSZA LATENCJA',()=>liveRestore?restoreLiveMode():enableLiveMode(),liveRestore?'rack-live-restore':'rack-live-enable'));
 editBar.append(el('span','LIVE: czasowo wyłącza Atomic Cluster i przełącza Attack/Decay z POLY na MONO; brzmienie może się zmienić.','hint'));editBar.append(button(detailsExpanded?'ZWIŃ EDYTOR SZCZEGÓŁOWY':'OTWÓRZ EDYTOR SZCZEGÓŁOWY',()=>{detailsExpanded=!detailsExpanded;render()},'rack-editor-toggle'),el('span','Cztery moduły na górze to sterowanie na żywo. IR jest osobnym blokiem. Numer pokazuje rzeczywistą kolejność DSP.','hint'));main.append(editBar);let subs=el('nav',undefined,'subtabs');sections[selected].forEach((n,i)=>subs.append(button(n,()=>{section=i;advanced=false;renderPanel()},i===section?'active':'')));subs.id='subtabs';subs.hidden=!detailsExpanded;main.append(subs,el('section',undefined,'pedal'));main.lastElementChild.id='controls';main.lastElementChild.hidden=!detailsExpanded;await renderPanel();
 }else if(tab==='Ustawienia'){if(midiMapOpen)renderMidiMap();else renderSettings()}else if(tab==='MIDI i Bluetooth')await renderHardware();else renderPresets();lastSignature=signature();updateControls()}
async function renderPanel(){if(tab!=='Efekty'||!$('controls'))return;const serial=++renderSerial;const result=await api('view',{block:selected,section,mxSlot,advanced:advanced?1:0});if(serial!==renderSerial)return;let panel=$('controls');panel.replaceChildren();panel.className='pedal '+(selected===1?['clone','attack','atomic'][section]:selected===2?'md':'');const heading=el('div',undefined,'pedalhead');heading.append(el('h2',sections[selected][section]||blockNames[selected]));let right=el('div',undefined,'row');const ownBypass=selected===1?find(['clone.bypass','attack.bypass','atomic.bypass'][section]):undefined;if(ownBypass!==undefined)right.append(toggleControl(parameters.find(p=>p.id===ownBypass)),learnButton(ownBypass));right.append(button(advanced?'Widok podstawowy':'Ustawienia efektu',()=>{advanced=!advanced;renderPanel()}));heading.append(right);const notice=el('p',undefined,'signal-status');notice.id='signal-status';panel.append(heading,notice);
 $('subtabs').querySelectorAll('button').forEach((b,i)=>b.className=i===section?'active':'');
 if(selected===2&&advanced&&Math.round(values[find((section?'delayB':'delayA')+'.type')]*4)===1){const row=el('div',undefined,'subtabs');row.append(el('span','MX — wybierz silnik wewnętrzny:'),button('MX 1',()=>{mxSlot=0;renderPanel()},mxSlot===0?'active':''),button('MX 2',()=>{mxSlot=1;renderPanel()},mxSlot===1?'active':''));panel.append(row)}
 if(selected===4){const box=el('label','Wczytaj plik IR WAV','inputfile'),input=el('input');input.type='file';input.accept='.wav';input.onchange=async()=>{if(!input.files[0])return;try{await api('ir',input.files[0],true);await poll();box.append(el('p','IR wczytany i włączony.','notice'))}catch(e){error(e)}};box.append(input,el('p',snapshot.ir?'Wczytany: '+snapshot.ir:'Nie wczytano IR.','hint'));panel.append(box)}
 const common=el('section',undefined,'common'),knobs=el('section',undefined,'knobs');for(const id of result.ids){if(id===ownBypass)continue;const p=parameters.find(p=>p.id===id);if(!p)continue;const general=selected===2&&(p.key.startsWith('md.')||/^delay[AB]\.(type|bypass|level|mix)$/.test(p.key)||p.key.includes('.mx.')&&!p.key.includes('_algo'));const isCompact=general||p.key==='atomic.stereo'||p.control==='switch'||p.control==='select'||p.control==='number'||p.steps>0||p.key==='atomic.smooth'||p.key==='atomic.flat'||p.choices?.includes('|');(isCompact?common:knobs).append(control(p,isCompact))}if(common.children.length)panel.append(common);panel.append(knobs);
 if(selected===0&&section===2&&values[find('drive.type')]<.25&&values[find('fuzz.model')]*19>5.5)panel.append(el('p','Wybrano oryginalny model obwodowy. Wymaga znacznie więcej CPU; modele Legacy są lżejsze do gry na żywo.','notice'));
 if(selected===1&&section===1)panel.append(button('TRIGGER',async()=>{await set(find('attack.trigger'),1);await new Promise(r=>setTimeout(r,120));await set(find('attack.trigger'),0)}),el('p','Attack Decay dodaje własne opóźnienie. Wyłączenie tego efektu usuwa jego opóźnienie z toru.','hint'));
 if(selected===2){const bank=section?'delayB.':'delayA.',macro=el('section',undefined,'box');macro.append(el('h3','WSPÓLNE MIDI LEARN — '+(section?'DELAY B':'DELAY A')),el('p','Jedno CC na funkcję działa po zmianie silnika. Czas przy SYNC ustawia się podziałem rytmicznym; wtedy gałka czasu nie zmienia słyszalnego tempa.','hint'));for(const [key,label] of [['time','CZAS'],['feedback','FEEDBACK'],['mix','POZIOM EFEKTU / MIX']]){const id=find(bank+key);macro.append(button('Przypisz CC: '+label,()=>set(find('midi.learn'),(id+1)/window.MF_META.controlCount),'learn'))}panel.append(macro,el('p','Parametry dotyczą tylko wybranej rodziny i modelu. BANK MIX miesza cały procesor; pozostałe regulatory należą do oryginalnego silnika.','hint'))}
 lastSignature=signature();updateControls();}
function row(...items){let r=el('div',undefined,'row');r.append(...items);return r}
function selectControl(label,choices,value,onchange){let box=el('label',label),s=el('select');s.setAttribute('aria-label',label);choices.forEach(([v,t])=>{let o=el('option',t);o.value=v;s.append(o)});s.value=value;s.onchange=()=>onchange(s.value);box.append(s);return box}
const controllerNames={hotone:'Hotone Ampero Control',easycontrol9:'WORLDE EASYCONTROL.9'};
function midiBinding(profile,control){return controllerLayout[profile]?.[control]}
function ccTarget(cc){const map=find('midi.cc.'+cc);return map===undefined?-1:Math.round(-1+window.MF_META.controlCount*(values[map]??0))}
function controllerMapSignature(){return Object.entries(controllerLayout[controllerProfile]||{}).map(([id,b])=>id+':'+b.kind+':'+b.number+':'+(b.kind==='cc'?ccTarget(b.number):'pc')).sort().join('|')}
function controllerButton(profile,id,title){
 const binding=midiBinding(profile,id),target=binding?.kind==='cc'?ccTarget(binding.number):-1;
 const p=parameters[target],b=button(id,()=>{selectedControl=id;renderMidiMap()},'map-control'+(selectedControl===id&&controllerProfile===profile?' selected':''));
 b.title=title+' · '+(binding?binding.kind.toUpperCase()+' '+binding.number+(p?' → '+name(p):''):'naciśnij, aby przypisać komunikat');
 b.setAttribute('aria-label',b.title);
 if(binding)b.append(el('small',binding.kind.toUpperCase()+' '+binding.number));
 if(p)b.append(el('small',name(p)));
 return b;
}
function controllerDrawing(){
 const shell=el('div',undefined,'controller-shell '+controllerProfile);
 if(controllerProfile==='hotone'){
  shell.append(el('div','HOTONE  ·  AMPERO CONTROL','controller-brand'),el('div','MIDI','controller-screen'));
  const feet=el('div',undefined,'controller-feet');
  for(let n=1;n<=4;n++)feet.append(controllerButton('hotone','F'+n,'Przełącznik nożny '+n));
  shell.append(feet,controllerButton('hotone','EXP','Pedał ekspresji podłączony do Hotone'));
 }else{
  shell.append(el('div','WORLDE  ·  EASYCONTROL.9','controller-brand'));
  const channels=el('div',undefined,'controller-channels');
  for(let n=1;n<=9;n++){
   const channel=el('div',undefined,'controller-channel');
   channel.append(controllerButton('easycontrol9','K'+n,'Pokrętło '+n),controllerButton('easycontrol9','S'+n,'Suwak '+n),controllerButton('easycontrol9','B'+n,'Przycisk '+n));
   channels.append(channel);
  }
  const face=el('div',undefined,'easycontrol-face'),left=el('div',undefined,'controller-left');
  for(const [id,title] of [['ENC','Duże pokrętło'],['X','Crossfader'],['BANK','Bank']])left.append(controllerButton('easycontrol9',id,title));
  face.append(left,channels);shell.append(face);
  const extra=el('div',undefined,'controller-extra');
  for(const [id,title] of [['LOOP','Pętla'],['BACK','Wstecz'],['FORWARD','Naprzód'],['STOP','Stop'],['PLAY','Play'],['RECORD','Record']])
   extra.append(controllerButton('easycontrol9',id,title));
  shell.append(extra);
 }
 return shell;
}
function midiMapDetail(){
 const box=el('section',undefined,'box map-detail'),binding=midiBinding(controllerProfile,selectedControl);
 box.append(el('h3',controllerNames[controllerProfile]+' · '+selectedControl));
 if(midiCapture)box.append(el('p','Naciśnij teraz tę kontrolkę na urządzeniu. Oczekuję komunikatu CC lub PC…','notice'));
 else box.append(el('p',binding?`${binding.kind.toUpperCase()} ${binding.number} · kanał ${binding.channel}`:'Brak rozpoznanego komunikatu.','hint'));
 box.append(button(binding?'Rozpoznaj ponownie':'Rozpoznaj kontrolkę',()=>{
  midiCapture={profile:controllerProfile,control:selectedControl,after:snapshot.midiEvents||0};renderMidiMap();
 },'primary'));
 if(midiCapture)box.append(button('Anuluj rozpoznawanie',()=>{midiCapture=null;renderMidiMap()}));
 if(binding?.kind==='pc')box.append(el('p',`PC ${binding.number} wczytuje slot program${binding.number}. Ustawisz ten slot w zakładce Presety.`, 'hint'));
 if(binding?.kind==='cc'){
  const map=find('midi.cc.'+binding.number),target=ccTarget(binding.number),current=parameters[target];
  box.append(el('p',current?'Steruje: '+name(current):'Ten CC nie ma przypisanej funkcji.','hint'));
  const search=el('input');search.type='search';search.placeholder='Szukaj efektu lub parametru';search.setAttribute('aria-label','Szukaj parametru MIDI');
  const choice=el('select');choice.setAttribute('aria-label','Funkcja MIDI');
  const fill=()=>{
   const query=search.value.toLocaleLowerCase('pl').trim();
   const options=parameters.filter(p=>p.id<window.MF_META.controlCount&&!p.key.startsWith('midi.')&&p.key!=='factory.program'&&(!query||(p.key+' '+name(p)).toLocaleLowerCase('pl').includes(query)));
   if(current){const index=options.findIndex(p=>p.id===current.id);if(index>=0)options.splice(index,1);options.unshift(current)}
   choice.replaceChildren();const none=el('option','Brak przypisania');none.value='-1';choice.append(none);
   for(const p of options.slice(0,80)){const option=el('option',name(p)+' · '+p.key);option.value=String(p.id);choice.append(option)}
   choice.value=String(target);
  };
  search.oninput=fill;fill();
  choice.onchange=async()=>{try{await set(map,(Number(choice.value)+1)/window.MF_META.controlCount);renderMidiMap()}catch(e){error(e)}};
  box.append(search,choice,el('p','Przypisanie dotyczy bieżącego brzmienia. Zapisz preset, aby je zachować. CC o tym samym numerze z obu kontrolerów używa tej samej funkcji.','hint'));
 }
 return box;
}
function renderMidiMap(){
 if(!midiMapOpen)return;
 const main=$('main');main.replaceChildren();
 const box=el('section',undefined,'box controller-map');
 box.append(row(button('← Ustawienia',()=>{midiMapOpen=false;midiCapture=null;renderSettings()}),el('h2','Mapa kontrolerów MIDI')));
 const tabs=el('nav',undefined,'subtabs');
 for(const profile of Object.keys(controllerNames))tabs.append(button(controllerNames[profile],()=>{controllerProfile=profile;selectedControl=profile==='hotone'?'F1':'K1';midiCapture=null;renderMidiMap()},profile===controllerProfile?'active':''));
 const live=el('p',undefined,'notice');live.id='midi-map-live';
 box.append(tabs,el('p','Kliknij kontrolkę na schemacie, wybierz „Rozpoznaj” i naciśnij ją na urządzeniu. Potem wybierz funkcję MultiFX. Układ jest zapamiętany; mapowanie CC należy do presetu.','hint'),live,controllerDrawing(),midiMapDetail());
 main.append(box);
 lastMidiMapSignature=controllerMapSignature();
 updateMidiMapLive();
}
async function openMidiMap(){const data=await api('controller-layout');controllerLayout=data.layout||{};midiMapOpen=true;renderMidiMap()}
function updateMidiMapLive(){const x=$('midi-map-live');if(!x)return;const raw=snapshot.lastMidi||0,kind=(raw>>16)&240,number=(raw>>8)&127;if(!snapshot.midiEvents){x.textContent='Nie odebrano jeszcze MIDI.';return}x.textContent='Ostatni komunikat: '+(kind===176?'CC '+number:kind===192?'PC '+number:'MIDI '+((raw>>16)&255))+' · kanał '+(((raw>>16)&15)+1)}
async function consumeMidiCapture(state){
 if(!midiCapture||!midiMapOpen||state.midiEvents<=midiCapture.after)return;
 const status=(state.lastMidi>>16)&255,kind=status&240;
 if(kind!==176&&kind!==192){midiCapture=null;$('error').textContent='Ta kontrolka wysłała komunikat inny niż CC lub PC. Ustaw ją w kontrolerze na CC/PC i spróbuj ponownie.';renderMidiMap();return}
 const selected=midiCapture;midiCapture=null;
 const response=await api('controller-layout',{profile:selected.profile,control:selected.control,message:{kind:kind===176?'cc':'pc',number:(state.lastMidi>>8)&127,channel:(status&15)+1}});
 controllerLayout=response.layout;renderMidiMap();
}
async function resetAllMidi(){
 if(!window.confirm('Usunąć wszystkie przypisania CC i ekspresji ze wszystkich presetów MultiFX oraz odłączyć sloty PC? Brzmienia i połączenia kontrolerów zostaną zachowane.'))return;
 if(sending)await drainPromise;
 const result=await api('midi-reset',{confirm:'CLEAR_ALL_MIDI'});
 const state=await api('state');snapshot=state;values=state.values;editRevision++;
 midiCapture=null;
 renderSettings();
 $('midi-reset-result').textContent=`Usunięto przypisania MIDI z bieżącego brzmienia i ${result.presets} zapisanych presetów. Sloty PC zachowano jako presety z dopiskiem „bez-PC”.`;
}
function renderSettings(){const main=$('main'),audio=el('section',undefined,'box');audio.append(el('h2','Dźwięk i opóźnienie'));let inputMode=String(snapshot.inputMode||0);audio.append(row(selectControl('Wejście instrumentu ',[['0','STEREO — wejście L i R'],['1','MONO — tylko wejście L'],['2','MONO — tylko wejście R']],inputMode,v=>inputMode=v),button('Zastosuj wejście',async()=>{await api('hardware',{operation:'input-mode',mode:Number(inputMode)});await poll()})),el('p','Dla sygnału mono wybierz ten sam kanał co w VST. Nie sumujemy dwóch wejść ani nie dodajemy dodatkowego gainu.','hint'));
 const tempo=el('input');tempo.type='number';tempo.min=20;tempo.max=400;tempo.step=.1;tempo.value=snapshot.bpm||120;tempo.setAttribute('aria-label','Tempo BPM');audio.append(el('p','Dla opóźnień i modulacji zsynchronizowanych ustaw tempo takie samo jak w projekcie VST.','hint'),row(el('span','TEMPO BPM'),tempo,button('Zastosuj tempo',async()=>{await api('hardware',{operation:'tempo',bpm:Number(tempo.value)});await poll()})));
 const ld=dspLatencyDetails();
 audio.append(el('p','Opóźnienie samego DSP: '+ld.ms(ld.reported)+' · Drive '+ld.ms(ld.drive)+' · Attack/Decay '+ld.ms(ld.attack)+' · Atomic '+ld.ms(ld.atomic)+' · IR '+ld.ms(ld.irDelay),'hint'));
 if(ld.attack>=2064||ld.atomic>=2048){
  audio.append(el('p','WYSOKIE OPÓŹNIENIE ALGORYTMICZNE: tryb LIVE na ekranie Efekty może je zmniejszyć. Nie zastępuje to regulacji bufora JACK ani pomiaru całego toru USB.','warning'));
 }
 audio.append(el('p',snapshot.realtime===true?'JACK: tryb czasu rzeczywistego aktywny':snapshot.realtime===false?'JACK: brak trybu czasu rzeczywistego — możliwe przerwy przy dużym obciążeniu':'JACK: tryb czasu rzeczywistego nie został zgłoszony','hint'));
 let chosen=snapshot.bufferSamples||128;audio.append(el('p',`${snapshot.sampleRate||0} Hz · bufor ${chosen} próbek · opóźnienie efektów ${((snapshot.latencySamples||0)*1000/(snapshot.sampleRate||48000)).toFixed(2)} ms`));audio.append(row(selectControl('Bufor ',[64,128,256,512,1024].map(v=>[v,v+' próbek']),chosen,v=>chosen=Number(v)),button('Zastosuj bufor',async()=>{await api('hardware',{operation:'buffer',buffer:chosen});await poll();render()},'primary')));audio.append(row(button('PRÓBA 64 PRÓBEK · MNIEJSZA LATENCJA',async()=>{const before=snapshot.bufferSamples||128;try{await api('hardware',{operation:'buffer',buffer:64});await poll();await render()}catch(e){if(before!==64)await api('hardware',{operation:'buffer',buffer:before}).catch(()=>{});throw e}},'primary'),el('span','W razie trzasków lub wzrostu licznika PRZERWY przywróć 128 próbek. To zmienia bufor JACK, nie opóźnienie DSP.','hint')));
 audio.append(el('p','HeadRush Core: sprawdź ustawienie USB Audio i tryb monitorowania. Równoległy dźwięk bezpośredni z HeadRush oraz opóźniony sygnał z Pi mogą powodować słyszalny dubel. Nie zmieniaj trybu USB bez sprawdzenia toru odsłuchowego.','hint'));
 audio.append(button('Uruchom / napraw audio',async()=>{await api('hardware',{operation:'repair-audio'});$('status').textContent='Ponowne uruchamianie audio…'}));audio.append(el('p','Mniejszy bufor zmniejsza opóźnienie, ale bardziej obciąża Raspberry Pi. Jeśli słychać trzaski i rośnie licznik PRZERWY, wybierz większy. 128 to punkt startowy, nie gwarancja.','hint'));audio.append(el('p','Opóźnienie efektów nie obejmuje bufora, USB i przetworników HeadRush. Zmiana bufora nie usuwa opóźnienia aktywnego Attack Decay lub Atomic.','hint'));main.append(audio);
 const routing=el('section',undefined,'box');routing.append(el('h2','Wejścia i wyjścia HeadRush'),el('p','Domyślnie: wejścia 1–2 i wyjścia 1–2. Return to opcjonalny powrót zewnętrznego efektu dla Attack Decay.','hint'));main.append(routing);
 api('hardware').then(h=>{if(tab!=='Ustawienia')return;const a=h.audio_ports||{},grid=el('div',undefined,'common');['Wejście L','Wejście R','Wyjście L','Wyjście R','Return L','Return R'].forEach((name,index)=>{const ports=index<2||index>3?a.inputs||[]:a.outputs||[];grid.append(selectControl(name,[['',index>3?'Niepodłączony':'Domyślne'],...ports.map(p=>[p,p.replace('system:capture_','Wejście ').replace('system:playback_','Wyjście ')])],a.routing?.[index]||'',async v=>{try{await api('hardware',{operation:'audio-route',index,name:v})}catch(e){error(e)}}))});routing.append(grid)}).catch(error);
 const output=el('section',undefined,'box');output.append(el('h2','Poziom i ochrona wyjścia'));const grid=el('div',undefined,'common');for(const key of ['master.output','master.limiter']){const p=parameters.find(p=>p.key===key);if(p)grid.append(control(p))}output.append(grid,el('p','PRZESTER liczy przekroczenia przed zabezpieczeniem. Gdy rośnie, zmniejsz poziom efektu lub wyjścia.','hint'));main.append(output);
 const midi=el('section',undefined,'box'),midiResult=el('p',undefined,'notice');midiResult.id='midi-reset-result';midi.append(el('h2','Mapowanie MIDI'),el('p','Graficzna mapa Hotone Ampero Control z pedałem ekspresji i WORLDE EASYCONTROL.9. Przypisania CC zapisują się w presetach.','hint'),button('Otwórz mapę kontrolerów',openMidiMap,'primary'),el('p','Kasowanie usuwa własne mapy CC, aktywną ekspresję i powiązania slotów PC ze wszystkich presetów na Pi. Sparowanie Bluetooth i ustawienia wejść USB zostają.','hint'),button('Wyczyść wszystkie przypisania MIDI',resetAllMidi,'danger'),midiResult);main.append(midi);
 const diag=el('section',undefined,'box');diag.append(el('h2','Diagnostyka'),button('Pobierz raport',async()=>{const h=await api('hardware');const data={audio:snapshot,hardware:h};if(data.hardware.bluetooth?.pairing)delete data.hardware.bluetooth.pairing;const a=el('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));a.download='JerzyMultiFX-diagnostyka.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}),el('p','Raport nie zawiera klucza dostępu. Zawiera nazwy i adresy urządzeń — sprawdź go przed udostępnieniem.','hint'));main.append(diag)}
async function hardwareAction(data,b){if(b)b.disabled=true;try{await api('hardware',data);hardwareSnapshot=await api('hardware');await renderHardware()}finally{if(b)b.disabled=false}}
async function renderHardware(){if(tab!=='MIDI i Bluetooth')return;const main=$('main');const h=await api('hardware');if(tab!=='MIDI i Bluetooth')return;hardwareSnapshot=h;main.replaceChildren();const bt=h.bluetooth||{};let box=el('section',undefined,'box');box.append(el('h2','Kontroler Bluetooth MIDI'),el('p','Tutaj łączysz pedał MIDI, np. Hotone Ampero Control. To połączenie steruje efektami; nie przesyła dźwięku.','hint'));if(!h.available)box.append(el('p',h.error,'notice'));
 let scan=button(bt.scanning?'Wyszukiwanie…':'Wyszukaj urządzenia',()=>hardwareAction({operation:'scan'},scan),'primary');scan.disabled=!h.available||bt.scanning;box.append(scan);
 for(const d of bt.devices||[]){let card=el('div',undefined,'device'),info=el('div');info.append(el('b',d.name||d.address),el('p',d.connected?'Połączone':d.paired?'Sparowane':'Niepołączone','hint'));let buttons=row();for(const [text,op] of [['Sparuj','pair'],['Połącz MIDI','midi-connect']]){let b=button(text,()=>hardwareAction({operation:op,address:d.address},b));b.disabled=!h.available||op==='pair'&&d.paired;buttons.append(b)}card.append(info,buttons);box.append(card)}
 if(bt.midi?.connected){box.append(el('p','MIDI połączone: '+(bt.midi.name||bt.midi.device),'notice'),button('Rozłącz MIDI',()=>hardwareAction({operation:'midi-disconnect'})))}
 if(bt.pairing){const p=bt.pairing;box.append(el('h3','Potwierdź parowanie'));if(p.kind==='confirm')box.append(el('p','Porównaj kod: '+String(p.code).padStart(6,'0')),row(button('Kod się zgadza',()=>hardwareAction({operation:'confirm',id:p.id,value:true})),button('Odrzuć',()=>hardwareAction({operation:'confirm',id:p.id,value:false}))));else if(['pin','passkey'].includes(p.kind)){const input=el('input');input.placeholder='Kod z urządzenia';box.append(input,button('Potwierdź kod',()=>hardwareAction({operation:'confirm',id:p.id,value:p.kind==='pin'?input.value:Number(input.value)})))}else box.append(el('p','Wpisz na urządzeniu: '+p.code))}
 if(h.relay){box.append(el('p','Odebrane MIDI: '+(h.relay.received||0)+' · pominięte: '+(h.relay.dropped||0),'hint'));if(h.relay.last_event)box.append(el('p','Ostatni komunikat: '+h.relay.last_event.bytes.join(' '),'hint'))}
 if(h.last_error||bt.error)box.append(el('p',h.last_error||bt.error,'notice'));for(const [name,op] of Object.entries(h.operations||{}))if(op.pending||op.error)box.append(el('p',name+': '+(op.pending?'łączenie…':op.error),'notice'));main.append(box);
 box=el('section',undefined,'box');box.append(el('h2','USB MIDI — wejścia do MultiFX'),el('p','Podłącz kontroler USB do Raspberry Pi. Włączone wejścia sterują MultiFX. Most USB MIDI jest uruchamiany przez instalator.','hint'));if(!h.midi_inputs?.length)box.append(el('p','Brak wykrytych wejść MIDI. Sprawdź kabel i zasilanie kontrolera.'));
 for(const port of (h.midi_inputs||[]).filter(p=>!p.name.includes('Bluetooth'))){let r=el('div',undefined,'device');r.append(el('span',port.name),button(port.connected?'POŁĄCZONO · ODŁĄCZ':'POŁĄCZ',()=>hardwareAction({operation:'midi-input',name:port.name,enable:!port.connected}),port.connected?'active':''));box.append(r)}box.append(el('p','Przypisanie regulatora: otwórz efekt, kliknij MIDI Learn pod gałką i porusz regulatorem kontrolera.','hint'));let midiLive=el('p',undefined,'notice');midiLive.id='midi-live';box.append(midiLive);main.append(box);updateMidiLive();
 box=el('section',undefined,'box');box.append(el('h2','Panel na telefonie'),el('p','Najprościej: telefon w tej samej sieci Wi-Fi → http://bassambient.local:8097. Zwykłe sparowanie Bluetooth nie otworzy panelu w Safari.','hint'));
 if(bt.pan?.connected)box.append(el('p','Połączenie z telefonem: '+bt.pan.url),button('Rozłącz telefon',()=>hardwareAction({operation:'pan-disconnect'})));else{box.append(selectControl('iPhone z włączonym Hotspotem ',[['','Wybierz urządzenie'],...(bt.devices||[]).map(d=>[d.address,d.name||d.address])],selectedPan,v=>selectedPan=v),button('Połącz panel przez Bluetooth PAN',()=>hardwareAction({operation:'pan-connect',address:selectedPan})),el('p','Na iPhone włącz Hotspot osobisty i Bluetooth. Ta opcja korzysta z sieci udostępnionej przez telefon; obecne Wi-Fi Raspberry Pi zostaje zachowane.','hint'))}main.append(box);}
function renderPresets(){const main=$('main'),box=el('section',undefined,'box');box.append(el('h2','Presety'));
 const imported=el('label','Wczytaj brzmienie z VST (.jmfx)'),input=el('input');input.type='file';input.accept='.jmfx';input.onchange=async()=>{if(!input.files[0])return;try{await api('preset',input.files[0],true);await poll();await render();$('error').textContent='Wczytano kompletne brzmienie z VST wraz z IR i mapami MIDI.'}catch(e){error(e)}};imported.append(input);box.append(imported);
let factory=parameters.find(p=>p.key==='factory.program');if(factory)box.append(control(factory,true));const program=el('input');program.type='number';program.min=0;program.max=127;program.step=1;program.value=0;program.setAttribute('aria-label','Numer programu MIDI PC');box.append(row(el('span','PROGRAM CHANGE 0–127'),program,button('Zapisz w slocie PC',async()=>{const n=Number(program.value);if(!Number.isInteger(n)||n<0||n>127)throw Error('Numer PC musi być od 0 do 127.');await api('save',{name:'program'+n});await render()}),button('Wczytaj slot PC',async()=>{const n=Number(program.value);if(!Number.isInteger(n)||n<0||n>127)throw Error('Numer PC musi być od 0 do 127.');await api('load',{name:'program'+n});await poll();await render()})),el('p','Każdy slot PC zapisuje cały preset MultiFX wraz z własnym mapowaniem MIDI. PC z kontrolera wczytuje ten sam slot. Sloty 0–7 bez zapisu wybierają presety fabryczne.','hint'));const name=el('input');name.id='preset';name.placeholder='Nazwa, np. moj-bas';name.value='moj-preset';box.append(row(name,button('Zapisz moje brzmienie',async()=>{await api('save',{name:name.value});render()},'primary'),button('Wczytaj',async()=>{await api('load',{name:name.value});await poll();render()})));api('presets').then(data=>{if(tab!=='Presety')return;let chosen=data.presets[0]||'';const list=selectControl('Moje zapisane brzmienia ',[['','Wybierz preset'],...data.presets.map(n=>[n,n])],'',v=>{chosen=v;name.value=v});box.append(row(list,button('Wczytaj wybrany',async()=>{if(!chosen)throw Error('Wybierz preset z listy.');await api('load',{name:chosen});await poll();render()})))}).catch(error);box.append(el('p','Zapis obejmuje cały łańcuch, ustawienia efektów, mapowanie MIDI i wczytany IR.','hint'));main.append(box)}
function updateMidiLive(){const x=$('midi-live');if(!x)return;const count=snapshot.midiEvents||0;if(!count){x.textContent='Do rdzenia nie dotarł jeszcze żaden komunikat MIDI.';return}const raw=snapshot.lastMidi||0,kind=(raw>>16)&240,cc=(raw>>8)&127,value=raw&127;if(kind===176){const map=find('midi.cc.'+cc),target=map===undefined?-1:Math.round((parameters[map].min+(parameters[map].max-parameters[map].min)*(values[map]??0)));x.textContent='Odebrano: CC '+cc+' · wartość '+value+' · '+(target>=0&&parameters[target]?'steruje '+name(parameters[target]):'brak przypisania MIDI Learn')+' · razem '+count;return}x.textContent='Odebrano komunikat MIDI '+((raw>>16)&255)+' · razem '+count}
function paintLevelBar(id,amplitude){const bar=$(id);if(!bar)return;const db=20*Math.log10(Math.max(1e-6,amplitude));bar.style.width=Math.max(0,Math.min(100,(db+60)*100/60))+'%';bar.classList.toggle('hot',db>=-3)}
async function poll(){if(!connected||polling)return;polling=true;const revision=editRevision,editing=sending;try{const s=await api('state');if(!connected)return;if(connectionFailed){$('error').textContent='';connectionFailed=false;}snapshot=s;if(!editing&&!sending&&revision===editRevision)values=s.values;updateMidiLive();updateMidiMapLive();await consumeMidiCapture(s);if(midiMapOpen&&!midiCapture&&controllerMapSignature()!==lastMidiMapSignature)renderMidiMap();$('meter-in').textContent=(20*Math.log10(Math.max(1e-6,s.in))).toFixed(1)+' dB';$('meter-out').textContent=(20*Math.log10(Math.max(1e-6,s.out))).toFixed(1)+' dB';paintLevelBar('meter-in-bar',s.in);paintLevelBar('meter-out-bar',s.out);$('meter-cpu').textContent=s.cpu.toFixed(1)+' %';$('meter-latency').textContent=((s.latencySamples||0)*1000/s.sampleRate).toFixed(2)+' ms';$('meter-xruns').textContent=s.xruns;$('meter-clips').textContent=s.clips||0;$('meter-clips').className=s.clips?'warning':'';$('status').textContent=s.delaysReady===false?'Przygotowywanie efektu…':'● Połączono';if(lastSignature&&signature()!==lastSignature&&document.activeElement?.type!=='range')await renderPanel();updateControls()}catch(e){if(e.status===401){connected=false;setTimeout(connectPanel,1000);return}connectionFailed=true;$('status').textContent='Brak połączenia z audio';error(e)}finally{polling=false}}
async function connectPanel(){if(connected)return;try{await fetch('/',{cache:'no-store'});const meta=await api('metadata');parameters=meta.parameters;window.MF_META=meta;connected=true;await poll();if(!connected)return;$('error').textContent='';await render()}catch(e){connected=false;$('status').textContent='Czekam na uruchomienie MultiFX…';error(e);setTimeout(connectPanel,1500)}}
$('master-learn').onclick=()=>set(find('midi.learn'),(find('master.bypass')+1)/window.MF_META.controlCount).catch(error);
$('master').onclick=()=>set(find('master.bypass'),values[find('master.bypass')]>=.5?0:1).then(updateControls).catch(error);
setInterval(poll,100);setInterval(()=>{if(connected&&tab==='MIDI i Bluetooth'&&!document.querySelector('select:focus,input:focus'))renderHardware().catch(error)},3000);render();connectPanel();
